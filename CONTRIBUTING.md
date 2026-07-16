# Contributing

Contributions are welcome for runtime stability, PSP model compatibility, documentation, performance, user interface improvements, and additional testing.

## Before opening a pull request

1. Run `make public-check`.
2. Build with PSPDEV using `make clean && make`.
3. Test on real PSP hardware when changing kernel threads, framebuffer access, controls, audio, memory reads, profile storage, or lifecycle behavior.
4. Do not commit captures, generated game packages, badge artwork, profiles, logs, credentials, or local release archives.
5. Keep kernel PRX code free of accidental libc or unsupported compiler-runtime dependencies.
6. Preserve compatibility with existing `.pach v3`, `.pbad v1`, and profile data unless a migration is explicitly documented.

## Bug reports

Include the PSP model, CFW version, exact Game ID, plugin version, reproduction steps, and relevant log lines.

## Game compatibility reports

State the exact Game ID and executable revision. Region names alone are not sufficient.

## License note

No open-source license has been selected yet. Discuss licensing with the repository owner before contributing code intended for redistribution.
