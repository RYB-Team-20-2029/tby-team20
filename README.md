# RYB Team 20 — Rock Your Baby

Autonomous embedded system that calms a simulated baby in a motorised cradle.
TU/e course **5ECW0**, academic year 2026–2027.

> **Repository status: scaffolding.** Structure, standards and the interface contract are in place. No application code is written yet, and team members have not been invited. See [Open items](#open-items).

## The problem

The cradle rocks. The baby has a hidden stress level. Our job is to bring it down.

Two signals come **out** of the baby, and two go **in** to the cradle:

| Sensed from the baby | Driven to the cradle |
|---|---|
| Heart rate (60–240 BPM) | Rocking **frequency** (PWM) |
| Crying loudness (0–100 %) | Rocking **amplitude** (PWM) |

A hidden **5x5 stress matrix** maps each (F, A) motion pair to a stress level K9..K1. K9 sits at (F5, A5), K1 at (F1, A1), and **exactly one path of adjacent moves descends from K9 to K1**. A wrong move triggers a panic jump. So this is a guided search under noisy observation, not a control loop with a setpoint.

One catch worth knowing up front: crying volume is **constant for 50 < S < 100** and only falls off below S = 50. Above that knee, loudness tells you almost nothing — heart rate and trend carry the signal.

The deliverable is a working prototype, not a commercial product.

## The four modules

Four PYNQ-Z2 boards, one module each, cooperating over the supplied backbone and nothing else.

| Module | Responsibility | Prefix | |
|---|---|---|---|
| [`heartbeat`](modules/heartbeat/) | Optical sensing of the wrist LED → BPM + trend | `hb/` | → decision |
| [`sound`](modules/sound/) | Microphone conditioning → loudness + trend | `snd/` | → decision |
| [`decision`](modules/decision/) | Matrix navigation → next (F, A) region | `dec/` | → motor |
| [`motor`](modules/motor/) | Generates the two PWM drive signals | `mot/` | → cradle |

## Layout

```
ryb-team20/
├── docs/
│   ├── interface-contract.md      the wire format, in prose + open questions
│   ├── coding-standard.md         the portability rule and the style rules
│   ├── git-workflow.md            branching, commits, PRs — from scratch
│   ├── safety-and-constraints.md  read before your first slot
│   └── test-log/                  what happened in each booked slot
├── include/ryb/                   SHARED. Management-owned. PR + acknowledgement.
│   ├── ryb_types.h                shared enums and return codes
│   ├── ryb_ipc.h                  authoritative wire format, version-stamped
│   └── ryb_config.h               electrical constants, region table, bounds
├── modules/{heartbeat,sound,decision,motor}/
│   ├── src/main.c                 init + super-loop only, <= 50 lines
│   ├── src/app/                   pure logic, portable C, NO vendor headers
│   ├── src/hal/                   thin hardware wrappers
│   ├── src/ui/                    LCD rendering
│   ├── include/  test/            module-internal headers, host tests
│   └── README.md
├── tools/format.sh                applies .clang-format
├── .clang-format  .editorconfig  .gitignore  CODEOWNERS
└── README.md
```

One repository, four module directories — not four repositories. All four boards must agree on **one** wire format, and a single shared header under management ownership prevents the drift that separate repos or copy-pasted headers guarantee. Ownership boundaries are enforced by `CODEOWNERS`, not by repo splits.

## Quickstart

```bash
git clone https://github.com/RYB-Team-20-2029/tby-team20.git
cd tby-team20
git config user.name "Your Name"
git config user.email "your@student.tue.nl"
```

Then, in order:

1. **[`docs/safety-and-constraints.md`](docs/safety-and-constraints.md)** — before you touch hardware. 24 V AC / 42 V DC ceiling, never bypass the emergency stop, duty never above 90 %.
2. **[`docs/coding-standard.md`](docs/coding-standard.md)** — especially §1, the portability rule. It is the most important standard here and the rest of this section explains why.
3. **[`docs/git-workflow.md`](docs/git-workflow.md)** — literal commands, written for people new to git.
4. **[`docs/interface-contract.md`](docs/interface-contract.md)** — what your module sends and receives.

Then work in your module directory, on a branch with your module's prefix.

### The one rule to internalise

**Nothing under `src/app/` may include a vendor header.** Application logic takes plain values in and returns plain values out; hardware access stays in `src/hal/`.

There are six shared cradle setups for 42 teams. Everything under `src/app/` — peak detection, BPM averaging, matrix navigation, duty mapping — compiles with `gcc` on a laptop and can be tested at your desk. Slot time then goes to what genuinely needs the cradle. This is not architectural taste; it is how the team avoids spending booked hardware time on off-by-one errors.

## Milestones

| Date | Event | Weight |
|---|---|---|
| **15 Oct 2026** | **Demo 1** — early per-module designs plus a 4-board communication demo, ≥ 3 signals shown on LCDs | Up to **1 bonus point**, carried into the final |
| **3 Dec 2026** | **Demo 2** — full-system dry run | Ungraded |
| **14 Jan 2027** | **Final Demo** — on the physical cradle | Graded **out of 10**, bonus included |

The **design document** is a separate graded deliverable with a supplied template. If the team intends to use the **rescue point** — solving a demo matrix instead of calming the baby — that must be described in the design document **in advance**. It is a documentation deadline, so decide early.

Note what Demo 1 actually asks for: four boards talking over the backbone with signals on the LCDs. That is an integration milestone, not four separate module milestones. It rewards agreeing the wire format early — which is why [`docs/interface-contract.md`](docs/interface-contract.md) exists before any module code does.

## Constraints

Non-negotiable, and several are graded:

- **Backbone only** — no extra wires, no direct board-to-board cabling
- **C only** · **PYNQ-Z2 only** · one module per board
- **PWM duty ≤ 90 %** — above it, the cradle trips its emergency light
- Analog circuitry on the breadboard, behind the RYB shield
- Max 24 V AC / 42 V DC · never bypass the emergency stop

Full list in [`docs/safety-and-constraints.md`](docs/safety-and-constraints.md).

## Open items

Carried deliberately, not overlooked:

- **Members are not yet invited.** `CODEOWNERS` holds commented placeholders and every module README has a blank `Owner:` field. The repository owner fills these in after invites.
- **Backbone framing is undecided** — physical link, update rates, byte order, whether a header carries a version and sender id. Tracked as open questions in [`docs/interface-contract.md`](docs/interface-contract.md). Nothing was guessed and written into the header, because a guess that ships is a guess four boards then have to un-ship.
- **Pin assignments are undecided** — marked `TODO` in `ryb_config.h`, as comments rather than plausible-looking defaults.
- **No application code exists yet.** `main.c` files are commented placeholders.
