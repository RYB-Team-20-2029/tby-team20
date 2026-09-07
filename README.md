# RYB Team 20 | Rock Your Baby

TU/e 5ECW0, 2026-2027.

A cradle rocks a doll. The doll has a heart rate and a crying volume; we sense both,
work out how stressed it is, and pick a rocking motion that calms it down.

Four PYNQ-Z2 boards, one module each, talking over the supplied backbone.

| Module | Does | Sends to | Prefix |
|---|---|---|---|
| [`heartbeat`](modules/heartbeat/) | Optical wrist sensing, BPM + trend | decision | `hb/` |
| [`sound`](modules/sound/) | Microphone conditioning, loudness + trend | decision | `snd/` |
| [`decision`](modules/decision/) | Stress-matrix navigation, next (F, A) region | motor | `dec/` |
| [`motor`](modules/motor/) | Two PWM drive signals to the cradle | cradle | `mot/` |

## The problem

A hidden 5x5 matrix maps each (F, A) motion pair to a stress level, K9 (worst) down to
K1 (calm). K9 is at (F5, A5), K1 at (F1, A1), and exactly one path of adjacent moves
descends between them. A wrong move makes the baby panic and jump to a different state,
so guessing is expensive.

One catch worth knowing up front: crying volume is flat for 50 < S < 100 and only drops
below S = 50. Loudness alone cannot tell high stress levels apart.

## Layout

```
docs/                             read these first, in the order below
include/ryb/                      shared headers, management-owned
  ryb_types.h                     enums and return codes
  ryb_ipc.h                       wire format (authoritative)
  ryb_config.h                    electrical constants, region table, bounds
modules/{heartbeat,sound,decision,motor}/
  src/main.c                      init + super-loop, aim for <= 50 lines
  src/app/                        portable logic, no vendor headers
  src/hal/                        thin hardware wrappers
  src/ui/                         LCD rendering
  include/  test/                 module-internal headers, host tests
tools/format.sh                   applies .clang-format
```

## Start here

```bash
git clone https://github.com/RYB-Team-20-2029/tby-team20.git
cd tby-team20
```

Read in this order:

1. [`docs/safety-and-constraints.md`](docs/safety-and-constraints.md) - what will hurt you, break shared hardware, or cost marks.
2. [`docs/coding-standard.md`](docs/coding-standard.md) - the portability rule and the style rules. PRs are rejected against this.
3. [`docs/git-workflow.md`](docs/git-workflow.md) - branches, commits, reviews.
4. [`docs/interface-contract.md`](docs/interface-contract.md) - what your module receives and sends.

Then branch with your module's prefix and go.

If you write something that other modules could use, say so in the team channel
before it ends up copy-pasted four times.

## Milestones

| Date | Event | Weight |
|---|---|---|
| 15 Oct 2026 | Demo 1: all modules talking to each other | up to 1 bonus point, carried into the final |
| 3 Dec 2026 | Demo 2: sensors working on the real cradle | ungraded |
| 14 Jan 2027 | Final demo | graded out of 10 |

Demo 1 is an integration deadline, not a per-module one. If your module cannot send or
receive its messages by then, the whole team is late, not just you.

The design document is a separate graded deliverable with its own template.
