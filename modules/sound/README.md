# sound

**Owner: N/A**
**Board:** PYNQ-Z2 #2 | **Branch prefix:** `snd/`

Microphone conditioning and loudness estimation which generates and sends a loudness percentage and general trend over time, publishes them to the decision module as `ryb_sound_msg_t`.

## Responsibility

| In | Out |
|---|---|
| Microphone signal via the breadboard conditioning chain and the ADC | `ryb_sound_msg_t`, `loudness` (0..100 %), `trend`, `valid` |

Also drives its own 1.54" LCD: current loudness, trend.

## Layout

```
src/main.c   init target <= 50 lines
src/app/     envelope/level estimation, smoothing and general trend. Portable C, NO vendor headers.
src/hal/     ADC, timer, backbone and the LCD driver
src/ui/      LCD rendering
include/     module-internal headers
test/        host tests for src/app/
```

## Maybe consider when publishing

The doll's crying volume is constant for 50 < S < 100 and only falls off below S = 50

- Do not report "loudness unchanged" as "no progress", these are not valid program states for this submodule!
- Resolution below S = 50 is the only part thats practically important, so the resolution only needs to be reliable between 0 - 50.

`RYB_STRESS_CRY_KNEE_PCT` in `ryb_config.h` names the knee.

## Known issues / open questions

- Microphone input not decided (pre-amp gain, rectification, filtering).
- Mapping from measured level to the 0..50 scale needs calibration against the actual doll. Consider making the calibration auto adjusting / relational to the BPM.
- The cradle motor itself makes noise, maybe we might need pre-conditioning for motor noise rejection?
