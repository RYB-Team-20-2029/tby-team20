# Safety and Constraints

Owner: management (`mgmt/`)

Safety rules protect people and shared equipment and are not negotiable.
Course constraints cost marks if broken, even if the prototype works.

## 1. Safety

**Electrical**

- Max 24 V AC / 42 V DC. Nothing in this project needs more. If you think you need more, you don't.
- Never bypass the cradle's emergency stop. Not even to check something.
- The cradle drive is 12 V at 0.8 A or more. It can hurt you and it can destroy hardware.

**The setup is shared.** Six complete setups for 42 teams. If we break something, we pay
for it. So: do not move setups, do not remove the doll, do not unplug mains, and do not
tape the wrist sensor on. Mounting it is a mechanical design problem.

**In the lab**

- Power down the boards before changing breadboard wiring.
- Check polarity before applying power. A reversed supply kills a PYNQ-Z2.
- If something smells hot, sounds wrong, or the emergency light comes on: stop, power
  down, and leave the hardware alone.

## 2. The 90 % duty ceiling

PWM duty above 90 % trips the cradle's emergency breaker and ends the run. This is the
one safety rule that code can enforce, so it is enforced in exactly one place:

- `RYB_PWM_DUTY_MAX_PCT` in `include/ryb/ryb_config.h` is the only definition of the
  number. Never retype `90`.
- The clamp lives in the motor module's HAL, at the last point before the value reaches
  the peripheral. A clamp sitting upstream of an arithmetic bug does not clamp. It does
  not belong in the decision module.
- Region 5 already tops out at 90 %. Any duty computed from something other than the
  region table needs its own explicit clamp.
- Test it on the host with out-of-range inputs before it ever runs on hardware.

## 3. Graded constraints

| Constraint | Detail |
|---|---|
| Backbone only | Boards communicate only through the supplied backbone. No extra wires, not even for a demo. |
| C only | No other language for the embedded software. |
| PYNQ-Z2 only | Four boards, one per module. No other platform. |
| Duty <= 90 % | See §2. |
| Circuits on breadboard | Analog front ends go on the supplied breadboard, behind the RYB shield's overvoltage protection. |
| One module per board | heartbeat, sound, decision and motor stay separate. |

## 4. Hardware per board

PYNQ-Z2 (Zynq-7000), RYB shield PCB with overvoltage protection, 1.54" LCD module
(may not arrive before Demo 1, which is fine), breadboard, backbone connector.

Electrical and signal constants live in `include/ryb/ryb_config.h`. Read them there
rather than copying them into a document that can drift.

## 5. Booked slots

The setups and the digital twin are only available in booked slots, which is the whole
reason for the portability rule in `docs/coding-standard.md` §1. Turn up knowing what
you are testing, with the code building and the host tests passing.

Afterwards, write up what happened in `docs/test-log/`, including the failures.
A slot with no failures logged reads as a slot with nothing logged.
