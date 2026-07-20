# Troubleshooting — v2.0.0

Start every diagnosis with:

```text
ms0:/SEPLUGINS/PSPAchievementsNG/logs/plugin.log
```

Do not report only “it does not work.” The log normally identifies the running Game ID, package path, profile state, badge state, evaluator mode, and performance counters.

## No `plugin.log` is created

Check all of the following:

- ARK-4 is active.
- The plugin is enabled in GAME mode, not only VSH mode.
- `ms0:/SEPLUGINS/GAME.TXT` exists.
- The entry is exactly:

  ```text
  ms0:/SEPLUGINS/PSPAchievementsNG/PSPAchievementsNG.prx 1
  ```

- The PRX exists at that path.
- The old legacy `PspAchievements.prx` is disabled.
- The PSP was fully restarted after installation.

## `package load error: 0x80010002`

The matching `.pach` file was not found.

1. Find the log line:

   ```text
   game id: ULES-00151
   ```

2. Confirm this file exists:

   ```text
   ms0:/SEPLUGINS/PSPAchievementsNG/games/ULES-00151.pach
   ```

Do not remove the hyphen from the Game ID.

## `package game id mismatch`

The file belongs to another region or revision. Do not rename it. Install a package created for the exact Game ID shown in the log.

## Achievement logic works but icons do not appear

Check the matching `.pbad` file:

```text
ms0:/SEPLUGINS/PSPAchievementsNG/games/GAME-ID.pbad
```

The log should contain:

```text
badge pack found
badge pack loaded
```

A missing or damaged `.pbad` does not disable achievement evaluation.

## No startup notification or sound

Verify:

```ini
[notifications]
enabled = 1
startup = 1

[audio]
enabled = 1
volume = 100
startup_sound = 1
```

Then exit and restart the game. Configuration is loaded only at game startup.

## Menu does not open

- Hold the combination for the full `hold_ms` duration.
- Confirm `menu.enabled = 1`.
- Test the default `L+R+SELECT` combination.
- Check the `menu button mask` log line.
- Choose a different hotkey when the game constantly uses the configured buttons.

## Menu flickers, tears, or moves

Version 2.0.0 draws the menu only when it opens or the selection changes, while the game threads are temporarily paused. Confirm the log header says:

```text
PSPAchievementsNG 2.0.0
```

Replace old PRX files and retest. Include the PSP model and game title when reporting persistent rendering problems.

## Game performance is poor

1. Keep:

   ```ini
   [performance]
   mode = auto
   ```

2. For GTA, confirm:

   ```text
   adaptive achievement scheduler enabled
   ```

3. Do not force `full` for GTA.
4. Test with the achievement menu closed.
5. Temporarily move the game's `.pach` off the Memory Stick and compare performance.
6. Include these lines in the report:

   ```text
   condition count
   memory reference count
   sample cache hits
   sample cache misses
   scheduler peak conditions
   ```

A small performance cost may remain in GTA because its achievement package is much larger than the other tested games.

## Achievement does not unlock

- Confirm the exact supported region and Game ID.
- Do not use a renamed package.
- Check whether the achievement is session-based, missable, difficulty-specific, or blocked by cheats.
- Confirm that the profile does not already mark it as unlocked.
- Attach the relevant log section and describe the exact in-game action.

## Achievement unlocks repeatedly

The profile may not be writable.

- Check free Memory Stick space.
- Check `profiles/` permissions and storage health.
- Look for profile save errors in the log.
- Do not delete `.bak` until diagnosis is complete.

## Profile recovery messages

These messages indicate automatic recovery and are not necessarily a failure:

```text
profile recovered from backup
primary profile repaired
```

The plugin writes a validated `.tmp`, preserves the previous valid `.dat` as `.bak`, and then commits the new profile.

## Progress disappeared after reinstalling

Restore the backed-up files from:

```text
profiles/GAME-ID.dat
profiles/GAME-ID.bak
```

Legacy `.prof` files are not compatible with PSPAchievementsNG profiles.

## PSP hangs when leaving a game

- Confirm version 2.0.0 is installed.
- Close the achievement menu before pressing HOME and compare the result.
- Disable the plugin and retest.
- Include the last 100 lines of `plugin.log`.
- State whether the hang occurs in one game or every game.

## GTA is detected but the package does not load

The supported release is:

```text
Grand Theft Auto: Liberty City Stories
Europe v3.00
ULES-00151
```

The USA release requires a separate address port and is not supported in v2.0.0.

## PSP Go

The runtime uses `ms0:` paths. PSP Go internal storage uses `ef0:` and is not supported in v2.0.0. External M2 operation has not been verified.

## What to include in a GitHub issue

```text
PSP model:
CFW and version:
Plugin version:
Game title:
Region / revision:
Game ID:
Issue with menu open or closed:
Steps to reproduce:
Expected behavior:
Actual behavior:
Relevant plugin.log lines:
```

Remove unrelated personal information before posting logs.
