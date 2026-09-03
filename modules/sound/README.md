# sound

**Owner:**
**Board:** PYNQ-Z2 #2 · **Branch prefix:** `snd/` · **Status:** not started

Microphone conditioning and loudness estimation. Produces a loudness percentage and a trend, and publishes them to the decision module as `ryb_sound_msg_t`.

## Responsibility

| In | Out |
|---|---|
| Microphone signal, via the breadboard conditioning chain and the ADC | `ryb_sound_msg_t` — `loudness` (0..100 %), `trend`, `valid` |

Also drives its own 1.54" LCD: current loudness, trend.

## Layout

```
src/main.c   init + super-loop only, target <= 50 lines
src/app/     envelope/level estimation, smoothing, trend. Portable C, NO vendor headers.
src/hal/     ADC, timer, backbone, LCD driver
src/ui/      LCD rendering
include/     module-internal headers
test/        host tests for src/app/
```

## The quirk that shapes this module

The doll's crying volume is **constant for 50 < S < 100** and only falls off below S = 50. Loudness alone cannot resolve high stress levels. Two consequences:

- Do not report "loudness unchanged" as "no progress" — above the knee it is the expected reading.
- Resolution below S = 50 is where this module earns its keep. Spend the effort on the low end.

`RYB_STRESS_CRY_KNEE_PCT` in `ryb_config.h` names the knee.

## Known issues / open questions

- Microphone front-end topology undecided (pre-amp gain, rectification, filtering). Open question 3 in `docs/interface-contract.md`.
- Mapping from measured level to the 0..100 scale needs calibration against the actual doll, which needs a booked slot. Design it so the calibration is a constant, not a rewrite.
- The cradle motor itself makes noise, and it changes with F and A. Consider how to avoid tracking the motor instead of the baby.
- Publication rate not yet agreed. Open question 2.
