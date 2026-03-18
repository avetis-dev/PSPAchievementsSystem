# 🏆 PSP Achievements System

[![Release](https://img.shields.io/github/v/release/user/PSPAchievementsSystem?style=for-the-badge&logo=github&color=blue)](https://github.com/user/PSPAchievementsSystem/releases)


> **Bring PlayStation-style achievements to your PSP!**  
> A lightweight plugin that displays achievement notifications using RetroAchievements data.

---

---

> ⚠️ **HELP WANTED: TESTERS & DEVELOPERS NEEDED!** ⚠️
> I am actively looking for community help to improve this plugin! 
> Unfortunately, **not all achievements are currently working** as intended. I need volunteers to play the supported games, test the plugin, and report exactly which achievements trigger correctly and which ones do not. 
> 
> If you have experience with C/C++, PSP memory editing, or just want to help by testing games, please head over to the [Issues](https://github.com/user/PSPAchievementsSystem/issues) tab and share your feedback! Your help is highly appreciated! 🙌


> 🛠️ **Upcoming Update:** In the near future, I will be creating a separate repository containing all the development files, tools, and resources to make it easier for anyone who wants to contribute to the project. Stay tuned!
---

## 📖 Table of Contents

- [Overview](#-overview)
- [Features](#-features)
- [Quick Start](#-quick-start)
- [Installation](#-installation)
- [Supported Games](#-supported-games)
- [File Structure](#-file-structure)
- [Roadmap](#-roadmap)
- [Credits](#-credits)

---

## 🎯 Overview

**PSP Achievements System** is a homebrew plugin for PlayStation Portable that integrates with [RetroAchievements](https://retroachievements.org/) to bring you a console-style achievement tracking experience.

### What is it?

This plugin monitors your game progress in real-time and displays beautiful popup notifications when you unlock achievements — just like on modern PlayStation consoles!

---

## ✨ Features

| Feature | Description |
|---------|-------------|
| 🎮 **Real-time Tracking** | Monitors game memory for achievement conditions |
| 🔔 **Popup Notifications** | PlayStation-style achievement unlock notifications |
| 💾 **Persistent Progress** | Your achievements are saved across sessions |
| 📦 **Lightweight** | Only 21 KB plugin size |
| 🔧 **Easy Installation** | Simple file copy setup |
| 🎯 **Test Mode** | L+R button combo for testing notifications |

---

## 🚀 Quick Start

```bash
# 1. Download the latest release
wget https://github.com/user/PSPAchievementsSystem/releases/latest/download/PSP_Achievements_System.zip

# 2. Extract to your PSP memory stick
unzip PSP_Achievements_System.zip -d /mnt/ms0/

# 3. Enable the plugin
echo "ms0:/PSP/SEPLUGINS/PspAchievementsRuntime.prx"

# 4. Restart your PSP and play!
```

---

## 📥 Installation

### Prerequisites

- ✅ PSP with Custom Firmware
- ✅ Memory Stick / SD Card with free space
- ✅ File manager (FreeMP, File Manager, or USB connection)

### Step-by-Step Guide

#### 1️⃣ Create Directory Structure

```
ms0:/PSP/ACH/
ms0:/PSP/ACH/games/
ms0:/PSP/ACH/profiles/
ms0:/PSP/SEPLUGINS/
```

#### 2️⃣ Copy Files from `release/`

| File | Destination |
|------|-------------|
| `PspAchievementsRuntime.prx` | `ms0:/PSP/SEPLUGINS/` |
| `game_map.dat` | `ms0:/PSP/ACH/` |
| `*.ach` files | `ms0:/PSP/ACH/games/` |

#### 4️⃣ Activate Plugin

#### 5️⃣ Restart PSP

⚠️ **Full restart required** (sleep mode won't work!)

---

## 🎮 Supported Games

### Currently Supported

| Game | Region | Code | Achievements | Status |
|------|--------|------|--------------|--------|
| Silent Hill: Origins | USA | ULUS10285 | ~66~ | ✅ Working |

### Planned Support

| Game | Region | Code | Status |
|------|--------|------|--------|
| Silent Hill: Origins | Europe | ULES11337 | 🔜 Coming Soon |
| Silent Hill: Origins | Japan | ULJS00202 | 🔜 Coming Soon |

---

## 📁 File Structure

```
PSPAchievementsSystem/
├── 📦 release/                # Ready-to-use files
│   └── PSP/
│       ├── ACH/
│       │   ├── games/        # .ach achievement files
│       │   └── game_map.dat  # Game mapping database
│       └── COMMON/
│           └── PspAchievementsRuntime.prx  # Main plugin

## ⚙️ Technical Details

### System Requirements

| Component | Requirement |
|-----------|-------------|
| Platform | PSP (Firmware ARK-4)
| Storage | 100 KB free space |
| Dependencies | PSPSDK, GCC for PSP |

### File Formats

#### .ach Binary Structure

```

## 🗺️ Roadmap

### ✅ Completed (v1.0.0)

- [x] Core plugin architecture
- [x] Binary .ach file format
- [x] Popup notification system
- [x] Game detection module
- [x] Profile management
- [x] Silent Hill: Origins support

### 🔄 In Progress

- [ ] Real PSP memory address discovery
- [ ] European version support

### 📋 Planned

- [ ] Achievement viewer application
- [ ] File-based logging
- [ ] Timer condition support
- [ ] Delta condition detection
- [ ] Trigger/reset/pause logic
- [ ] Multi-game database

---

## 🙏 Credits

### Special Thanks

- **[RetroAchievements](https://retroachievements.org/)** - Achievement data and definitions
- **[PPSSPP Team](https://www.ppsspp.org/)** - Emulator and memory dump tools
- **[Universus](https://retroachievements.org/user/Universus)** - Achievement set creator
- **[PSPSDK Contributors](https://github.com/pspdev/pspsdk)** - PSP development framework

### Tools Used

| Tool | Purpose |
|------|---------|
| PPSSPP | Memory dumping & testing |
| PSPSDK | Plugin compilation |
| rcheevos | Achievement logic reference |
| Python 3 | Data conversion scripts |

---

## 📄 License

This project is provided for **educational purposes only**.

- 📚 Free to use for personal projects
- 🔧 Free to modify for learning
- ❌ Not for commercial use
- ⚠️ Use at your own risk

See [LICENSE](LICENSE) for full terms.

---

<div align="center">

**Made with ❤️ for the PSP Homebrew Community**

[Report Bug](https://github.com/user/PSPAchievementsSystem/issues) · [Request Feature](https://github.com/user/PSPAchievementsSystem/issues) · [Discussions](https://github.com/user/PSPAchievementsSystem/discussions)

</div>
