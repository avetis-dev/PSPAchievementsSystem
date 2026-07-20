# Migration from Legacy Releases

PSPAchievementsNG 1.0.0 introduced the new runtime and directory layout. Version 2.0.0 keeps that layout and profile format while expanding the engine for much larger game sets.

## Legacy files to look for

Older installations commonly used:

```text
ms0:/seplugins/PspAchievements.prx
ms0:/PSP/ACH/
```

They may also contain old `.ach`, `.prof`, `game_map.dat`, or other data files that are not compatible with PSPAchievementsNG.

## Safe migration

1. Back up the entire Memory Stick or at least all achievement-related directories.
2. Disable every legacy `PspAchievements.prx` entry in ARK-4 or plugin text files.
3. Restart the PSP and confirm that the old plugin is no longer active.
4. Install PSPAchievementsNG using `INSTALLATION.md`.
5. Install the new `.pach` and `.pbad` packages.
6. Launch each supported game and verify `plugin.log`.

## Profile compatibility

Legacy `.prof` or older profile formats are not imported automatically. PSPAchievementsNG uses:

```text
ms0:/SEPLUGINS/PSPAchievementsNG/profiles/GAME-ID.dat
```

Do not rename an old legacy profile to `.dat`. The internal format and achievement IDs may be different.

## Can old files be kept?

They may be kept as an offline backup on a computer, but do not leave the old PRX enabled together with PSPAchievementsNG. Running both plugins can cause duplicate memory evaluation, framebuffer conflicts, audio conflicts, reduced performance, or crashes.

## Repository version note

The repository contains older `v1.0.1`–`v1.0.3` tags from the legacy implementation. The current release should be identified by the `PSPAchievementsNG 2.0.0` log header.
