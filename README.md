# PSP Achievements System

<p align="center">
  <strong>Offline achievements for real PlayStation Portable hardware.</strong>
</p>

<p align="center">
  <a href="https://github.com/avetis-dev/PSPAchievementsSystem/actions/workflows/build.yml"><img alt="Build" src="https://github.com/avetis-dev/PSPAchievementsSystem/actions/workflows/build.yml/badge.svg"></a>
  <a href="https://github.com/avetis-dev/PSPAchievementsSystem/releases"><img alt="Release" src="https://img.shields.io/github/v/release/avetis-dev/PSPAchievementsSystem?display_name=tag"></a>
  <img alt="Platform" src="https://img.shields.io/badge/platform-PSP-2d6cdf">
  <img alt="Runtime" src="https://img.shields.io/badge/runtime-PSPAchievementsNG-4c8f2f">
  <img alt="Version" src="https://img.shields.io/badge/version-2.0.0-brightgreen">
</p>

PSP Achievements System is a GAME-mode kernel plugin for custom-firmware PSP consoles. The current runtime, **PSPAchievementsNG**, evaluates achievement conditions directly from game memory and provides unlock notifications, sound, badge artwork, persistent profiles, live progress, and an in-game achievement browser.

The plugin works locally after installation. Version **2.0.0** does not require an account or a permanent network connection.

> [!IMPORTANT]
> This public repository and its releases do **not** contain RetroAchievements trigger definitions or prebuilt `.pach` packages. RetroAchievements does not permit its achievement condition logic to be reused or redistributed. The project is now pursuing an official `rcheevos`/`rc_client` integration; until that work is complete, the public build alone cannot activate RetroAchievements sets.

Achievement metadata and badge artwork are provided by [RetroAchievements](https://retroachievements.org/). PSP Achievements System is independent and is not affiliated with or endorsed by RetroAchievements.

> [!IMPORTANT]
> PSPAchievementsNG 2.0.0 is compatible with profiles created by PSPAchievementsNG 1.0.0. Users of the older legacy `PspAchievements.prx` / `PSP/ACH` implementation must perform a clean migration.

## Features

- Real-time achievement evaluation on physical PSP hardware.
- Automatic game detection by PSP Game ID.
- Runtime support for separately supplied compatible achievement packages; none are distributed publicly by this project.
- Optional `.pbad` badge packs with locked and unlocked artwork.
- Achievement notifications with a synthesized unlock sound.
- Persistent per-game profiles stored on the Memory Stick.
- Checksum validation, temporary writes, and automatic `.bak` recovery.
- In-game achievement browser with badge artwork and descriptions.
- `ALL`, `LOCKED`, and `UNLOCKED` filters.
- Live measured progress such as `7 / 10` where the achievement logic exposes it safely.
- Configurable menu hotkey, notification duration, audio, logging, and performance mode.
- Memory-read caching and adaptive scheduling for large achievement sets.
- Fully local runtime after the plugin and matching game packages are installed.

## Tested hardware

| Device / environment | Status |
|---|---|
| PSP-3000, ARK-4, Memory Stick path `ms0:` | **Tested and supported** |
| PSP-1000 / PSP-2000 / PSP Street with ARK-4 | Expected to work; community verification needed |
| PSP Go with external M2 storage exposed as `ms0:` | Untested |
| PSP Go internal storage `ef0:` | Not supported in v2.0.0 |
| PRO-C / LME | Untested |
| PS Vita / Adrenaline | Not supported or tested |
| PPSSPP | Not a runtime target; the plugin is intended for real PSP hardware |
| Official firmware without CFW | Not supported |

Only the PSP-3000 + ARK-4 configuration is currently considered officially verified.

## Supported games

Game support is tied to the **exact PSP Game ID and executable revision**. A package for one region cannot be made compatible with another region by renaming it.

| Game | Region / revision | PSP Game ID | Achievements | Points | Evaluator | Status |
|---|---|---:|---:|---:|---|---|
| Silent Hill: Origins | USA | `ULUS-10285` | 66 | 666 | Full-rate | Tested on hardware |
| Dante's Inferno | USA | `ULUS-10469` | 63 | 666 | Full-rate | Tested on hardware |
| Grand Theft Auto: Liberty City Stories | Europe v3.00 | `ULES-00151` | 105 | 1016 | Adaptive | Tested on hardware |
| Grand Theft Auto: Vice City Stories | Europe v1.02 | `ULES-00502` | 190 | 1532 | Adaptive | Tested on hardware |
| Metal Gear Solid: Peace Walker | USA | `ULUS-10509` | 110 | 2100 | Ultra-heavy adaptive | Experimental |

Both GTA packages require the exact European revisions listed above. The USA and Russian `ULUS-10160` release of Vice City Stories is not compatible with the European package. Large sets may still have a small performance cost because they contain substantially more conditions than the original three games.

See [Supported Games](docs/SUPPORTED_GAMES.md) for exact notes.

## Release files

The public v2.0.0 release provides:

```text
PSPAchievementsSystem-v2.0.0.zip
```

The archive contains the plugin, default configuration, documentation, and permitted badge artwork. It intentionally contains no `.pach` trigger packages.

## Quick installation

1. Back up any existing PSP achievement profiles.
2. Remove or disable the legacy `PspAchievements.prx` plugin entry.
3. Extract the v2.0.0 plugin archive.
4. Copy its `SEPLUGINS` directory to the root of the Memory Stick.
5. Add this line to `ms0:/SEPLUGINS/GAME.TXT`:

   ```text
   ms0:/SEPLUGINS/PSPAchievementsNG/PSPAchievementsNG.prx 1
   ```

6. Restart the PSP completely.

The public build will load, but RetroAchievements unlock support requires the planned official `rc_client` integration. Prebuilt trigger packages are not provided.

Read the complete [Installation Guide](docs/INSTALLATION.md), especially when upgrading from a legacy release.

## Memory Stick layout

```text
ms0:/
└── SEPLUGINS/
    ├── GAME.TXT
    └── PSPAchievementsNG/
        ├── PSPAchievementsNG.prx
        ├── config.ini
        ├── games/
        │   ├── ULUS-10285.pach
        │   ├── ULUS-10285.pbad
        │   ├── ULUS-10469.pach
        │   ├── ULUS-10469.pbad
        │   ├── ULES-00151.pach
        │   ├── ULES-00151.pbad
        │   ├── ULES-00502.pach
        │   ├── ULES-00502.pbad
        │   ├── ULUS-10509.pach
        │   └── ULUS-10509.pbad
        ├── profiles/
        │   ├── ULUS-10285.dat
        │   ├── ULUS-10285.bak
        │   └── ...
        └── logs/
            └── plugin.log
```

`profiles/` and `logs/` are created automatically. Do not place `.pach` or `.pbad` files in `profiles/`.

## In-game menu

Hold the default hotkey for approximately 400 ms:

```text
L + R + SELECT
```

| Control | Action |
|---|---|
| Up / Down | Select the previous or next achievement |
| Left / Right | Move one page |
| Triangle | Switch `ALL / LOCKED / UNLOCKED` |
| L / R | Jump to the first or last item in the current filter |
| Circle | Close the menu |

The game is temporarily paused while the menu is open to keep the framebuffer stable. The achievement evaluator is paused at the same time.

## Configuration

The plugin reads this file when a game starts:

```text
ms0:/SEPLUGINS/PSPAchievementsNG/config.ini
```

Recommended configuration:

```ini
[general]
enabled = 1
logging = 1

[notifications]
enabled = 1
startup = 1
duration_ms = 4000
startup_duration_ms = 2500

[audio]
enabled = 1
volume = 100
startup_sound = 1
unlock_sound = 1

[menu]
enabled = 1
hotkey = L+R+SELECT
hold_ms = 400
show_badges = 1

[performance]
mode = auto
```

Keep `performance.mode = auto` unless diagnosing a problem. Forcing `full` can make GTA and Peace Walker difficult to play.

See [Configuration](docs/CONFIGURATION.md).

## Verifying the installation

After launching a supported game, open:

```text
ms0:/SEPLUGINS/PSPAchievementsNG/logs/plugin.log
```

A normal startup contains lines similar to:

```text
PSPAchievementsNG 2.0.0
config loaded
game identification succeeded
game package found
achievement engine initialized
game package loaded
badge pack loaded
startup notification queued
achievement menu ready
```

The log is the first place to check when a package is missing, a region is unsupported, icons do not load, or performance is poor.

## Profiles and backups

Each game receives its own profile:

```text
profiles/GAME-ID.dat
profiles/GAME-ID.bak
```

The plugin writes new data to a temporary file, validates it, keeps the previous valid profile as `.bak`, and then replaces the main `.dat`. If the main profile becomes damaged, the plugin attempts to restore it automatically from the backup.

Back up the whole `profiles/` directory before replacing a Memory Stick or performing a clean installation.

## Updating

For updates within the PSPAchievementsNG line:

1. Back up `profiles/`.
2. Replace `PSPAchievementsNG.prx`.
3. Replace `config.ini` only when the release notes require it.
4. Keep existing `.pach`, `.pbad`, `.dat`, and `.bak` files unless the release notes explicitly say otherwise.
5. Restart the PSP completely.

For migration from legacy repository releases, follow [Legacy Migration](docs/LEGACY_MIGRATION.md).

## Building from source

PSPDEV / PSPSDK is required.

```bash
export PSPDEV="$HOME/pspdev"
export PATH="$PATH:$PSPDEV/bin"

make clean
make
make dist
```

Output:

```text
dist/PSPAchievementsNG.prx
dist/config.ini
```

The repository also includes GitHub Actions workflows that build the PRX and create release archives from version tags. See [Building](docs/BUILDING.md).

## Known limitations

- Version 2.0.0 is offline-only; there are no accounts, cloud synchronization, public profiles, leaderboards, or social sharing.
- Only the exact Game IDs listed above are currently supported.
- PSP Go internal `ef0:` storage is not supported.
- Very large achievement sets can still cause a small performance cost.
- Some achievement definitions intentionally reject cheat-enabled game states.
- Badge packs are optional; achievements still work when `.pbad` is missing.
- Metal Gear Solid: Peace Walker support is experimental and needs additional unlock testing.

See [Troubleshooting](docs/TROUBLESHOOTING.md) before opening an issue.

## Reporting a problem

Create an issue and include:

- PSP model;
- CFW name and version;
- plugin version;
- game title, region, and exact Game ID;
- clear reproduction steps;
- whether the issue occurs with the achievement menu closed;
- the relevant section of `plugin.log`.

Remove unrelated personal paths or data from logs before posting them.

## Repository policy

The public Git repository intentionally excludes:

- captured service responses and raw trigger definitions;
- downloaded badge caches;
- compiled `.pach` and `.pbad` game packages;
- player profiles, backups, and logs;
- PRX, ELF, object files, and local build output;
- old release archives, `.patch-backups`, and operating-system metadata;
- credentials, tokens, account data, or private endpoints.

Run this before every public commit:

```bash
make public-check
```

See [Repository Policy](docs/REPOSITORY_POLICY.md).

## Credits and legal notice

PSP Achievements System is an independent community project. It is not affiliated with or endorsed by Sony Interactive Entertainment, ARK-4, PPSSPP, or RetroAchievements. PlayStation and PSP are trademarks of their respective owners.

No open-source license has been selected for the repository yet. Source availability does not automatically grant redistribution or commercial-use rights. See [License Notice](LICENSE_NOTICE.md).
