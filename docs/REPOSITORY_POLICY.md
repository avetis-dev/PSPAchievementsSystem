# Repository Policy

The Git repository contains the plugin runtime source, build files, release automation, and documentation.

The following data must not be committed:

- `*.jsonl` achievement captures;
- `*.raw.json` service responses;
- downloaded badge artwork or badge caches;
- compiled `.pach` and `.pbad` game data;
- `.dat` and `.bak` player profiles;
- plugin logs and crash dumps;
- PRX, ELF, object files, and `dist/` output;
- `.patch-backups/`, editor state, or OS metadata;
- credentials, tokens, account data, or private endpoints.

Before every public commit:

```bash
make public-check
```

Game support may be documented without committing the underlying captured responses or local player data.
