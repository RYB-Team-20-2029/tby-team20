# motor

**Owner:**
**Board:** PYNQ-Z2 #4 · **Branch prefix:** `mot/` · **Status:** not started

Generates the two PWM drive signals to the cradle. Consumes `ryb_motion_cmd_t` and turns regions into duty cycles.

## Responsibility

| In | Out |
|---|---|
| `ryb_motion_cmd_t` from decision — `f_region`, `a_region` (each 1..5) | Two PWM signals: rocking frequency channel, rocking amplitude channel |

Also drives its own 1.54" LCD: current regions, current duty cycles.

## Drive parameters

Carrier **1 kHz**, amplitude **12 V**, must supply **>= 0.8 A**. Region mapping (from `ryb_config.h`):

| Region | Duty | Frequency | Amplitude |
|---|---|---|---|
| 1 | 0–10 % | 0.20 Hz | 20 % |
| 2 | 10–30 % | 0.35 Hz | 40 % |
| 3 | 30–50 % | 0.50 Hz | 60 % |
| 4 | 50–70 % | 0.65 Hz | 80 % |
| 5 | 70–90 % | 0.70 Hz | 100 % |

## The 90 % rule

**Duty must never exceed 90 %.** Above it the cradle trips its emergency light and the slot is over.

The clamp belongs in this module's HAL, at the last point before the value reaches the peripheral — a clamp upstream of an arithmetic bug does not clamp. Use `RYB_PWM_DUTY_MAX_PCT`; never retype `90`. Test it on the host with deliberately out-of-range inputs.

## Layout

```
src/main.c   init + super-loop only, target <= 50 lines
src/app/     region -> duty mapping, clamping. Portable C, NO vendor headers.
src/hal/     PWM timer, backbone, LCD driver
src/ui/      LCD rendering
include/     module-internal headers
test/        host tests for src/app/ — clamp behaviour first
```

## Known issues / open questions

- PWM output pins not yet assigned. `TODO(mot)` in `ryb_config.h`.
- Behaviour when commands stop arriving is undefined — hold last region, or ramp down? Open question 6 in `docs/interface-contract.md`. This one has a safety flavour; decide it deliberately.
- Whether region transitions should step or ramp. A hard jump from region 1 to region 5 is a mechanical shock to a shared setup.
- Current drive capability (>= 0.8 A) must be verified on the actual driver stage before the first cradle run.
