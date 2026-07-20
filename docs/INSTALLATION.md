# Installation Guide — v2.0.0

This guide covers a clean installation, migration from legacy repository releases, first-start verification, updates, backups, and uninstallation.

## 1. Requirements

You need:

- a real PSP console;
- ARK-4 custom firmware;
- Memory Stick storage available as `ms0:`;
- the v2.0.0 plugin release archive;
- the matching supported-game package archive;
- one of the exact game IDs listed in `SUPPORTED_GAMES.md`.

Officially verified hardware:

```text
PSP-3000 + ARK-4 + ms0:
```

Other PSP models may work, but they are not yet officially verified. PSP Go internal `ef0:` storage is not supported in v2.0.0.

## 2. Files used by the plugin

The main plugin archive should provide:

```text
SEPLUGINS/PSPAchievementsNG/PSPAchievementsNG.prx
SEPLUGINS/PSPAchievementsNG/config.ini
```

The supported-game archive should provide pairs such as:

```text
ULUS-10285.pach
ULUS-10285.pbad
```

File meaning:

| Extension | Purpose | Required |
|---|---|---|
| `.prx` | PSPAchievementsNG runtime plugin | Yes |
| `.ini` | User configuration | Recommended |
| `.pach` | Achievement logic and metadata for one exact Game ID | Yes |
| `.pbad` | Badge artwork for the same Game ID | No, but recommended |
| `.dat` | Automatically generated player profile | Generated automatically |
| `.bak` | Previous valid profile backup | Generated automatically |

## 3. Before installation

### Back up existing progress

Copy this folder to your computer when it already exists:

```text
ms0:/SEPLUGINS/PSPAchievementsNG/profiles/
```

### Check for a legacy installation

Older releases from this repository used a different plugin and directory structure, commonly including:

```text
ms0:/seplugins/PspAchievements.prx
ms0:/PSP/ACH/
```

Do not run the legacy plugin and PSPAchievementsNG simultaneously. Follow `LEGACY_MIGRATION.md` before continuing.

## 4. Clean installation

### Step 1 — Connect the Memory Stick

Use PSP USB mode or a card reader. In this guide, the Memory Stick root is represented as `ms0:/`.

### Step 2 — Copy the plugin directory

Copy the release archive's `SEPLUGINS` directory to the Memory Stick root.

After copying, this exact file must exist:

```text
ms0:/SEPLUGINS/PSPAchievementsNG/PSPAchievementsNG.prx
```

The capitalization shown above is recommended for consistency.

### Step 3 — Enable the plugin in GAME mode

Open or create:

```text
ms0:/SEPLUGINS/GAME.TXT
```

Add exactly one line:

```text
ms0:/SEPLUGINS/PSPAchievementsNG/PSPAchievementsNG.prx 1
```

Do not add the same PRX more than once. Disable old `PspAchievements.prx` entries.

### Step 4 — Install game packages

Copy all supported-game files into:

```text
ms0:/SEPLUGINS/PSPAchievementsNG/games/
```

For the current tested set:

```text
ULUS-10285.pach
ULUS-10285.pbad
ULUS-10469.pach
ULUS-10469.pbad
ULES-00151.pach
ULES-00151.pbad
ULES-00502.pach
ULES-00502.pbad
```

An experimental package is also available for Metal Gear Solid: Peace Walker USA:

```text
ULUS-10509.pach
ULUS-10509.pbad
```

You may install all packages at once. The plugin automatically selects the package matching the running Game ID. Do not rename `ULES-00502.pach` for `ULUS-10160`: those Vice City Stories revisions use different memory layouts.

### Step 5 — Confirm the final layout

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
        └── logs/
```

The empty `profiles/` and `logs/` directories are optional because the plugin can create them.

### Step 6 — Restart the console

Safely leave USB mode and fully restart the PSP or restart the custom firmware. A simple return to XMB may not reload the GAME plugin list in every setup.

### Step 7 — Launch a supported game

A successful first start should display a short startup notification and sound after the game package is initialized.

## 5. First-start verification

Open:

```text
ms0:/SEPLUGINS/PSPAchievementsNG/logs/plugin.log
```

A successful startup contains:

```text
PSPAchievementsNG 2.0.0
config loaded
game id: ULUS-10285
game identification succeeded
game package found
achievement engine initialized
game package loaded
badge pack loaded
startup notification queued
achievement menu ready
```

The Game ID changes with the running game.

### Missing package example

```text
package load error: 0x80010002
```

This means the required `.pach` file was not found. Read the `game id:` line and verify that a file with the same Game ID exists in `games/`.

## 6. Open the achievement menu

Hold:

```text
L + R + SELECT
```

for approximately 400 ms.

Default controls:

| Control | Action |
|---|---|
| Up / Down | Previous or next achievement |
| Left / Right | Previous or next page |
| Triangle | Change list filter |
| L / R | Jump to first or last item |
| Circle | Close the menu |

The hotkey can be changed in `config.ini`.

## 7. Installing only one game

It is safe to install only one `.pach` / `.pbad` pair. Unsupported or uninstalled games will create a log entry indicating that the matching package was not found; the plugin will not use a package belonging to another Game ID.

## 8. Updating PSPAchievementsNG

1. Back up `profiles/`.
2. Replace:

   ```text
   PSPAchievementsNG.prx
   ```

3. Replace `config.ini` only when the release notes introduce new settings or require a reset.
4. Preserve:

   ```text
   games/*.pach
   games/*.pbad
   profiles/*.dat
   profiles/*.bak
   ```

5. Restart the PSP completely.
6. Confirm the new version in `plugin.log`.

## 9. Moving to another Memory Stick

Copy the entire directory:

```text
ms0:/SEPLUGINS/PSPAchievementsNG/
```

Also copy the `GAME.TXT` plugin entry. Keeping the whole directory preserves configuration, packages, badges, profiles, and backups.

## 10. Uninstallation

1. Remove or disable this line from `GAME.TXT`:

   ```text
   ms0:/SEPLUGINS/PSPAchievementsNG/PSPAchievementsNG.prx 1
   ```

2. Restart the PSP.
3. Back up `profiles/` when progress should be preserved.
4. Delete:

   ```text
   ms0:/SEPLUGINS/PSPAchievementsNG/
   ```

## 11. Common installation mistakes

- Copying `games/` into `profiles/`.
- Using the USA game with a European package or the opposite.
- Renaming a `.pach` file instead of using a real regional package.
- Leaving the legacy plugin enabled at the same time.
- Adding the PRX to VSH mode instead of GAME mode.
- Forgetting the final `1` in `GAME.TXT`.
- Installing on PSP Go internal `ef0:` storage.
- Forcing `[performance] mode = full` for GTA.

For diagnosis, continue with `TROUBLESHOOTING.md`.
