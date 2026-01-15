# Teensy2DMD - Teensy 4.1 & SmartLED Shield V5 Edition

This is a hardware-specific port of the original [Teensy2DMD by gi1mic](https://github.com/gi1mic/teensy2dmd). It has been updated to work with the **Teensy 4.1** and the **SmartLED Shield V5**.

## 🚀 Improvements & Changes
* **Hardware**: Optimized for Teensy 4.1 and SmartMatrix Shield V5 (HUB75).
* **Boot Screen**: Displays a green "*** RETROPIE ARCADE SYSTEM READY ***" message on startup.
* **Commands**: Added `ON` and `OFF` serial commands to control the display power.
* **Help**: Updated the internal `HELP` command with ZMODEM instructions.

## 💾 Easy Install (HEX File)
If you don't want to compile the code yourself, I have included a `.hex` file.
1. Download the `.hex` file from this repository.
2. Use the **Teensy Loader** application to flash it directly to your Teensy 4.1.

## ⚙️ Requirements for Developers
If you want to modify the code, make sure you use:
* **Arduino IDE**: 1.8.13 or higher.
* **Teensyduino Board Manager**: **Version 1.53** or higher.
* **Libraries**: SmartMatrix 4.0.3 and SdFat 2.3.0.

## 📂 How to upload GIFs
1. Connect your Teensy to your PC.
2. Open a terminal (like Tera Term) at 57600 baud.
3. Type `RZ` and use **ZMODEM** to send your `.gif` files to the SD card.

---
*Credits to the original creator gi1mic for the base code.*
