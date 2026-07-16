PSP Achievements System v1.0.0
================================

Runtime: PSPAchievementsNG
Purpose: Offline achievements for real PSP hardware

Officially tested setup:
- PSP-3000
- ARK-4
- Memory Stick installation using ms0:

IMPORTANT:
This release replaces the old PspAchievements.prx implementation.
Do not enable both plugins at the same time.

Quick installation:
1. Copy the SEPLUGINS directory to the root of the Memory Stick.
2. Add this line to ms0:/SEPLUGINS/GAME.TXT:

   ms0:/SEPLUGINS/PSPAchievementsNG/PSPAchievementsNG.prx 1

3. Copy matching GAME-ID.pach files and optional GAME-ID.pbad files to:

   ms0:/SEPLUGINS/PSPAchievementsNG/games/

4. Restart the PSP completely.
5. Launch a supported game.
6. Hold L + R + SELECT to open the achievement menu.

Read INSTALLATION.md, LEGACY_MIGRATION.md, SUPPORTED_GAMES.md,
CONFIGURATION.md, and TROUBLESHOOTING.md before reporting an issue.
