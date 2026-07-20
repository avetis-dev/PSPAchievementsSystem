# PSP Achievements System v2.0.0

This release replaces the legacy PSP achievement runtime with **PSPAchievementsNG**, a new multi-game GAME-mode plugin tested on real PSP hardware.

> **Important:** this release contains no RetroAchievements trigger definitions or prebuilt `.pach` packages. RetroAchievements does not permit redistribution of its achievement condition logic. We are pursuing an official `rcheevos`/`rc_client` integration; until it is completed, this public build cannot activate RetroAchievements sets by itself.

Achievement metadata and badge artwork are provided by RetroAchievements. This project is independent and is not affiliated with or endorsed by RetroAchievements.

## Highlights

- Offline achievement evaluation on physical PSP hardware
- Automatic game detection
- Per-game profiles with checksum and backup recovery
- Badge artwork and unlock notifications
- Startup and achievement sounds
- In-game achievement browser
- `ALL / LOCKED / UNLOCKED` filters
- Live measured progress
- Configurable hotkey, audio, notifications, logging, and performance mode
- Adaptive scheduling for large achievement sets

## Tested games

| Game | Region | Game ID | Achievements |
|---|---|---:|---:|
| Silent Hill: Origins | USA | `ULUS-10285` | 66 |
| Dante's Inferno | USA | `ULUS-10469` | 63 |
| GTA: Liberty City Stories v3.00 | Europe | `ULES-00151` | 105 |
| GTA: Vice City Stories v1.02 | Europe | `ULES-00502` | 190 |
| Metal Gear Solid: Peace Walker | USA | `ULUS-10509` | 110 (experimental) |

## Tested hardware

- PSP-3000
- ARK-4
- Memory Stick installation using `ms0:`

Other PSP models may work but are not yet officially verified.

## Important migration note

Do not run this release together with the old `PspAchievements.prx` plugin. Version 2.0.0 keeps the PSPAchievementsNG profile layout introduced in v1.0.0, but greatly expands runtime capacity. Read `INSTALLATION.md` and `LEGACY_MIGRATION.md` before updating from the legacy plugin.

## Installation

Copy the included `SEPLUGINS` directory to the Memory Stick root and add:

```text
ms0:/SEPLUGINS/PSPAchievementsNG/PSPAchievementsNG.prx 1
```

to:

```text
ms0:/SEPLUGINS/GAME.TXT
```

Permitted `.pbad` badge files are included in:

```text
ms0:/SEPLUGINS/PSPAchievementsNG/games/
```

They contain artwork only. No trigger logic is included.

## Limitations

- Offline-only; no accounts, cloud sync, public profiles, or social sharing.
- Packages are specific to the exact Game ID and executable revision.
- PSP Go internal `ef0:` storage is not supported.
- GTA may still have a small performance cost.

This is an independent community project and is not affiliated with Sony, ARK-4, PPSSPP, or RetroAchievements.
