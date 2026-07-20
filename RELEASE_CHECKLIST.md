# v2.0.0 Release Checklist

## Source and version

- [x] `plugin/src/logger.c` reports `PSPAchievementsNG 2.0.0`.
- [x] Root `Makefile` defaults to `VERSION=2.0.0`.
- [x] `config.ini`, README, installation guide, changelog, and release body show v2.0.0.
- [ ] `make public-check` passes.
- [ ] `make clean && make && make dist` succeeds with PSPDEV.

## Hardware smoke tests

- [ ] Silent Hill: Origins loads 66 achievements and badges.
- [ ] Dante's Inferno loads 63 achievements and badges.
- [ ] GTA LCS loads 105 achievements and uses adaptive scheduling.
- [x] GTA VCS Europe `ULES-00502` loads 190 achievements and badges.
- [x] GTA VCS unlocks `Oh Mary, Quite Contrary`, `A Brother in Arms`, and `Loud and Clear, Boss` on real hardware.
- [ ] Metal Gear Solid: Peace Walker loads 110 achievements; unlock behavior remains experimental.
- [ ] Startup notification and sound work.
- [ ] Achievement notification, badge, sound, and profile save work.
- [ ] Menu filters and measured progress work.
- [ ] Menu does not flicker.
- [ ] HOME → Quit Game does not hang.
- [ ] Profile `.bak` recovery is verified.

## Public repository

- [ ] No captures or raw JSON.
- [ ] No `.pach`, `.pbad`, profiles, logs, PRX, ELF, object files, `.DS_Store`, or `.patch-backups`.
- [ ] GitHub Actions build passes.
- [ ] README renders correctly.
- [ ] Legacy migration warning is visible.

## Release assets

- [x] `PSPAchievementsSystem-v2.0.0.zip` created.
- [x] SHA-256 file created.
- [ ] Supported-games archive verified separately.
- [ ] GitHub Release marked Latest when appropriate.
