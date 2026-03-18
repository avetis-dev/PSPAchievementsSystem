# Changelog

All notable changes to this project will be documented in this file.

## [1.0.0] - 2024-03-18

### ✨ Added
- Initial beta release
- Support for Silent Hill: Origins (ULUS10285)
- 66 achievements from RetroAchievements
- On-screen popup notifications
- L+R button test combination
- Persistent progress saving
- Binary .ach file format
- Game mapping system

### ⚠️ Known Issues
- Timer-based achievements don't work (e.g., "Let Me Burn")
- Delta condition achievements don't work (e.g., "Enter Otherworld first time")
- Complex multi-group achievements may fail
- Only USA version (ULUS10285) is supported
- Memory addresses from PPSSPP may not match real PSP

### 🔧 Technical Details
- Plugin size: 21 KB
- Achievement data: 43 KB (66 achievements)
- Binary format: FORMAT v2
- Memory addressing: PPSSPP → PSP mapping (approximate)

### 📦 Files Included
- `PspAchievementsRuntime.prx` (21 KB) - Main plugin
- `3927.ach` (43 KB) - Achievement definitions
- `game_map.dat` (56 bytes) - Game mapping
- `active_profile.txt` - Profile activation
- `game.txt` - Plugin activation example
- `INSTALL.txt` - Installation guide

### 🎯 Achievement Breakdown
- Simple conditions: ~45 achievements (some work)
- Timer-based: ~10 achievements (don't work)
- Delta conditions: ~8 achievements (don't work)
- Complex groups: ~3 achievements (may not work)

### 🙏 Credits
- RetroAchievements.org - Achievement data
- PPSSPP Team - Emulator and memory dumps
- Universus - Achievement definitions

---

## [Unreleased]

### 🎯 TODO
- [ ] Find real PSP memory addresses
- [ ] Implement full rcheevos parser
- [ ] Support European version (ULES11337)
- [ ] Support Japanese version (ULJS00202)
- [ ] Add achievement viewer application
- [ ] Add logging to text file
- [ ] Improve delta condition detection
- [ ] Add trigger/reset/pause support

### 🤝 Help Wanted
- C/C++ developers with PSP homebrew experience
- Memory hackers who can find real PSP addresses
- Testers with different PSP models and firmware
- Contributors familiar with rcheevos specification

---

**This is a beta release. Use at your own risk.**
