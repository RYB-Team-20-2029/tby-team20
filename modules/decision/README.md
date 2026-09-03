# decision

**Owner:**
**Board:** PYNQ-Z2 #3 · **Branch prefix:** `dec/` · **Status:** not started

The brain. Consumes heart rate and loudness, navigates the hidden 5x5 stress matrix, and commands the next motion region as `ryb_motion_cmd_t`.

## Responsibility

| In | Out |
|---|---|
| `ryb_heart_msg_t` from heartbeat, `ryb_sound_msg_t` from sound | `ryb_motion_cmd_t` — `f_region` (1..5), `a_region` (1..5) |

Also drives its own 1.54" LCD: current estimated stress level, current (F, A), what it is about to try.

## The problem

A hidden 5x5 matrix maps (F, A) pairs to stress levels K9..K1. K9 sits at (F5, A5), K1 at (F1, A1). **Exactly one path of adjacent moves descends from K9 to K1.** A wrong move triggers a panic jump — stress leaps back up — so the search is not free and blind hill-climbing will be punished.

The module therefore has to: estimate the current stress level from two noisy sensed signals, decide whether the last move helped, and choose the next adjacent move. It should also detect a panic jump and recover deliberately rather than thrashing.

## Layout

```
src/main.c   init + super-loop only, target <= 50 lines
src/app/     matrix navigation, stress estimation, panic detection. Portable C, NO vendor headers.
src/hal/     backbone, timer, LCD driver
src/ui/      LCD rendering
include/     module-internal headers
test/        host tests for src/app/ — including a simulated matrix
```

**This module benefits most from the portability rule.** Navigation logic is pure computation over two numbers. Write a host-side simulated matrix in `test/`, run thousands of descents against it on a laptop, and arrive at the slot with a strategy that already works. Doing this search on hardware, in booked time, is not a plan.

## Known issues / open questions

- Stress estimation from (BPM, loudness) is undesigned. Note that loudness saturates above S = 50 — see `modules/sound/README.md`.
- Panic-jump detection and recovery policy undefined.
- How long to wait after a move before judging its effect: unknown, depends on cradle response time. Needs measurement.
- What to send before the first valid sensor readings arrive. Coupled to open question 6 in `docs/interface-contract.md`.
- **Rescue point:** if the team intends to solve a demo matrix instead of calming the baby, that must be described **in advance** in the design document. Decide early; it is a documentation deadline, not a coding one.
