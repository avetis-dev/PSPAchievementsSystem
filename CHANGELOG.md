# Changelog

# v2.0.0 — Large Sets and Vice City Stories

- Added verified support for Grand Theft Auto: Vice City Stories Europe (`ULES-00502`).
- Raised the runtime capacity to 192 achievements, 512 groups, and 12,288 conditions.
- Moved the large evaluator storage out of the PRX static image so ARK-4 can load the plugin reliably.
- Improved Delta/Prior transition handling for large adaptively scheduled sets.
- Expanded badge packs to 192 images.
- Added experimental package support for Metal Gear Solid: Peace Walker USA (`ULUS-10509`).
- Kept existing v1.0.0 game packages and profiles compatible.

## v1.0.0 — First PSPAchievementsNG Release

Version 1.0.0 is a complete replacement for the legacy runtime previously published in this repository.

### Runtime

- Multi-game package loading by exact PSP Game ID.
- Full achievement evaluation with core and alternate groups.
- Delta, Prior, hit counts, pause/reset logic, pointer chains, measured progress, source modifiers, and bit-level memory reads.
- Memory-read cache and adaptive scheduling for heavy packages.
- Safe GAME-mode lifecycle and exit handling.

### User interface

- Achievement unlock notification with badge artwork.
- Startup notification.
- Synthesized startup and unlock sounds.
- In-game achievement browser.
- `ALL`, `LOCKED`, and `UNLOCKED` filters.
- Live progress where it can be represented exactly.
- Stable full-screen rendering with game-thread suspension while the menu is open.

### Profiles and configuration

- Per-game persistent profiles.
- Checksum validation.
- Temporary-file writes and `.bak` recovery.
- Configurable notifications, audio, menu hotkey, logging, and performance mode.

### Tested games

- Silent Hill: Origins USA — `ULUS-10285` — 66 achievements.
- Dante's Inferno USA — `ULUS-10469` — 63 achievements.
- GTA: Liberty City Stories Europe v3.00 — `ULES-00151` — 105 achievements.

### Tested hardware

- PSP-3000 with ARK-4 and Memory Stick storage exposed as `ms0:`.

### Known limitations

- Offline-only runtime.
- Exact game regions and revisions are required.
- PSP Go internal `ef0:` storage is unsupported.
- GTA can retain a small performance cost even with adaptive scheduling.
