# Safety and Constraints

**Owner:** management (`mgmt/`)

Two kinds of rule live here. **Safety rules** protect people and shared equipment — they are not negotiable and most cannot be enforced by code. **Course constraints** are graded: breaking one costs marks even if the prototype works perfectly.

Read this before your first booked slot.

---

## 1. Safety

### Electrical

- **Maximum 24 V AC / 42 V DC.** Nothing in this project needs more. If a design calls for more, the design is wrong.
- **Never bypass the cradle's emergency stop.** Not to save time, not to get one more run out of a slot, not "just to check something".
- The cradle drive is 12 V and must supply at least 0.8 A. Treat the drive side as capable of hurting you and of destroying a board.

### The setup is shared

Six setups serve 42 teams. Damage is not just your problem.

- **Do not move setups.**
- **Do not remove the doll.**
- **Do not unplug mains.**
- **Do not use adhesive tape to mount the wrist sensor.** Find a mechanical solution — a sleeve, a clip, a strap.

### In the lab

- Power down before rewiring the breadboard.
- Check polarity before applying power. Reversed supply kills a PYNQ-Z2, and there are only four.
- If something smells hot, sounds wrong, or the emergency light comes on: stop, power down, tell a supervisor. Do not debug a fault while it is live.

---

## 2. The 90 % duty ceiling

**PWM duty cycle must never exceed 90 %.** Above it, the cradle trips its emergency light and the run is over.

This is the one safety-adjacent rule that *is* enforceable in code, so enforce it:

- `RYB_PWM_DUTY_MAX_PCT` in `include/ryb/ryb_config.h` is the single definition. Do not retype `90` anywhere.
- The clamp lives in the motor module's HAL, at the last point before the value reaches the peripheral. Not in the decision module — a clamp upstream of an arithmetic bug does not clamp.
- Region 5 tops out at 90 % by construction. Any path that can produce a duty from something other than the region table must be clamped explicitly.
- Test the clamp on the host with out-of-range inputs before it ever reaches hardware.

---

## 3. Course constraints (graded)

| Constraint | Detail |
|---|---|
| **Backbone only** | Boards communicate **only** through the supplied backbone. No extra wires, no direct board-to-board cabling, no jumper "just for the demo". This is graded. |
| **C only** | No other language for the embedded software. |
| **PYNQ-Z2 only** | Four boards, one per submodule. No other hardware platform. |
| **Duty <= 90 %** | See §2. |
| **Circuits on breadboard** | Analog front ends are built on the supplied breadboard, behind the RYB shield's overvoltage protection. |
| **One module per board** | heartbeat, sound, decision, motor — separate boards, separate responsibilities. |

## 4. Hardware per board

- PYNQ-Z2 (Xilinx Zynq-7000)
- RYB shield PCB — overvoltage protection; do not bypass it
- 1.54" LCD module
- Breadboard for analog circuitry
- Backbone connector

## 5. Signal constants

Authoritative values live in `include/ryb/ryb_config.h`. Repeated here for reading away from the code:

- PWM carrier **1 kHz**, amplitude **12 V**, must supply **>= 0.8 A**
- Duty ceiling **90 %**
- Heart rate **60–240 BPM**; stress **10–100 %**
- Crying volume is **constant for 50 < S < 100** and only falls off below S = 50
- ECG bonus work: sampling rate justified against **Nyquist**, front end scaled to **0–3.3 V**

## 6. Booked slots

The physical setups and the digital twin are shared and available only in booked slots. This is why `docs/coding-standard.md` §1 exists: everything that *can* be tested on a laptop *must* be, so that slot time goes to what genuinely needs the cradle.

Before a slot: know what you are testing, have it building, and have the host-side tests passing. Log what happened in `docs/test-log/` afterwards — including the failures, which are the ones you will want to reread.
