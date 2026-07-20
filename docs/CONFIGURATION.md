# Configuration — v2.0.0

Configuration is loaded once when the plugin starts in GAME mode:

```text
ms0:/SEPLUGINS/PSPAchievementsNG/config.ini
```

Keys and section names are case-insensitive. Boolean values accept `1/0`, `true/false`, `yes/no`, and `on/off`. Invalid or unknown values are counted as warnings in `plugin.log`; supported settings fall back to safe defaults when parsing fails.

## Recommended configuration

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

## `[general]`

| Key | Values | Meaning |
|---|---|---|
| `enabled` | Boolean | Master runtime switch |
| `logging` | Boolean | Write diagnostic output to `logs/plugin.log` |

Disabling logging makes troubleshooting substantially harder.

## `[notifications]`

| Key | Values | Meaning |
|---|---|---|
| `enabled` | Boolean | Enable achievement unlock notifications |
| `startup` | Boolean | Show the game-ready notification |
| `duration_ms` | Milliseconds | Unlock notification duration |
| `startup_duration_ms` | Milliseconds | Startup notification duration |

Very short durations may make text difficult to read.

## `[audio]`

| Key | Values | Meaning |
|---|---|---|
| `enabled` | Boolean | Master sound switch |
| `volume` | `0`–`100` | Plugin sound level |
| `startup_sound` | Boolean | Play the game-ready sound |
| `unlock_sound` | Boolean | Play the achievement sound |

The plugin synthesizes its sounds at runtime and does not require external audio files.

## `[menu]`

| Key | Values | Meaning |
|---|---|---|
| `enabled` | Boolean | Enable the in-game achievement browser |
| `hotkey` | Button combination | Combination used to open and close the menu |
| `hold_ms` | Milliseconds | Required hold duration |
| `show_badges` | Boolean | Display artwork when a compatible `.pbad` is loaded |

Supported button names:

```text
L R SELECT START
UP RIGHT DOWN LEFT
TRIANGLE CIRCLE CROSS SQUARE
```

Join buttons with `+`:

```ini
hotkey = L+R+START
```

Avoid combinations used constantly by the game.

## `[performance]`

```ini
[performance]
mode = auto
```

| Mode | Behavior |
|---|---|
| `auto` | Recommended. Uses full-rate evaluation for normal packages and adaptive scheduling for heavy packages |
| `full` | Forces full-rate evaluation for all games; may cause serious slowdown in GTA |
| `adaptive` | Forces distributed evaluation for all packages |

Keep `auto` for normal play.

## Applying changes

Configuration is not reloaded while a game is running. Exit the game completely and launch it again after editing `config.ini`.

## Confirming parsed values

Look for lines such as:

```text
config loaded
config warning count: 0x00000000
notification duration ms: 0x00000FA0
audio volume percent: 0x00000064
menu button mask: 0x00000301
performance mode: auto
```
