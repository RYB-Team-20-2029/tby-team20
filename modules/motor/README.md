# motor

Owner: unassigned | Board: PYNQ-Z2 #4 | Branch prefix: `mot/`

Turns region commands into the two PWM drive signals for the cradle.

| In | Out |
|---|---|
| `ryb_motion_cmd_t` from decision: `f_region`, `a_region` (each 1..5) | Two PWM signals: rocking frequency, rocking amplitude |

Also drives its own 1.54" LCD: current regions and duty cycles.

## Drive parameters

Carrier 1 kHz, amplitude 12 V, at least 0.8 A. Region mapping, from `ryb_config.h`:

| Region | Duty | Frequency | Amplitude |
|---|---|---|---|
| 1 | 0-10 % | 0.20 Hz | 20 % |
| 2 | 10-30 % | 0.35 Hz | 40 % |
| 3 | 30-50 % | 0.50 Hz | 60 % |
| 4 | 50-70 % | 0.65 Hz | 80 % |
| 5 | 70-90 % | 0.70 Hz | 100 % |

The 90 % duty clamp belongs to this module and lives in its HAL. The rules for it are
in `docs/safety-and-constraints.md` §2, and they are not optional.

`src/app/` holds the region-to-duty mapping and clamping, tested on the host with
out-of-range inputs first. See `docs/coding-standard.md` §1.

## Open questions

- PWM output pins are not assigned. `TODO(mot)` in `ryb_config.h`.
- What to do when commands stop arriving: hold the last region, or ramp down? Open
  question 6 in `docs/interface-contract.md`. This one has a safety flavour, so decide
  it deliberately.
- Whether region transitions step or ramp. Jumping from region 1 to region 5 is a
  mechanical shock to a shared setup.
- Drive capability (>= 0.8 A) has to be verified on the actual driver stage before the
  first cradle run.
