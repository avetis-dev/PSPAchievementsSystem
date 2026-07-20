# Supported Games — v2.0.0

Support is tied to the exact PSP Game ID and executable revision. Region names alone are not sufficient, and renaming a package does not translate its memory addresses.

## Compatibility matrix

| Game | Region / revision | PSP Game ID | Achievements | Points | Mode | Verified result |
|---|---|---:|---:|---:|---|---|
| Silent Hill: Origins | USA | `ULUS-10285` | 66 | 666 | Full-rate | Complex unlock confirmed on PSP-3000 / ARK-4 |
| Dante's Inferno | USA | `ULUS-10469` | 63 | 666 | Full-rate | First unlock confirmed on PSP-3000 / ARK-4 |
| Grand Theft Auto: Liberty City Stories | Europe v3.00 | `ULES-00151` | 105 | 1016 | Adaptive | First unlock confirmed on PSP-3000 / ARK-4 |
| Grand Theft Auto: Vice City Stories | Europe v1.02 | `ULES-00502` | 190 | 1532 | Adaptive | Three story unlocks confirmed on PSP-3000 / ARK-4 |
| Metal Gear Solid: Peace Walker | USA | `ULUS-10509` | 110 | 2100 | Ultra-heavy adaptive | Package loads; achievement unlock testing is incomplete |

Total available in the tested set:

```text
534 achievements
5980 points
```

## Silent Hill: Origins

```text
Game ID: ULUS-10285
Region: USA
Package: ULUS-10285.pach
Badges: ULUS-10285.pbad
```

Notes:

- Uses full-rate evaluation.
- Badge artwork, sound, profile saving, menu filters, and measured progress are supported.
- Existing profiles are tied to stable achievement IDs and the package identity.

## Dante's Inferno

```text
Game ID: ULUS-10469
Region: USA
Package: ULUS-10469.pach
Badges: ULUS-10469.pbad
```

Notes:

- Uses full-rate evaluation.
- Includes pointer-based values, big-endian memory reads, hit counts, and session-based conditions.
- Some missable achievements intentionally reset after death, failure, or a session restart.

## Grand Theft Auto: Liberty City Stories

```text
Game ID: ULES-00151
Region: Europe
Revision: v3.00
Package: ULES-00151.pach
Badges: ULES-00151.pbad
```

Notes:

- The USA release is not supported by this package.
- Uses adaptive scheduling because the achievement set is much larger than the other tested packages.
- Keep `[performance] mode = auto`.
- A small performance cost may remain during heavy gameplay.
- Some conditions intentionally reject cheat-enabled game states.

## Grand Theft Auto: Vice City Stories

```text
Game ID: ULES-00502
Region: Europe
Revision: v1.02
Package: ULES-00502.pach
Badges: ULES-00502.pbad
```

Notes:

- Uses adaptive scheduling for its 190-achievement set.
- `Oh Mary, Quite Contrary`, `Loud and Clear, Boss`, and `A Brother in Arms` have been confirmed on PSP-3000 / ARK-4.
- The USA release and Russian translations identified as `ULUS-10160` are not compatible with this package.
- Rebuilding the European package with another Game ID does not translate its memory addresses.

## Metal Gear Solid: Peace Walker

```text
Game ID: ULUS-10509
Region: USA
Package: ULUS-10509.pach
Badges: ULUS-10509.pbad
```

Notes:

- The full package loads successfully with 110 achievements.
- It uses the ultra-heavy adaptive scheduler because the set contains nearly 12,000 conditions.
- Hardware loading and menu operation are confirmed, but achievement unlock behavior still needs broader testing.
- Treat this package as experimental in v2.0.0.

## Unsupported regions and revisions

The plugin validates the internal Game ID in every `.pach` and `.pbad` file. A mismatched package is refused instead of being evaluated against the wrong memory layout.

Examples that do not work:

```text
Rename ULES-00151.pach to ULUS-10041.pach
Rename ULES-00502.pach to ULUS-10160.pach
Use a European package with a USA executable
Use a package for a different revision of the same title
```

A separate tested package is required for every incompatible executable layout.
