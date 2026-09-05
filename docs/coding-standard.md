# Coding Standard

**Owner:** coordination (`mgmt/`)
---

## 1. Portability

**Nothing under `src/app/` may include a vendor header.** THat includes, and means no `xparameters.h`, no `xil_*.h`, no `xgpio.h`, no `xadcps.h`, no `sleep.h` from the BSP. Nothing that an ordinary laptop is unable to run or which only exists inside Vitis.

Application logic should take plain values in and return plain values out such that all hardware access is confined to `src/hal/`.

Just as a general rule of thumb to follow:
```
src/hal/   talks to the board. Thin wrappers, no decisions or logic.
src/app/   makes decisions. (this is where this rule has to apply)
src/ui/    draws on the LCD. (if required / optional)
main.c     wires the three together, treat as the entry-point for your sub team's program.
```

### Why should you care at all

Just as an example, if your BPM averaging can only be tested on hardware, you can only test it in a booked slot with the real hardware. Safe to say we arent going to have a bunch of time getting hands on with the hardware since the baby and cradle are shared between multiple teams.

So unless you want to spend alot of time probing outputs and verifying against vendor docs, only to discover that you messed up a mathematical operation somewhere in the middle of a booked slot for hardware.. That would really suck.

If it obeys this rule, peak detection, BPM averaging, matrix navigation and duty mapping all compile with `gcc` on any machine and can be tested on your laptop or on the PYNQ at any time! Then the time we spend on analog front ends, timing, and the backbone will be the only times we will need the full hardware.

### What this looks like

- HAL reads the ADC and hands `src/app/` a `uint16_t`. The application does not know an ADC exists
- Application decides the next `(f_region, a_region)`. It does not know a PWM peripheral exists
- HAL turns a region into a duty cycle and programs the timer
- If an application function needs the current time, it is passed in as a parameter, specifically does not call the BSP to find out

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

- LLVM base w/ 4-space indent and a 100-column limit, pointers should bind to the type (`uint8_t* p`, NOT `uint8_t *p`).
- Exclusively use Fixed-width integer types for anything going into the communication API or representing a hardware value. `uint16_t` instead of `int`. Plain `int` is acceptable for a local loop counter and will cause a PR reject if you attempt to merge.
- No amount of dynamic allocation is acceptable. No `malloc`, no `calloc`, no `strdup`. Buffers should be static or automatic with a size known at compile time. You should know how much memory you need at the start of the program and should be able to calculate it.
- No `float` in an interrupt handler or anything critical to time. Always use fixed point. Cradle frequencies are stored as centihertz in `ryb_config.h` in similar vain (1/100th of a hertz).
- All state machines explicit, if I see your ass using Java-esque isRunning = True and other BS your PR will be rejected. JUST USE JUMPS! That is completely legal! NO pile of `bool is_ready`, `bool has_started`, `bool was_valid` three flags are eight states!!
- Every file starts with a two-line header that describes what it is for and which module owns it.

```c
/* Purpose: sliding-window BPM average with outlier rejection.
 * Owning module: heartbeat. */
```

- `const` on anything that does not change explicitly, Lookup tables are `static const`.
- Braces always, This also applies on a one-line `if` statements.
- Don't write useless comments, the code already shows what's happening. The comments should answer why you wrote what you did. `/* Calculate baby stress from heartbeat */` is fine; `/* increment i */` is not at all useful.

---

## 3. Include order

```c
/* two-line file header */

#include <stdint.h>        /* 1. C standard library               */

#include "ryb/ryb_ipc.h"   /* 2. shared RYB headers from upstream */

#include "hb_filter.h"     /* 3. this module's own headers        */
```

Vendor headers are technically a group and appear only in `src/hal/` and `main.c`, you are not allowed to edit them if you are in an assigned sub-team or bring forward a change that will be approved by coordination.

## 4. Naming

| Thing | Convention | Example |
|---|---|---|
| Type | `snake_case_t` | `ryb_heart_msg_t` |
| Function | `<module>_<verb>_<noun>` | `hb_compute_bpm` |
| Shared macro | `RYB_UPPER_SNAKE` | `RYB_PWM_DUTY_MAX_PCT` |
| File-local function | `static`, unprefixed | `static clamp_duty` |
| Module prefixes | `hb_`, `snd_`, `dec_`, `mot_` | |

Anything with external linkage carries its module prefix. Four boards' worth of code lands in one repo; `init()` in two modules is a name collision waiting for the integration branch.
