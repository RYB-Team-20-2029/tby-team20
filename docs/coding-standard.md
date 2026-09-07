# Coding Standard

Owner: management (`mgmt/`)

## 1. Portability

**Nothing under `src/app/` may include a vendor header.** No `xparameters.h`, no
`xil_*.h`, no `xgpio.h`, no `xadcps.h`, no BSP `sleep.h`. Nothing that only exists
inside Vitis.

```
src/hal/   talks to the board. Thin wrappers, no decisions.
src/app/   makes decisions. This is where the rule applies.
src/ui/    draws on the LCD.
main.c     wires the three together.
```

The reason is slot time. Six cradle setups are shared between 42 teams, so hardware
access is rare. If your BPM averaging can only run on the board, then a sign error in
it costs you a booked slot to find. Keep the logic portable and peak detection, BPM
averaging, matrix navigation and duty mapping all build and run with `gcc` on any
laptop, which leaves slot time for the things that genuinely need hardware: analog
front ends, timing and the backbone.

In practice:

- The HAL reads the ADC and hands `src/app/` a `uint16_t`. The application never
  learns that an ADC exists.
- The application decides the next `(f_region, a_region)`. It never learns that a PWM
  peripheral exists.
- The HAL turns a region into a duty cycle and programs the timer.
- If an application function needs the current time, it takes it as a parameter.

Both of these must come back empty:

```bash
grep -rn --include='*.c' --include='*.h' -E '#include\s*[<"](x|X)(parameters|il_|gpio|adc|uart|time|scu)' modules/*/src/app/
grep -rln --include='*.c' --include='*.h' -E 'xil_printf|XPAR_' modules/*/src/app/
```

A PR that adds a vendor include under `src/app/` gets sent back, including one added
for debugging. A debug include is how this rule dies.

## 2. Style

Formatting is `.clang-format`, applied by `./tools/format.sh`. Run it before you
commit. `./tools/format.sh --check` reports without writing.

- LLVM base, 4-space indent, 100 columns, pointers bind to the type
  (`uint8_t* p`, not `uint8_t *p`).
- Fixed-width integer types for anything crossing the wire or representing a hardware
  value: `uint16_t`, not `int`. Plain `int` is fine for a local loop counter.
- No dynamic allocation. No `malloc`, `calloc` or `strdup`. Buffers are static or
  automatic with a compile-time size. You should know your memory budget up front.
- No `float` in an interrupt handler or any timing-critical path. Use fixed point.
  Cradle frequencies are stored as centihertz in `ryb_config.h` for this reason.
- State machines are explicit. Use an enum and a `switch`, not a pile of
  `bool is_ready`, `bool has_started`, `bool was_valid`. Three flags are eight states,
  and five of them are ones you never thought about.
- Every file opens with a two-line header saying what it is for and who owns it:

```c
/* Purpose: sliding-window BPM average with outlier rejection.
 * Owning module: heartbeat. */
```

- `const` on anything that does not change. Lookup tables are `static const`.
- Braces always, including on a one-line `if`.
- Comments explain why, not what. `/* Reject the beat if it lands inside the
  refractory window. */` is useful; `/* increment i */` is not.

## 3. Include order

```c
/* two-line file header */

#include <stdint.h>        /* 1. C standard library               */

#include "ryb/ryb_ipc.h"   /* 2. shared RYB headers from upstream */

#include "hb_filter.h"     /* 3. this module's own headers        */
```

Vendor headers are a fourth group and appear only in `src/hal/` and `main.c`.

## 4. Naming

| Thing | Convention | Example |
|---|---|---|
| Type | `snake_case_t` | `ryb_heart_msg_t` |
| Function | `<module>_<verb>_<noun>` | `hb_compute_bpm` |
| Shared macro | `RYB_UPPER_SNAKE` | `RYB_PWM_DUTY_MAX_PCT` |
| File-local function | `static`, unprefixed | `static clamp_duty` |
| Module prefixes | `hb_`, `snd_`, `dec_`, `mot_` | |

Anything with external linkage carries its module prefix. Four boards' worth of code
lands in one repo, and two modules both defining `init()` is a collision waiting for
the integration branch.
