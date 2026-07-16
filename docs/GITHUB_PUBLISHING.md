# Publishing v1.0.0 to GitHub

Target repository:

```text
https://github.com/avetis-dev/PSPAchievementsSystem
```

The repository already contains legacy history and tags. Do not use `git init` inside an unrelated directory and do not force-push over `main`.

## 1. Authenticate

```bash
brew install gh
gh auth login
gh auth status
```

Confirm the active account:

```bash
gh api user --jq '.login'
```

Expected:

```text
avetis-dev
```

## 2. Clone the repository

```bash
cd "$HOME/Developer"
gh repo clone avetis-dev/PSPAchievementsSystem PSPAchievementsSystem-public
cd PSPAchievementsSystem-public

git switch main
git pull --ff-only origin main
```

## 3. Create a safety branch

```bash
git switch -c release/v1.0.0-ng
```

Optional backup tag for the old main state:

```bash
git tag -a backup-before-ng-v1.0.0 \
  -m "Backup before PSPAchievementsNG v1.0.0"
git push origin backup-before-ng-v1.0.0
```

## 4. Replace the working tree

Extract the prepared GitHub-ready archive, then copy it into the cloned repository while preserving `.git`:

```bash
rsync -av --delete \
  --exclude='.git/' \
  /path/to/PSPAchievementsSystem-v1.0.0-github-ready/ \
  "$HOME/Developer/PSPAchievementsSystem-public/"
```

## 5. Validate before committing

```bash
python3 scripts/check_public_tree.py .
find . \
  \( -name '*.jsonl' \
  -o -name '*.raw.json' \
  -o -name '*.pach' \
  -o -name '*.pbad' \
  -o -name '*.dat' \
  -o -name '*.bak' \
  -o -name '*.log' \
  -o -name '*.prx' \
  -o -name '*.elf' \
  -o -name '*.o' \) \
  -not -path './.git/*'
```

The second command should print nothing.

## 6. Commit and push the branch

```bash
git add -A
git status --short
git commit -m "Release PSPAchievementsNG v1.0.0 rewrite"
git push -u origin release/v1.0.0-ng
```

## 7. Open and merge a pull request

```bash
gh pr create \
  --base main \
  --head release/v1.0.0-ng \
  --title "PSPAchievementsNG v1.0.0 rewrite" \
  --body "Replaces the legacy runtime with the tested multi-game PSPAchievementsNG implementation, new documentation, profiles, badges, sound, menu, configuration, and adaptive performance support."

gh pr view --web
```

After build checks pass:

```bash
gh pr merge --merge --delete-branch
```

Update local main:

```bash
git switch main
git pull --ff-only origin main
```

## 8. Create the release tag

```bash
git tag -a v1.0.0 \
  -m "PSP Achievements System v1.0.0 — PSPAchievementsNG rewrite"
git push origin v1.0.0
```

The repository already has legacy `v1.0.1`–`v1.0.3` tags. In the release description, clearly state that v1.0.0 is a complete runtime reset and supersedes the legacy implementation. Mark the new release as **Latest** manually if GitHub does not select it automatically.

## 9. Upload the supported-games asset

The generated source tree must not contain `.pach` or `.pbad` files. Upload the separate supported-games ZIP to the GitHub Release manually only when you have decided it is appropriate to distribute it.

Suggested release assets:

```text
PSPAchievementsSystem-v1.0.0.zip
PSPAchievementsSystem-v1.0.0.zip.sha256
PSPAchievementsSystem-v1.0.0-supported-games.zip
PSPAchievementsSystem-v1.0.0-supported-games.zip.sha256
```
