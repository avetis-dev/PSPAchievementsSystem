# Building PSPAchievementsNG v1.0.0

## Requirements

- PSPDEV / PSPSDK
- GNU Make
- Python 3 for release packaging and repository validation

## Local build

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

## Validate the public repository

```bash
make public-check
```

The validator rejects captures, raw JSON, badge caches, player profiles, logs, generated game packages, build output, old release directories, and local patch backups.

## Create the public end-user ZIP

```bash
make release VERSION=1.0.0
```

Output:

```text
release/PSPAchievementsSystem-v1.0.0.zip
release/PSPAchievementsSystem-v1.0.0.zip.sha256
```

The default public build contains the runtime, configuration, and documentation but does not embed generated game packages.

## Create a local bundle with game data

```bash
python3 scripts/make_release.py \
  --version 1.0.0 \
  --game-data-dir /path/to/local/games
```

Only `.pach` and `.pbad` files are accepted from that directory. Captures, profiles, logs, and raw JSON are rejected.

## GitHub Actions

- Pushes and pull requests build the PRX in the PSPDEV PSPSDK container.
- A tag matching `v*` builds the PRX, validates the repository, creates a deterministic ZIP, calculates SHA-256, and publishes a GitHub Release.

## Kernel PRX constraints

- The plugin uses `USE_KERNEL_LIBS = 1`.
- Avoid accidental libc dependencies.
- Avoid compiler-generated 64-bit division helpers unless explicitly linked and verified.
- Keep the `OBJS` declaration on one line in the plugin Makefile to avoid tab/continuation mistakes.
- Test changes involving threads, framebuffer access, controller input, audio, or storage on real PSP hardware.
