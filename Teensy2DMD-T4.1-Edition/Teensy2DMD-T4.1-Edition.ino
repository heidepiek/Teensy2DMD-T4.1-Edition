#include "Arduino.h"
#include "info.h"
#include "zmodem_config.h"
#include "zmodem.h"
#include "zmodem_zm.h"

// Hardware instelling voor Teensy 4 Shield V5
#include <MatrixHardware_Teensy4_ShieldV5.h> 
#include <SmartMatrix.h>
#include <SdFat.h>  
#include "GifDecoder.h"
#include "FilenameFunctions.h"

SdFs sd;

#define DISPLAY_TIME_SECONDS 10
#define error(s) sd.errorHalt(s)
#define DSERIALprintln(_p) ({ DSERIAL.println(_p); }) 
#define fname (&oneKbuf[512])

extern int Filesleft;
extern long Totalleft;

FsFile fout;

int num_files;
int defaultBrightness = 30; 

rgb24 tColour = {0x00, 0xff, 0x00};
ScrollMode  tMode = wrapForward;   
int tSpeed = 10; // txt speed
fontChoices  tFont = font3x5; 
char tText[50] = "Undefined text message";
int tLoopCount = 1;

const rgb24 COLOR_BLACK = { 0, 0, 0 };

// --- MATRIX CONFIGURATIE ---
#define COLOR_DEPTH 24               
const uint16_t kMatrixWidth = 128;        
const uint16_t kMatrixHeight = 32;       
const uint8_t kRefreshDepth = 36;       
const uint8_t kDmaBufferRows = 4;       
const uint8_t kPanelType = SMARTMATRIX_HUB75_32ROW_MOD16SCAN; 
const uint32_t kMatrixOptions = (SMARTMATRIX_OPTIONS_NONE);

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);
SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, SM_BACKGROUND_OPTIONS_NONE);
SMARTMATRIX_ALLOCATE_SCROLLING_LAYER(scrollingLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, SM_SCROLLING_OPTIONS_NONE);

GifDecoder<kMatrixWidth, kMatrixHeight, 12> decoder;

// --- CALLBACK FUNCTIES ---
void screenClearCallback(void) { 
  backgroundLayer.fillScreen({0, 0, 0}); 
}

void updateScreenCallback(void) { 
  backgroundLayer.swapBuffers(); 
}

void drawPixelCallback(int16_t x, int16_t y, uint8_t red, uint8_t green, uint8_t blue) {
  backgroundLayer.drawPixel(x, y, {red, green, blue});
}

// --- HELPER FUNCTIES ---
size_t DSERIALprint(const __FlashStringHelper *ifsh)
{
  PGM_P p = reinterpret_cast<PGM_P>(ifsh);
  size_t n = 0;
  while (1)
  {
    unsigned char c = pgm_read_byte(p++);
    if (c == 0) break;
    if (DSERIAL.availableForWrite() > SERIAL_TX_BUFFER_SIZE / 2) DSERIAL.flush();
    if (DSERIAL.write(c)) n++;
    else break;
  }
  return n;
}

void help(void)
{
  DSERIALprintln(F("--------------------------------------------------"));
  DSERIALprint(Progname);
  DSERIALprint(F(" - Transfer rate: ")); 
  DSERIAL.println(ZMODEM_SPEED);
  DSERIALprintln(F("--------------------------------------------------"));
  DSERIALprintln(F("Display Commands:"));
  DSERIALprintln(F(" ON, OFF          - Enable/Disable display"));
  DSERIALprintln(F(" BRI [0-255]      - Set brightness level"));
  DSERIALprintln(F(" DIS [file.gif]   - Play specific GIF"));
  DSERIALprintln(F(" RND              - Play random GIFs"));
  DSERIALprintln(F(" TXT [text]       - Scroll custom text"));
  DSERIALprintln(F("--------------------------------------------------"));
  DSERIALprintln(F("File & System Commands:"));
  DSERIALprintln(F(" DIR, LS          - List files on SD card"));
  DSERIALprintln(F(" CD [dir], PWD    - Change/Print directory"));
  DSERIALprintln(F(" MD, RD, DEL      - Make/Remove Dir, Delete file"));
  DSERIALprintln(F(" RZ, SZ           - Receive/Send via ZMODEM"));
  DSERIALprintln(F("--------------------------------------------------"));
  DSERIALprintln(F("HOW TO TRANSFER FILES (PC -> SD):"));
  DSERIALprintln(F(" 1. Type 'RZ' and press Enter."));
  DSERIALprintln(F(" 2. In Tera Term menu: File -> Transfer -> ZMODEM -> Send..."));
  DSERIALprintln(F(" 3. Select your .gif file and click 'Open'."));
  DSERIALprintln(F("--------------------------------------------------"));
  DSERIALprintln(F("Status: System Ready"));
}

// --- SETUP ---
void setup()
{
  ZSERIAL.begin(ZMODEM_SPEED);
  ZSERIAL.setTimeout(TYPICAL_SERIAL_TIMEOUT);
  delay(400);

  if (!sd.begin(SdioConfig(FIFO_SDIO))) sd.initErrorHalt(&DSERIAL);
  if (!sd.chdir("/")) sd.errorHalt(F("sd.chdir"));

  decoder.setScreenClearCallback(screenClearCallback);
  decoder.setUpdateScreenCallback(updateScreenCallback);
  decoder.setDrawPixelCallback(drawPixelCallback);
  decoder.setFileSeekCallback(fileSeekCallback);
  decoder.setFilePositionCallback(filePositionCallback);
  decoder.setFileReadCallback(fileReadCallback);
  decoder.setFileReadBlockCallback(fileReadBlockCallback);

  matrix.addLayer(&backgroundLayer);
  matrix.addLayer(&scrollingLayer);
  
  matrix.setBrightness(defaultBrightness);
  matrix.setRefreshRate(60);
  matrix.begin();

  backgroundLayer.fillScreen(COLOR_BLACK);
  backgroundLayer.swapBuffers(false);

  // --- RETROPIE WELCOME TEXT ---
  scrollingLayer.setColor({0x00, 0xff, 0x00}); // Retro groen
  scrollingLayer.setMode(wrapForward);
  scrollingLayer.setSpeed(35); 
  scrollingLayer.setFont(font3x5); // Dit font gebruikt jouw code al, dus dit MOET werken
  scrollingLayer.start("*** RETROPIE ARCADE SYSTEM READY ***", 1); 
  
  delay(8500);

  // Toon help in Serial Monitor
  help();

  // --- AUTO-START GIF ---
  if (openGifFilenameByFilename("/mariobros.gif") >= 0) {
      backgroundLayer.fillScreen(COLOR_BLACK);
      backgroundLayer.swapBuffers();
      decoder.startDecoding();
      DSERIALprintln(F("Arcade Marquee: mariobros.gif started!"));
  }
}

// --- LOOP ---
void loop(void)
{
  char *cmd = oneKbuf;
  char *param;
  static unsigned long futureTime;
  char curPath[40];

  *cmd = 0;
  while (DSERIAL.available()) DSERIAL.read();

  char c = 0;
  while (1)
  {
    if (DSERIAL.available() > 0)
    {
      c = DSERIAL.read();
      if ((c == 8 || c == 127) && strlen(cmd) > 0) cmd[strlen(cmd) - 1] = 0;
      if (c == '\n' || c == '\r') break;
      DSERIAL.write(c);
      if (c != 8 && c != 127) strncat(cmd, &c, 1);
    }
    else { delay(20); }
    decoder.decodeFrame();
  }

  param = strchr(cmd, 32);
  if (param > 0) { *param = 0; param = param + 1; }
  else { param = &cmd[strlen(cmd)]; }

  strupr(cmd);
  DSERIAL.println();

  if (!strcmp_P(cmd, PSTR("HELP"))) { help(); }
  else if (!strcmp_P(cmd, PSTR("DIR")) || !strcmp_P(cmd, PSTR("LS")))
  {
    char filename[40];
    FsFile cwd = sd.open(".");
    cwd.getName(fname, 13);
    cwd.close();
    snprintf(curPath, sizeof(curPath), "/%s/", fname);
    DSERIALprint(F("Directory Listing for: ")); DSERIALprintln(curPath);
    FsFile directory = sd.open(curPath);
    FsFile file;
    while (file.openNext(&directory)) {
      file.getName(filename, sizeof(filename));
      DSERIALprintln(filename);
      file.close();
    }
    directory.close();
  }
  else if (!strcmp_P(cmd, PSTR("BRI"))) {
    defaultBrightness = atoi(param);
    matrix.setBrightness(defaultBrightness);
    DSERIALprint(F("Brightness set to: ")); DSERIAL.println(defaultBrightness);
  }
  else if (!strcmp_P(cmd, PSTR("OFF"))) {
    matrix.setBrightness(0); 
    backgroundLayer.fillScreen(COLOR_BLACK);
    backgroundLayer.swapBuffers();
    DSERIALprintln(F("Display disabled (LEDs OFF)"));
  }
  else if (!strcmp_P(cmd, PSTR("ON"))) {
    matrix.setBrightness(defaultBrightness);
    backgroundLayer.swapBuffers();
    DSERIALprint(F("Display enabled. Brightness: ")); DSERIAL.println(defaultBrightness);
  }
  else if (!strcmp_P(cmd, PSTR("PWD"))) {
    FsFile cwd = sd.open(".");
    cwd.getName(fname, 13);
    cwd.close();
    DSERIALprint(F("Current directory: ")); DSERIAL.println(fname);
  }
  else if (!strcmp_P(cmd, PSTR("CD"))) {
    if (!sd.chdir(param)) { DSERIALprintln(F("Directory not found")); }
    else { DSERIALprint(F("Changed to: ")); DSERIAL.println(param); }
  }
  else if (!strcmp_P(cmd, PSTR("DEL"))) {
    if (!sd.remove(param)) { DSERIALprintln(F("Delete failed")); }
    else { DSERIALprint(F("Deleted: ")); DSERIAL.println(param); }
  }
  else if (!strcmp_P(cmd, PSTR("MD"))) {
    if (!sd.mkdir(param)) { DSERIALprintln(F("Create directory failed")); }
    else { DSERIALprint(F("Directory created: ")); DSERIAL.println(param); }
  }
  else if (!strcmp_P(cmd, PSTR("RD"))) {
    if (!sd.rmdir(param)) { DSERIALprintln(F("Remove directory failed")); }
    else { DSERIALprint(F("Directory removed: ")); DSERIAL.println(param); }
  }
  else if (!strcmp_P(cmd, PSTR("DIS"))) {
    char filename[40];
    FsFile cwd = sd.open(".");
    cwd.getName(fname, 13);
    cwd.close();
    snprintf(filename, sizeof(filename), "/%s/%s", fname, param);
    if (openGifFilenameByFilename(filename) >= 0) {
      matrix.setBrightness(defaultBrightness);
      backgroundLayer.fillScreen(COLOR_BLACK);
      backgroundLayer.swapBuffers();
      decoder.startDecoding();
      DSERIALprint(F("Now playing: ")); DSERIAL.println(param);
    } else { DSERIALprintln(F("File not found!")); }
  }
  else if (!strcmp_P(cmd, PSTR("RND")) || !strcmp_P(cmd, PSTR("RANDOM"))) {
    DSERIALprintln(F("Starting Random Mode... Press Enter to stop."));
    num_files = enumerateGIFFiles("/", false);
    if (num_files <= 0) { DSERIALprintln(F("No GIFs found!")); } 
    else {
      randomSeed(micros());
      futureTime = 0; 
      while (1) {
        if (millis() > futureTime) {
          int index = random(num_files);
          if (openGifFilenameByIndex("/", index) >= 0) {
            matrix.setBrightness(defaultBrightness);
            decoder.startDecoding();
            futureTime = millis() + (DISPLAY_TIME_SECONDS * 1000);
          }
        }
        decoder.decodeFrame();
        if (DSERIAL.available() > 0) {
          char rc = DSERIAL.read();
          if (rc == '\n' || rc == '\r') { DSERIALprintln(F("Random mode stopped.")); break; }
        }
      }
    }
  }
  else if (!strcmp_P(cmd, PSTR("TXT"))) {
    matrix.setBrightness(defaultBrightness);
    scrollingLayer.setColor(tColour);
    scrollingLayer.start(param, tLoopCount);
    DSERIALprint(F("Scrolling text: ")); DSERIAL.println(param); 
  }
  else if (!strcmp_P(cmd, PSTR("RZ"))) {
    DSERIALprintln(F("Waiting for ZMODEM..."));
    if (wcreceive(0, 0)) { DSERIALprintln(F("Transfer failed!")); }
    else { DSERIALprintln(F("Transfer success!")); }
  }
  else if (!strcmp_P(cmd, PSTR("SZ"))) {
    if (!fout.open(param, O_READ)) { DSERIALprintln(F("File open failed")); }
    else {
      Filesleft = 1;
      Totalleft = fout.fileSize();
      ZSERIAL.print(F("rz\r"));
      sendzrqinit();
      delay(200);
      wcs(param);
      saybibi();
      fout.close();
    }
  }
  else { DSERIALprintln(F("Unknown command. Type HELP for list.")); }
}