# decision

Owner: unassigned | Board: PYNQ-Z2 #3 | Branch prefix: `dec/`

The brain. Consumes heart rate and loudness, navigates the hidden 5x5 stress matrix,
and commands the next motion region.

| In | Out |
|---|---|
| `ryb_heart_msg_t` from heartbeat, `ryb_sound_msg_t` from sound | `ryb_motion_cmd_t`: `f_region` (1..5), `a_region` (1..5) |

Also drives its own 1.54" LCD: estimated stress, current (F, A), and what it is about
to try.

## The problem

The matrix maps (F, A) pairs to stress levels K9 down to K1, with K9 at (F5, A5) and K1
at (F1, A1). Exactly one path of adjacent moves descends between them, and a wrong move
triggers a panic jump, so blind hill-climbing gets punished.

So the module has to estimate current stress from two noisy signals, judge whether the
last move helped, pick the next adjacent move, and recover deliberately from a panic
jump instead of thrashing.

This module gains the most from the portability rule: navigation is pure computation
over two numbers. Put a simulated matrix in `test/`, run thousands of descents against
it on a laptop, and arrive at a slot with a strategy that already works. Searching for
one on hardware, in booked time, is not a plan. See `docs/coding-standard.md` §1.

## Open questions

- Stress estimation from (BPM, loudness) is undesigned. Loudness saturates above
  S = 50, so it cannot carry the top of the range alone.
- Panic-jump detection and the recovery policy.
- How long to wait after a move before judging its effect. Depends on cradle response
  time, needs measuring.
- What to send before the first valid sensor readings arrive. Tied to open question 6
  in `docs/interface-contract.md`.
- Rescue point: if the team intends to solve a demo matrix instead of calming the baby,
  that has to be described in the design document in advance. It is a documentation
  deadline, so decide early.
