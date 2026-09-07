# sound

Board: PYNQ-Z2 #2 | Branch prefix: `snd/` | Owners: see `CODEOWNERS`

Microphone conditioning and loudness estimation. Publishes a loudness percentage and a
trend to the decision module.

| In | Out |
|---|---|
| Microphone signal, via the breadboard conditioning chain and the ADC | `ryb_sound_msg_t`: `loudness` (0..100 %), `trend`, `valid` |

Also drives its own 1.54" LCD: current loudness and trend.

`src/app/` holds envelope and level estimation, smoothing and trend, and must build and
pass its tests with `gcc` on a laptop. See `docs/coding-standard.md` §1.

## The knee

Crying volume is constant for 50 < S < 100 and only falls off below S = 50, named as
`RYB_STRESS_CRY_KNEE_PCT` in `ryb_config.h`. Resolution below the knee is the part that
matters; above it, this module cannot distinguish stress levels and should not pretend
otherwise.

## Open questions

- Microphone front end: pre-amp gain, rectification, filtering.
- Mapping the measured level onto the 0..50 scale needs calibrating against the real
  doll. Auto-calibration, possibly cross-checked against BPM, is worth considering.
- The cradle motor makes noise of its own. Do we need to reject it?
