# Safety and Constraints

**Owner:** "management" sub-team (`mgmt/`)

There are two types of rules outlined here. Safety rules protect people and shared equipment, they are not negotiable and usually are not enforceable strictly in code. Next there are course constraints, breaking one costs marks even if the prototype works and we meet deadlines. All code that you are working on should have its own branches, which you shouldn't pull into the main branch without a PR (Pull Request) that someone from the "management" sub-team official organizes.

---

## 1. Safety

### Electrical

- **Maximum 24 V AC / 42 V DC.** Nothing in RYB should need more then this. If you EVER find yourself thinking you need to go above it, you don't and shouldn't.
- **Never bypass the cradle's emergency stop.** This will be a major issue if you do not follow it, and might even result in penalties. NEVER, not "just to check something".
- The cradle drive is 12 V and must supply at least 0.8 A. The drive side as capable of hurting you and of destroying hardware, so be careful and keep an eye out.

### The setup is shared

There are six complete setups which are used by 42 teams including us.
If we break something, we pay for it as a team, so don't break or damage anything.

To facilitate this, follow the following rules:

- **Do not move setups.**
- **Do not remove the doll.**
- **Do not unplug mains.**
- **Do not use tape to mount the wrist sensor.** This will require a mechanical design.

### In the lab

- Turn off all Pink boards before changing the breadboard wiring.
- Check polarity before applying power. A reversed power supply can and will kill a PYNQ-Z2, its your hardware and you probably don't want to kill it.
- If something smells hot, sounds wrong, or the emergency light comes on. stop! power down. Do not try touching or interacting with hardware once the power is removed.

---

## 2. 90 % duty cycle

**PWM duty cycle must never exceed 90 %.** Above it the cradle emergency breaker trips and the run is disqualified.

This is the one safety-adjacent rule that *is* enforceable in code, be sure to follow it, specifically for the motor driver subteam:

- `RYB_PWM_DUTY_MAX_PCT` in `include/ryb/ryb_config.h` is the single definition. I already wrote that definition in there, USE THAT! Don't redefine it as a different variable, the value is right there.
- The clamp lives in the motor module's HAL, at the last point before the value reaches the peripheral. Never write it in the decision module, your PR WILL be rejected if it doesn't follow this!
- Region 5 tops out at 90 % by design. Any path that can produce a duty cycle from something other than the region table must be clamped explicitly.
- Test the clamp on the host with out-of-range inputs before we ever run it in hardware.

---

## 3. Course constraints (graded)

| Constraint | Detail |
|---|---|
| **Backbone only** | Boards communicate **only** through the supplied backbone. No extra wires, no direct board-to-board cabling, no jumper "just for the demo". This is  directly graded and reviewed. |
| **C only** | No other language for the embedded software. |
| **PYNQ-Z2 only** | Four boards are allowed, one per sub-module. No other hardware platform or computing hardware is accepted. |
| **Duty <= 90 %** | See §2. |
| **Circuits on breadboard** | Analog front ends are built on the supplied breadboard for sensor communication, behind the RYB shield's over voltage protection. |
| **One module per board** | heartbeat, sound, decision, motor are all separate boards with separate responsibilities and subteams. |

## 4. Hardware per board

- PYNQ-Z2 (Xilinx Zynq-7000)
- RYB shield PCB w/ overvoltage protection
- 1.54" LCD module (might or might not arrive by the first demo date.. Which is fine)
- Breadboard for analog circuitry
- Backbone connector

## 5. Signal constants

Authoritative values live in `include/ryb/ryb_config.h`. Repeated here for reading away from the code:

- PWM carrier **1 kHz** with an amplitude of **12 V**, must supply **>= 0.8 A**
- Duty ceiling **90 %**
- Heart rate **60–240 BPM**; stress **10–100 %**
- Crying volume is **constant for 50 < S < 100** and only falls off below S = 50
- ECG bonus work: sampling rate justified against **Nyquist** with a front end scaled to **0–3.3 V**

## 6. Booked slots

The physical setups and the digital twin are shared and available only in booked slots. This is why `docs/coding-standard.md` §1 exists: everything that *can* be tested on a laptop *must* be, so that slot time isn't wasted by your shit code having control flow or logic issues day-of.

Before a slot; know what you are testing, have it building, and have the host-side tests passing. Log what happened in `docs/test-log/` afterwards- including the failures! Don't lie, I can see when you don't submit any, and know its probably a lie.
