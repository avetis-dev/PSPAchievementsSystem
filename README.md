# PSP Achievements System (ARK-4)

A custom PRX plugin for the Sony PSP that brings the magic of [RetroAchievements](https://retroachievements.org/) to actual, physical PSP hardware. 

This plugin evaluates complex RA logic (including Delta, HitCounts, ResetIf, PauseIf, and Alternate groups) in real-time by reading the game's RAM, completely offline.

## ✨ Features (Alpha)

* **Real-Time Evaluation:** A lightweight, custom-built logic parser runs alongside the game.
* **In-Game Popups:** Custom trophy-like notifications rendered directly to the PSP's framebuffer (supports both 32-bit and 16-bit color formats).
* **Low Memory Footprint:** Optimized to fit within the strict RAM limits of the PSP (runs smoothly alongside heavy games).
* **Game Auto-Detection:** Automatically detects the running game ID (e.g., `ULUS10285`).

## 🎮 Supported Games

Currently used as a proof-of-concept for:
* **Silent Hill: Origins (USA)** - `ULUS10285` (RA Game ID: `3927`)
  * *Status:* Working (Popups trigger accurately on memory state changes).

## 📥 Installation

1. Install the **ARK-4** custom firmware on your PSP.
2. Download the latest release.
3. Copy `PspAchievements.prx` to your `ms0:/seplugins/` folder.
4. Copy the data files to the root of your memory stick:
   ```text
   ms0:/
   ├── seplugins/
   │   └── PspAchievements.prx
   └── PSP/
       └── ACH/
           ├── game_map.dat
           └── games/
               └── 3927.ach
5. Enable the plugin in the ARK-4 Recovery Menu (game, ms0:/seplugins/PspAchievements.prx, on).
6. Launch the game!

## 🛠 Building from Source
You need the PSPSDK installed.
~~~
cd plugin
make clean
make
~~~

## Converting RA Data
To add new games, use the provided Python script to convert a RetroAchievements JSONL dump into the optimized .ach binary format used by the plugin:

~~~
python3 tools/converter/dump_to_ach.py tools/converter/ppsspp_ra_dump.jsonl ULUS10285 3927 data/games/3927.ach
~~~

## 🗺 Architecture

* **main.c:** Thread management, popup loop, and game detection.
* **rcheevos_glue.c:** The core logic evaluator. Parses RA syntax and tracks delta/prior memory states.
* **memory.c:** Safe RAM access mapped to RA's 0x08000000 structure.
* **popup.c:** Direct framebuffer rendering (double-buffered) without interrupting the game's graphics pipeline.

## 🚀 Roadmap

 - [x] Base RA logic parsing
 - [x] Framebuffer popup rendering
 - [x] Fix memory leaks & RAM limits
 - [ ] Save/Load User Profile progress (Next update!)


## 🙏 Credits

### Special Thanks

- **[RetroAchievements](https://retroachievements.org/)** - Achievement data and definitions
- **[PPSSPP Team](https://www.ppsspp.org/)** - Emulator and memory dump tools
- **[Universus](https://retroachievements.org/user/Universus)** - Achievement set creator
- **[PSPSDK Contributors](https://github.com/pspdev/pspsdk)** - PSP development framework

## 📄 License

This project is provided for **educational purposes only**.

- 📚 Free to use for personal projects
- 🔧 Free to modify for learning
- ❌ Not for commercial use
- ⚠️ Use at your own risk
