# heartbeat

**Owner: N/A**
**Board:** PYNQ-Z2 #1 | **Branch prefix:** `hb/`

Optical sensing of the wrist LED. Derives a BPM signature and general trend which is published by `ryb_heart_msg_t`.

## Responsibility

| In | Out |
|---|---|
| Photodetector signal from the wrist LED via the breadboard front end and the ADC | `ryb_heart_msg_t` — `bpm` (60..240, nolock defined as 0), `trend`, `valid` |

Also drives its own 1.54" LCD.

## Layout

```
src/main.c   init <= 50 lines
src/app/     peak detection, BPM averaging and trend prediction. Portable C, NO vendor headers.
src/hal/     ADC, timer, interconnect and the LCD driver
src/ui/      LCD rendering
include/     module-internal headers
test/        host tests for src/app/
```

`src/app/` must compile and pass tests with `gcc` on a laptop explained further in `docs/coding-standard.md` §1.

## Known issues / open questions

- Sensor is undecided (photodiode vs phototransistor and how gain stage will work).
- How will we mount the wrist sensor?
- Motion artefacts could potentially show up in the wrist sensor, how will we reject these?
