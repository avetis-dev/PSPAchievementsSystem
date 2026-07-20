PSP Achievements System v2.0.0
================================

Runtime: PSPAchievementsNG
Purpose: Experimental achievement client for real PSP hardware

Officially tested setup:
- PSP-3000
- ARK-4
- Memory Stick installation using ms0:

IMPORTANT:
This release replaces the old PspAchievements.prx implementation.
Do not enable both plugins at the same time.
This public archive contains no RetroAchievements trigger definitions
or prebuilt .pach packages. Those files are not permitted for redistribution.
Official rcheevos/rc_client integration is planned.

Achievement metadata and badge artwork are provided by RetroAchievements.
This project is independent and is not affiliated with or endorsed by
RetroAchievements.

Quick installation:
1. Copy the SEPLUGINS directory to the root of the Memory Stick.
2. Add this line to ms0:/SEPLUGINS/GAME.TXT:

   ms0:/SEPLUGINS/PSPAchievementsNG/PSPAchievementsNG.prx 1

3. Restart the PSP completely.

The public build cannot activate RetroAchievements sets until the planned
official rc_client integration is completed. Included .pbad files contain
permitted badge artwork only and do not contain trigger logic.

Read INSTALLATION.md, LEGACY_MIGRATION.md, SUPPORTED_GAMES.md,
CONFIGURATION.md, and TROUBLESHOOTING.md before reporting an issue.
