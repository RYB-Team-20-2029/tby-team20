# heartbeat

Board: PYNQ-Z2 #1 | Branch prefix: `hb/` | Owners: see `CODEOWNERS`

Optical sensing of the wrist LED. Derives a BPM and a trend, publishes them to the
decision module.

| In | Out |
|---|---|
| Photodetector signal from the wrist LED, via the breadboard front end and the ADC | `ryb_heart_msg_t`: `bpm` (60..240, 0 = no lock), `trend`, `valid` |

Also drives its own 1.54" LCD.

`src/app/` holds peak detection, BPM averaging and trend estimation, and must build and
pass its tests with `gcc` on a laptop. Directory layout and the rules behind it are in
`docs/coding-standard.md` §1.

## Open questions

- Sensor choice: photodiode or phototransistor, and how the gain stage works.
- How to mount the wrist sensor. Tape is not allowed.
- Motion artefacts in the wrist signal. How do we reject them?
