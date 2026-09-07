# Git Workflow

Owner: management (`mgmt/`)

## 1. The rules

- `main` is protected. Everything arrives through a pull request.
- One approval is required, from a reviewer outside the authoring pair. GitHub counts
  approvals but does not know who wrote the code, so this part is on us.
- Branch names and commit subjects carry a module prefix.
- Never commit build output. `.gitignore` covers the known offenders; read
  `git status` before committing anyway.

## 2. Prefixes

| Prefix | Module |
|---|---|
| `hb/` | heartbeat |
| `snd/` | sound |
| `dec/` | decision |
| `mot/` | motor |
| `mgmt/` | `include/ryb/`, `docs/`, root config files |

Branches look like `hb/bpm-averaging` or `mgmt/ipc-add-checksum`.

Commit subjects are `<prefix>: <imperative, lower case, no full stop>`, under about
70 characters:

```
hb: fix BPM averaging window
mot: clamp duty cycle at 90 percent
dec: add panic-jump detection
```

If that is not enough room, add a blank line and a body explaining why. The diff
already shows what.

## 3. The loop

```bash
git checkout main && git pull
git checkout -b hb/bpm-averaging

# ... edit ...

git status && git diff          # read it before you stage it
./tools/format.sh
git add src/app/hb_filter.c src/app/hb_filter.h
git commit -m "hb: add sliding-window BPM average"
git push -u origin hb/bpm-averaging
```

Then open the PR from the link the push prints, and merge once you have an approval.

Prefer `git add <files>` over `git add -A`. `git add -A` is how a 40 MB `.xsa` and
someone's `_ide/` folder end up in the history for good.

When `main` moves on under you:

```bash
git checkout main && git pull
git checkout hb/bpm-averaging
git merge main
```

Fix conflicts, `git add` the fixed files, commit, push. We merge rather than rebase.
Rebasing rewrites history, and with ten people on shared branches a bad rebase ruins
an afternoon.

## 4. Reviewing

Approving your own pair's work does not count. Ask someone from another module. It is
also the cheapest way to spread knowledge of the shared header around the team.

Check, in order:

1. Vendor headers under `src/app/`. See `docs/coding-standard.md` §1.
2. Anything touching `include/ryb/`. Every affected module owner acknowledges before
   merge, not just you.
3. Fixed-width types on anything crossing the wire.
4. No `malloc`, no `float` in a timing-critical path.
5. Duty-cycle output clamped, in the motor HAL. See `docs/safety-and-constraints.md` §2.
6. Build artefacts in the diff.

"Looks good" without opening the Files changed tab is not an approval.

## 5. When it goes wrong

Committed to `main` by accident:

```bash
git branch hb/my-work        # save the work
git reset --hard origin/main # move main back
git checkout hb/my-work
```

Committed a build artefact:

```bash
git rm --cached path/to/artifact.xsa
# add the pattern to .gitignore, commit both
```

If it is already pushed, say so in the team channel. Large blobs stay in the history
and removing them properly means rewriting it.

No idea what state you are in: run `git status`, then ask before typing anything with
`--hard` or `--force` in it. Nothing is lost until someone forces it away.
