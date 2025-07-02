# LOFZ [Lights Out Flipper Zero] - Setup & Installation Guide

Welcome, Jedi!  
This file will guide you step-by-step through building and loading the LOFZ game onto your Flipper Zero device.

---

## Table of Contents

- [Requirements](#requirements)
- [1. Setting Up the Flipper Zero Firmware SDK](#1-setting-up-the-flipper-zero-firmware-sdk)
- [2. Adding the LOFZ Source Code](#2-adding-the-lofz-source-code)
- [3. Building the Game](#3-building-the-game)
- [4. Loading LOFZ to Your Flipper Zero](#4-loading-lofz-to-your-flipper-zero)
- [5. Playing the Game](#5-playing-the-game)
- [6. Troubleshooting](#6-troubleshooting)
- [7. Credits](#7-credits)

---

## Requirements

- **Flipper Zero device**
- **Computer** (Windows, macOS, or Linux)
- **Flipper Zero Firmware SDK** (for building custom applications)
- **qFlipper** (for flashing and managing files, highly recommended)
- **LOFZ Source Files** (`lofz_flipperzero.c`, `README.md`, `SETUP.md`)

---

## 1. Setting Up the Flipper Zero Firmware SDK

1. **Clone the Official Firmware Repository:**

   ```sh
   git clone https://github.com/flipperdevices/flipperzero-firmware.git
   cd flipperzero-firmware
   ```

2. **Install SDK Dependencies:**
    - Follow the [official Flipper Zero SDK setup instructions](https://github.com/flipperdevices/flipperzero-firmware/blob/dev/README.md#quick-start-for-developers)
    - On Linux/macOS, you can usually run:
      ```sh
      ./fbt
      ```
    - On Windows, use WSL or follow Windows instructions in the Flipper firmware docs.

---

## 2. Adding the LOFZ Source Code

1. **Navigate to the Applications Directory:**

   ```sh
   cd applications_user
   ```

2. **Create a New Directory for LOFZ:**

   ```sh
   mkdir lofz
   cd lofz
   ```

3. **Copy the Game Source Files:**
    - Place `lofz_flipperzero.c` (the game file) in this directory.
    - (Optional but recommended) Add `README.md` and `SETUP.md` for your repo/docs.

4. **Create a `CMakeLists.txt` file:**

   Paste the following content in `CMakeLists.txt`:

   ```
   add_application(lofz "lofz_flipperzero.c")
   ```

---

## 3. Building the Game

1. **Return to the root of the firmware repo:**

   ```sh
   cd ../..
   ```

2. **Build the User Application:**

   ```sh
   ./fbt fap_lofz
   ```

3. **The resulting `.fap` file will be in:**
   ```
   build/f7-firmware-D/.extapps/lofz.fap
   ```

---

## 4. Loading LOFZ to Your Flipper Zero

1. **Connect your Flipper Zero to your computer via USB.**
2. **Open [qFlipper](https://flipperzero.one/en/qflipper)** (or mount the Flipper as a USB drive).
3. **Copy the `lofz.fap` file to:**
    ```
    /apps/
    ```
    or the appropriate `apps` directory for your Flipper.

4. **Safely eject or disconnect the Flipper Zero.**

---

## 5. Playing the Game

1. **On your Flipper Zero:**
    - Go to **Applications** → **LOFZ** (or "Lights Out Flipper Zero").
2. **Controls:**
    - **D-pad:** Move cursor
    - **OK:** Select/make a move, advance messages, or select menu option
    - **Up/Down:** In menus, change selection
    - **Easter egg:** On top row, far right box, tap "right" 3 times quickly for credits!

3. **Enjoy your Star Wars-themed duel against the Flipper Zero Sith!**

---

## 6. Troubleshooting

- **If the game doesn't show up:**  
  Make sure the `.fap` file is copied to the right folder (`/apps/`) and you rebuilt the firmware after editing.
- **Build errors:**  
  Ensure your SDK is up to date and there are no typos in your `CMakeLists.txt`.
- **Game glitches:**  
  Rebuild and reflash. If issues persist, reach out with the error log.

---

## 7. Credits

**Original Game & BSDM Model:** DigiMancer3D  
**Game Design, Star Wars Theme, and Flipper Zero Port:** DigiMancer3D & GitHub Copilot AI  
**Flipper Zero Mascot:** Flipper Devices Team  
**AI Code Assistance:** [GitHub Copilot AI](https://github.com/github/copilot)

---

May the Force be with you!