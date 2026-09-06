# RYB Team 20 | Rock Your Baby

TU/e course **5ECW0**, academic year 2026–2027.

## The problem

There are two core signals that can be sensed and manipulated:

| Sensed from the baby | Driven to the cradle |
|---|---|
| Heart rate (60–240 BPM) | Rocking frequency (PWM) |
| Crying loudness (0–100 %) | Rocking amplitude (PWM) |

A hidden 5x5 stress matrix maps each (F, A) motion pair to a stress level K9..K1. K9 sits at (F5, A5), K1 at (F1, A1). Exactly one path of moves descends from K9 to K1.
In the condition where a wrong move has been made, the baby jumps to a different stress state.

> crying volume is constant for 50 < S < 100 and only falls off below S = 50. Above that point, there can be no calculated difference and this cannot be used reliably.

## The four modules

The published documents on Canvas outline a clear structural architecture to implement, using four PYNQ-Z2 boards ("Pink" boards), each are running modules that communicate over the supplied PCB interconnect.

| Module | Responsibility | Prefix | |
|---|---|---|---|
| [`heartbeat`](modules/heartbeat/) | Optical sensing of the wrist LED (BPM + trend) | `hb/` | decision |
| [`sound`](modules/sound/) | Microphone conditioning (loudness + trend) | `snd/` | decision |
| [`decision`](modules/decision/) | Matrix navigation (next (F, A) region) | `dec/` | motor |
| [`motor`](modules/motor/) | Generates the two PWM drive signals | `mot/` | cradle |

## Layout

```
tby-team20/
├── docs/
│   ├── interface-contract.md      physical wire format
│   ├── coding-standard.md         portability rule and the style rules
│   ├── git-workflow.md            branching, commits, PRs
│   ├── safety-and-constraints.md  read before your first slot
│   └── test-log/                  what happened in each booked slot
├── include/ryb/                   SHARED for PR acknowledgement
│   ├── ryb_types.h                shared enums and return codes
│   ├── ryb_ipc.h                  authoritative wire format needs to be version-stamped
│   └── ryb_config.h               electrical constants, region table, bounds
├── modules/{heartbeat,sound,decision,motor}/
│   ├── src/main.c                 init + super-loop ideally <= 50 lines
│   ├── src/app/                   pure logic and portable C with NO vendor headers
│   ├── src/hal/                   thin hardware wrappers
│   ├── src/ui/                    LCD rendering
│   ├── include/  test/            module internal headers and host tests
│   └── README.md
├── tools/format.sh                applies .clang-format
├── .clang-format  .editorconfig  .gitignore  CODEOWNERS
└── README.md
```
When considering the writing external libraries, consider if they might be useful upstream in the RBY general libraries for use will all sub-teams. If so, say something!

## Quickstart

```bash
gh login
git clone https://github.com/RYB-Team-20-2029/tby-team20.git
cd tby-team20
```

In order, please read:

1. **[`docs/safety-and-constraints.md`](docs/safety-and-constraints.md)** Safety related documentation abbreviated. Read this if you haven't yet read the PDF files or need to double check.
2. **[`docs/coding-standard.md`](docs/coding-standard.md)** Programming and development standards, this is very important! Includes rules on portability and code tests which cannot be skipped (PR will be rejected).
3. **[`docs/git-workflow.md`](docs/git-workflow.md)** List of commands if you forget how to use github and git.
4. **[`docs/interface-contract.md`](docs/interface-contract.md)** Basic input/output list of what your program or sub-module receives and sends over the bus as well as physical sensor inputs.

You are then free to work, on a branch with your module's prefix!

### Internalization

**Nothing under `src/app/` should include a vendor header.** Application logic takes plain values in and returns plain values out. The hardware access functions stay strictly in `src/hal/`.

There are six shared cradle setups for 42 teams. Everything under `src/app/` includes peak detection, BPM averaging, matrix navigation, duty mapping and compiles with `gcc` on a laptop. This can be tested without a physical hardware setup. This helps prevent programming issues when we are prototyping on the hardware.
I will reject PR requests which do not compile or test correctly under cradle, or which have badly written tests.

Tests essentially run per-computed input and checks outputs from the function to see if it behaves correctly fully in software.

## Milestones

| Date | Event | Weight |
|---|---|---|
| **15 Oct 2026** | **Demo 1** All sub-module logic should be completed and all sub-modules should be communicating together | Up to **1 bonus point**, carried into the final |
| **3 Dec 2026** | **Demo 2** Sensor input is functional and tested on the real cradle, its fine if there are issues or bugs | Ungraded |
| **14 Jan 2027** | **Final Demo** Should be operational and have one or two extra futures | Graded **out of 10**, bonus included |

The design document is a separate graded deliverable with a supplied template. There is a *rescue point* described in the docs, involving solving a demo matrix instead of calming the baby. that must be described in the design document in advance before the final submission of the document. This is NOT something we should consider.

Please consider what Demo 1 actually aims for. Its a system integration deadline, NOT a sub-module deadline. If a sub-module does not implement the baseline bare-minimum communication and logic to be considered "functional" or "testable" then your subteam is behind the schedule! This standard is outlined in [`docs/interface-contract.md`](docs/interface-contract.md).

## Constraints

These are graded constraints:

- **Backbone only** We are not allowed to wire-in any inter-board cables outside of the backbone.
- **C only** and **PYNQ-Z2 only**
- **PWM duty ≤ 90 %** Anything above 90% will automatically trip the cradle motor breaker.
- Analog circuitry has to be on the breadboard, behind the RYB shield.
- Max 24 V AC / 42 V DC never bypass the emergency stop.

Full list in [`docs/safety-and-constraints.md`](docs/safety-and-constraints.md).
