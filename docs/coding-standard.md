# Coding Standard

**Owner:** management (`mgmt/`)
**Language:** C only. No C++, no Python, no HLS. This is a course constraint, not a preference.

---

## 1. The portability rule — the highest-priority standard in this repo

**Nothing under `src/app/` may include a vendor header.** That means no `xparameters.h`, no `xil_*.h`, no `xgpio.h`, no `xadcps.h`, no `sleep.h` from the BSP — nothing that only exists inside Vitis.

Application logic takes plain values in and returns plain values out. All hardware access is confined to `src/hal/`.

```
src/hal/   talks to the board. Thin wrappers, no decisions.
src/app/   makes decisions. Plain C, no board.
src/ui/    draws on the LCD.
main.c     wires the three together. Init plus super-loop, nothing else.
```

### Why this matters more than anything else here

There are six shared cradle setups for 42 teams. If your BPM averaging can only be tested on hardware, you can only test it in a booked slot — and you will spend that slot discovering an off-by-one you could have found on a laptop.

If it obeys this rule, peak detection, BPM averaging, matrix navigation and duty mapping all compile with `gcc` on any machine and can be tested at your desk at 2 a.m. Slot time is then spent on what genuinely needs the cradle: analog front ends, timing, and the backbone.

### What this looks like in practice

- HAL reads the ADC and hands `src/app/` a `uint16_t`. The app does not know an ADC exists.
- App decides the next `(f_region, a_region)`. It does not know a PWM peripheral exists.
- HAL turns a region into a duty cycle and programs the timer.
- If an app function needs the current time, it is **passed in** as a parameter. It does not call the BSP to find out.

### How it is checked

Both of these must come back empty:

```bash
grep -rn --include='*.c' --include='*.h' -E '#include\s*[<"](x|X)(parameters|il_|gpio|adc|uart|time|scu)' modules/*/src/app/
grep -rln --include='*.c' --include='*.h' -E 'xil_printf|XPAR_' modules/*/src/app/
```

A pull request that adds a vendor include under `src/app/` gets sent back. No exceptions, including "just for debugging" — a debug include is how this rule dies.

---

## 2. Style

Formatting is not a matter of taste here; it is `.clang-format`, and `./tools/format.sh` applies it. Run it before you commit. `./tools/format.sh --check` fails without writing, for when you want to know rather than change.

- **LLVM base, 4-space indent, 100-column limit, pointer binds to the type** (`uint8_t* p`, not `uint8_t *p`).
- **Fixed-width integer types** for anything crossing the wire or representing a hardware value: `uint16_t`, not `int`. Plain `int` is acceptable for a local loop counter and nowhere else.
- **No dynamic allocation.** No `malloc`, no `calloc`, no `strdup`. Buffers are static or automatic with a size known at compile time. Bare-metal with four boards and no MMU: a heap here is a fault you find during the demo.
- **No `float` in an interrupt handler or a timing-critical path.** Use fixed point. Cradle frequencies are stored in centihertz in `ryb_config.h` for exactly this reason.
- **All state machines explicit.** A named `enum` for the states, one `switch`, one variable holding the current state. No pile of `bool is_ready`, `bool has_started`, `bool was_valid` — three flags are eight states, of which you tested three.
- **Every file starts with a two-line header:** what it is for, and which module owns it.

```c
/* Purpose: sliding-window BPM average with outlier rejection.
 * Owning module: heartbeat. */
```

- **`const` on anything that does not change.** Lookup tables are `static const`.
- **Braces always**, even on a one-line `if`.
- **Comments say why.** The code already says what. `/* clamp: >90% duty trips the emergency light */` earns its place; `/* increment i */` does not.

---

## 3. Include order

```c
/* two-line file header */

#include <stdint.h>        /* 1. C standard library      */

#include "ryb/ryb_ipc.h"   /* 2. shared RYB headers       */

#include "hb_filter.h"     /* 3. this module's own headers */
```

Vendor headers form a fourth group and appear **only** in `src/hal/` and `main.c`.

## 4. Naming

| Thing | Convention | Example |
|---|---|---|
| Type | `snake_case_t` | `ryb_heart_msg_t` |
| Function | `<module>_<verb>_<noun>` | `hb_compute_bpm` |
| Shared macro | `RYB_UPPER_SNAKE` | `RYB_PWM_DUTY_MAX_PCT` |
| File-local function | `static`, unprefixed | `static clamp_duty` |
| Module prefixes | `hb_`, `snd_`, `dec_`, `mot_` | |

Anything with external linkage carries its module prefix. Four boards' worth of code lands in one repo; `init()` in two modules is a name collision waiting for the integration branch.
