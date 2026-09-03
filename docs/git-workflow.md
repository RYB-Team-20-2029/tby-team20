# Git Workflow

**Owner:** management (`mgmt/`)

Written assuming some readers are new to git. If you already know git, read §1 and §2 and skip to §6.

---

## 1. The rules, in short

- **`main` is protected.** Nobody pushes to it directly. Everything arrives through a pull request.
- **One approval is required**, and it must come from **a reviewer outside the authoring pair**. GitHub cannot enforce that last part — it counts approvals, it does not know who wrote the code. It is on us.
- **Branch names carry a module prefix**, so ten people's branches stay legible.
- **Commit subjects name the module.**
- **Never commit IDE-generated build output.** `.gitignore` covers the known offenders; check `git status` before you commit anyway.

## 2. Branch prefixes

| Prefix | Module |
|---|---|
| `hb/` | heartbeat |
| `snd/` | sound |
| `dec/` | decision |
| `mot/` | motor |
| `mgmt/` | management-owned paths: `include/ryb/`, `docs/`, root config files |

Example: `hb/bpm-averaging`, `mgmt/ipc-add-checksum`.

## 3. Commit subjects

`<prefix>: <imperative, lower case, no full stop>`

```
hb: fix BPM averaging window
mot: clamp duty cycle at 90 percent
dec: add panic-jump detection
mgmt: bump IPC version for sender id
```

Under ~70 characters. If you need more, add a blank line and a body explaining **why**. The diff already shows what.

## 4. The whole loop, as literal commands

Once, on a new machine:

```bash
git clone https://github.com/RYB-Team-20-2029/tby-team20.git
cd tby-team20
git config user.name "Your Name"
git config user.email "your@student.tue.nl"
```

Every time you start a piece of work:

```bash
# 1. start from an up-to-date main
git checkout main
git pull

# 2. branch, with your module's prefix
git checkout -b hb/bpm-averaging

# 3. ... edit files ...

# 4. see what you changed, and read it before staging
git status
git diff

# 5. format C code before committing
./tools/format.sh

# 6. stage and commit
git add src/app/hb_filter.c src/app/hb_filter.h
git commit -m "hb: add sliding-window BPM average"

# 7. push the branch (first push on a new branch needs -u)
git push -u origin hb/bpm-averaging

# 8. open the pull request
#    either in the browser (the push prints a link), or:
gh pr create --base main --title "hb: add sliding-window BPM average" --fill
```

Then wait for one approval, and merge when you have it.

Prefer `git add <specific files>` over `git add -A`. `git add -A` is how a 40 MB `.xsa` and someone's `_ide/` folder end up in the history.

## 5. When `main` has moved on

If others merged while you were working:

```bash
git checkout main
git pull
git checkout hb/bpm-averaging
git merge main
```

Fix any conflicts, `git add` the fixed files, `git commit`, push again. We merge rather than rebase: rebasing rewrites history, and with ten people and shared branches, a bad rebase is a genuinely unpleasant afternoon.

## 6. Reviewing

Requesting a review of your own pair's work does not count. Find someone from another module — it is also the cheapest way to spread knowledge of the shared header across the team.

As a reviewer, check:

1. **Does anything under `src/app/` include a vendor header?** This is the first thing to look for. See `docs/coding-standard.md` §1.
2. **Does it touch `include/ryb/`?** Then every affected module owner acknowledges before merge, not just you.
3. Fixed-width types on anything crossing the wire.
4. No `malloc`, no `float` in a timing-critical path.
5. Any duty-cycle output clamped at 90 %.
6. Build artefacts in the diff.

"Looks good" without opening the Files-changed tab is not an approval; it is a coin flip with extra steps.

## 7. Common situations

**I committed to `main` by accident, before it was protected.**
```bash
git branch hb/my-work        # save the work on a branch
git reset --hard origin/main # move main back
git checkout hb/my-work
```

**I committed a build artefact.**
```bash
git rm --cached path/to/artifact.xsa
# add the pattern to .gitignore, then commit both changes
```
If it is already pushed, say so in the team channel — large blobs stay in the history and removing them properly requires rewriting it.

**I have no idea what state I am in.** `git status` first, then ask before typing anything with `--hard` or `--force` in it. Nothing is lost until someone forces it away.
