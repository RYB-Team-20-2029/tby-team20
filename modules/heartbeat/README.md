# heartbeat

**Owner:**
**Board:** PYNQ-Z2 #1 · **Branch prefix:** `hb/` · **Status:** not started

Optical sensing of the wrist LED. Produces a beats-per-minute figure and a trend, and publishes them to the decision module as `ryb_heart_msg_t`.

## Responsibility

| In | Out |
|---|---|
| Photodetector signal from the wrist LED, via the breadboard front end and the ADC | `ryb_heart_msg_t` — `bpm` (60..240, 0 = no lock), `trend`, `valid` |

Also drives its own 1.54" LCD: current BPM, trend, lock state.

## Layout

```
src/main.c   init + super-loop only, target <= 50 lines
src/app/     peak detection, BPM averaging, trend. Portable C, NO vendor headers.
src/hal/     ADC, timer, backbone, LCD driver
src/ui/      LCD rendering
include/     module-internal headers
test/        host tests for src/app/
```

`src/app/` must compile and be testable with `gcc` on a laptop — see `docs/coding-standard.md` §1. Peak detection and BPM averaging are exactly the kind of thing you do not want to be debugging in a booked slot.

## Known issues / open questions

- Sensor front-end topology is undecided (photodiode vs phototransistor, gain stage). Open question 3 in `docs/interface-contract.md`.
- Wrist sensor mounting: **adhesive tape is forbidden**. A mechanical solution is needed.
- Publication rate not yet agreed; it sets the trend-window length. Open question 2.
- Motion artefacts from the rocking cradle will show up in the optical signal. Worth planning for early.
