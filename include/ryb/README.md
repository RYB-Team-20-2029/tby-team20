# RYB Comms

The 13-bit packet link between the four boards, implemented to
[`RYB_Comms_Protocol_Guideline.txt`](RYB_Comms_Protocol_Guideline.txt): MSB first, start bit,
even parity, idle-high open-drain line, driven through the AXI GPIO behind the PYNQ-Z2 Arduino
header via `/dev/mem`. No library beyond libc and pthreads.

Pins, addresses and timing live in [`ryb_comms.json`](ryb_comms.json). CMake reads it and
generates a header per module, so editing the JSON and rebuilding is all a change takes.

## Writing a module

Include one header and call three functions:

```c
#include "ryb/ryb.h"

int main(void)
{
    ryb_begin();                    /* comms + LCD, pins and timing from ryb_comms.json */
    for (;;) {
        ryb_update();               /* runs your receive callbacks; refreshes the LCD */
        /* your module's work, then send its result: */
        ryb_send_appended(bpm, ryb_trend(previous, bpm));   /* heartbeat, sound */
        /* or ryb_send_command(f_region, a_region);            decision */
        usleep(10000);
    }
}
```

Plus one function per sender your module listens to. The library calls it from inside
`ryb_update()`:

| Module | Defines | Sends |
|---|---|---|
| heartbeat | nothing | `ryb_send_appended(bpm, trend)` |
| sound | `void snd_receive_heartbeat(uint8_t bpm, ryb_trend_t trend)` | `ryb_send_appended(volume, trend)` |
| decision | `void dec_receive_heartbeat(uint8_t bpm, ryb_trend_t trend)`<br>`void dec_receive_sound(uint8_t volume, ryb_trend_t trend)` | `ryb_send_command(f_region, a_region)` |
| motor | `void mot_receive_command(uint8_t first, uint8_t second)` | nothing |

Complete dummy programs are in [`examples/`](examples/): copy `examples/<module>_main.c` to
`modules/<module>/src/main.c` and replace the parts marked DUMMY. `cmake --build build --target
<module>_example` builds one as it stands.

What happens underneath:

- **Sending** queues the packet and returns at once. A background thread clocks it out on the
  TX pin with the module's address and parity added. Send whenever there is a new value, about
  once a second for sensors. Repeating a command is fine.
- **Receiving** happens in the same background thread, all the time, so nothing is lost while
  your loop is busy (it buffers 128 packets). The packets wait until your next `ryb_update()`.
  That call runs your callbacks on your own thread, so callbacks and main loop can share plain
  variables without locks. Keep callbacks short: store the value, act on it in the loop.
- **Repeating:** sound passes heartbeat's packets on to decision by itself, because the JSON
  marks it a repeater. Its callback is only so sound can look at them.
- **Checks:** packets with a parity error, an unexpected sender or a bad trend are dropped before
  any callback sees them. Range checks on the values are still the module's job.
- **The LCD** shows the last packet from each sender plus what this board sent, refreshed by
  `ryb_update()`. A module that wants its own screen uses `ryb_lcd_text()` instead.
- **Pins, addresses, timing and the callback names** come from `ryb_comms.json` at build time.
  The configure step prints the callbacks the module must define, and a missing one is a link
  error naming it.

## Preset zips: hand-wired chain

`tools/make_preset_zips.ps1` (Windows PowerShell) builds `dist/ryb_<module>.zip` from
`presets/manual_chain.json`. In that preset every board sends on AR1 and receives on AR2, and
`line.wiring` is `point_to_point`. Wire each board's AR1 to the next board's AR2, plus GND. The
monitors send a random value every 5 s. On a board:

```bash
cd ~ && unzip -o ryb_decision.zip && bash install_decision.sh
```

The script prints the TX and RX pins, builds, and starts the `ryb-comms` service at every boot.

## Demo day: wiring up the backbone

Every board needs the repository in `~/tby-team20` and one deploy, so that `build/ryb_pinfind`
exists. Then:

1. **Map the connector boards.** Seat the boards in the backbone. On one board run
   `./tools/pinfind.sh talk`. On every other board run `./tools/pinfind.sh listen 30`. Each
   listener prints lines like `RBPI29 <- talker's RBPI29`: that pin on this board is wired to
   that pin on the talker. Press Ctrl-C on the talker when the listeners are done. If the
   backbone links neighbours rather than sharing every line, let each board talk once.
   Never run two talkers at once. Never leave a board running `ryb-comms` during the search:
   the script stops it on the board it runs on and starts it again afterwards.
2. **Write the chain once.** On one board run `./tools/deploy.sh chain`. It asks for each hop
   (heartbeat → sound → decision → motor) the pin on the sending board and on the receiving
   board, then the display, the repeater and the line settings. It writes all four modules into
   `ryb_comms.json` and deploys that board.
3. **Copy and deploy the rest.** Copy `include/ryb/ryb_comms.json` to the other boards, for
   example with `scp include/ryb/ryb_comms.json student@<board>:tby-team20/include/ryb/`. On each
   one run `./tools/deploy.sh <module> --quick`, or just `./tools/deploy.sh --quick` on a board
   that has been deployed before (it remembers its module in `~/.ryb-board`). That takes about
   30 s, with no questions, and leaves it running at boot.
4. **Check the screens.** The bottom row shows each board's own pins. Each sender's block should
   count up with a green age, and the error triple should stay `0/0/0`. Until the modules' own
   `main.c` exist, the monitors send test values, so data flows through the whole chain anyway.

## Deploy a board

Copy the repository onto the board and run:

```bash
./tools/deploy.sh
```

It asks which module the board runs, where the display sits, which pin it sends on, and which
pins it receives on and from whom. It writes the answers into `ryb_comms.json` (backup in
`ryb_comms.json.bak`), builds, and can install a `ryb-comms` service that starts at boot, after
the overlay is loaded. Pins can be given as `AR3` or as a Raspberry Pi header pin number such as
`29`. Re-run it to change anything. Answers can also be piped in, one per line.

Each build includes `<module>_monitor`, which runs the comms for that module without any module
code. It shows received packets on the LCD and the console and sends a test value every 500 ms.
This is enough to bring up and check a chain before the modules' own `main.c` exist:

```bash
sudo ./build/decision_monitor          # -p <ms> send period (0: never), -t <s>, -q quiet, -n no LCD
journalctl -u ryb-comms -f             # when it runs as the service
```

## Build and run on the board by hand

```bash
cmake -B build -DRYB_MODULE=decision     # heartbeat, sound, decision or motor
cmake --build build
sudo ./build/decision                    # root for /dev/mem
```

This builds every `.c` under `modules/<name>/src/` into one program. Needs CMake 3.19 or newer
for its JSON reader; if the board's is older, `sudo pip3 install cmake`. If a module needs extra
libraries (an LCD driver, say), put the `target_link_libraries(${RYB_MODULE_TARGET} ...)` lines in
`modules/<name>/module.cmake` instead of editing the shared CMake files.

Host tests, on any Linux or macOS laptop (on Windows only the codec test builds):

```bash
cmake -B build && cmake --build build && cd build && ctest
```

## What your module must define

The library calls one function per sender your module listens to. The name comes from your
module and the sender: `<prefix>_receive_<sender>`, or `<prefix>_receive_command` for the
decision module's command packets. Define them in `modules/<name>/src/main.c`. If one is
missing, the link fails and names it. With the current `ryb_comms.json`:

| Module | Define in `main.c` | Send with |
|---|---|---|
| heartbeat | nothing, it has no rx link | `ryb_send_appended(bpm, trend)` |
| sound | `void snd_receive_heartbeat(uint8_t bpm, ryb_trend_t trend)` | `ryb_send_appended(volume, trend)` |
| decision | `void dec_receive_heartbeat(uint8_t bpm, ryb_trend_t trend)`<br>`void dec_receive_sound(uint8_t volume, ryb_trend_t trend)` | `ryb_send_command(first, second)` |
| motor | `void mot_receive_command(uint8_t first, uint8_t second)` | nothing: the chain ends here |

Change a module's `rx` list and the set changes with it. The configure step prints the current
list for the module being built:

```
-- RYB Comms ryb_comms_decision: decision at address 2, tx AR4, rx AR3, receive callbacks to define: dec_receive_heartbeat, dec_receive_sound
```

Each build declares only its own send function. The decision module gets `ryb_send_command`
and everyone else gets `ryb_send_appended`, so calling the wrong one is a compile error.

A decision `main.c` looks like this:

```c
#include <stdint.h>

#include "ryb/ryb_comms.h"

void dec_receive_heartbeat(uint8_t bpm, ryb_trend_t trend)
{
    /* store it for the main loop, show it on the LCD */
}

void dec_receive_sound(uint8_t volume, ryb_trend_t trend)
{
}

int main(void)
{
    if (ryb_comms_init() != RYB_OK) {
        return 1;
    }
    for (;;) {
        ryb_comms_poll(); /* runs the callbacks above for whatever arrived */
        /* compute the next (f, a), then: */
        ryb_send_command(3u, 2u); /* each 0..31 */
        /* refresh the LCD: the bit loop has its own thread, so this cannot stretch a bit */
    }
}
```

## API

| Call | Does |
|---|---|
| `ryb_comms_init()` | Maps the GPIO, sets up the pins, starts the bit loop thread. |
| `ryb_comms_poll()` | Runs your callbacks for every packet received since the last call, on your thread. Returns how many. |
| `ryb_send_appended(data, trend)` | Queues an appended packet with this module's address. Not on decision. |
| `ryb_send_command(first, second)` | Queues a command packet, each segment 0..31. Decision only. |
| `ryb_comms_get_stats(&s)` | Packet and error counters. Climbing parity errors mean the bit period is too short or the lane has no pull-up. |
| `ryb_comms_shutdown()` | Stops the loop and releases the line. Also runs at `exit()`. |

Sends are queued and clocked out in the background. `RYB_ERR_BUSY` means you are sending faster
than the line carries: one packet takes 14 bit periods, 7 ms at the default 500 us.

`ryb_packet.h` is the pure codec (`ryb_pack_appended`, `ryb_pack_command`, `ryb_frame_decode`).
It has no OS dependencies, so module code under `src/app/` can use it and still build on a laptop.

## `ryb_comms.json`

| Field | Meaning |
|---|---|
| `gpio.base_address` | AXI GPIO base address in your overlay, as a hex string. |
| `gpio.direction_settle_us` | Wait after configuring pin directions at start-up. |
| `line.bit_period_us` | Bit time. Every board must use the same value. |
| `line.drive` | `push_pull` (default: one sender per lane, no pull-up needed; CMake refuses two modules sending on one pin) or `open_drain` (the guideline's idle-high lane, needs an external pull-up, see below). |
| `line.idle_sleep_us` | Longest the bit loop sleeps while waiting for a start edge. `idle_sleep_us + 80` must fit in half a bit; 0 spins at 100 % CPU. |
| `thread.realtime` / `priority` / `cpu` | Run the bit loop `SCHED_FIFO`, pinned to a core. Falls back to normal priority without root. |
| `pins` | Arduino header pin name to GPIO bit. Bits 32..63 are channel 2. |
| `modules.<name>.address` | Sender address. CMake warns if it differs from the guideline's table. |
| `modules.<name>.repeater` | Forward every valid received packet, ahead of this module's own. |
| `modules.<name>.tx` | Pin this module sends on, or `null`. |
| `modules.<name>.rx` | List of `{ "pin": ..., "from": [senders] }`. A packet from a sender not listed for that link is dropped, per the guideline's check 3. |

## How it behaves

- **Receiver.** Arms on a falling edge that holds for 10 us, so a noise spike a few
  microseconds wide is not taken for a start bit. Each bit, the start bit included, is read three
  times, an eighth of a bit apart around its middle, and the majority wins. If the line is still
  low where the next slot begins, it treats that as a back-to-back start bit, which has no edge
  after a parity of 0. Checks run in the guideline's order: parity, then sender, then trend.
- **Recovery.** After a rejected frame, and at start-up, it waits until the line has been high for
  one packet time before it trusts another edge. That is the guideline's idle rule. So that a
  saturated link still offers that idle time, a sender pauses for 16 bit times after every 4
  back-to-back packets. That limits throughput to about 78 % of the line rate.
- **Timing.** The bit loop sleeps between packets, and inside a packet until 60 us before the
  next bit edge, so it stays well below the CPU level where Linux RT throttling would freeze it
  mid-packet. A start edge that arrives during a sleep is timed at the middle of the window in
  which it could have fallen.
- **Shared lane.** A module that hears its own address on an rx pin ignores it silently, so a
  pin can be both `tx` and `rx` in `open_drain` mode.
- **Line release.** On `exit()`, SIGINT or SIGTERM the tx pin is released. Otherwise a killed
  program could leave the lane pulled low for every board on it. If your module installs its own
  SIGINT handler, call `ryb_comms_shutdown()` from it.

## Measured on a PYNQ-Z2

TU/e image 0.2.0 with the `io0.2.0` overlay. Its AXI GPIO is at `0x41200000`, the
`io_switch` routes every header pin to GPIO after boot, and AR0..AR9 are GPIO bits 0..9 (AR1 and
AR2 confirmed with a jumper). Bits 10 to 13 belong to the LCD, and where AR10..AR13 land is not
known yet.
Measured with a loopback on one pin, in push-pull, 1000 to 8000 packets per run:

| Bit period | Idle sleep | Packets intact | Errors (all caught) | CPU |
|---|---|---|---|---|
| 500 us | 20 us | 100 % | 0 | 33 % |
| 200 us | 20 us | 100 % | 0 | 59 % |
| 100 us | 0 (spin) | 99.6 % | 5 parity | 98 % |
| 50 us | 0 (spin) | 99.8 % | 7 parity | 98 % |

A released Arduino pin took 6 to 9 ms to read high again. The board's own pull-up is far too weak
for `open_drain`, which is why `push_pull` is the default.

Through a jumper wire from AR1 to AR2, which picks up spikes a few microseconds wide, single
reads per bit lost 0.3 to 1.2 % of packets to caught parity errors. With the three-read vote and
the debounced start edge: 2000 of 2000 at 500 us, 2000 of 2000 at 200 us, and none delivered
corrupt.

## LCD

`ryb_lcd.h` drives the 1.54" display on the Raspberry Pi header with the same wiring as the TU/e
image's own display code: `axi_quad_spi_1`, MOSI, clock and chip select routed through the
`io_switch` to pins 30, 31 and 13, and backlight, reset and data/command on GPIO bits 10 to 12.
It draws text with the image's `/boot/ILGH16XB.FNT`. CMake refuses comms pins on those bits.

`ryb_lcd_show_comms()` fills the screen with what the guideline asks every board to show. For
each sender this module listens to, it shows the address, value and trend of the last packet,
its raw bits split into fields, how many arrived, and how long ago (red after 2 s). Above that are
the link counters and this module's own last send. Call it from the main loop:

```c
ryb_comms_init();
ryb_lcd_init();                 /* about 1.6 s; returns RYB_ERR_IO on the host */
for (;;) {
    ryb_comms_poll();
    /* ... */
    ryb_lcd_show_comms();       /* only redraws what changed, a few ms when little did */
    usleep(200000);
}
```

`ryb_lcd_text()` and `ryb_lcd_clear()` are there for a module's own screen.

## Still to decide

These are placeholders in `ryb_comms.json`, tracked as open items in the guideline:

- `gpio.base_address` and the `pins` bits are correct for the TU/e `io0.2.0` overlay (see
  above). With another overlay, read them from its address map. If something reroutes the
  `io_switch` (libpynq's `switchbox_set_pin`, say), put the comms pins back on GPIO.
- Which pin each hop uses. The chain is one way, heartbeat → sound → decision → motor, and the
  default puts its three links on AR2, AR3 and AR4, with sound as the repeater. The motor only
  receives, so its `tx` is `null`.
- Whether to fit pull-ups (about 10 kOhm to 3.3 V per lane) and move to the guideline's
  `open_drain`. Until then, `push_pull` needs exactly one sender per conductor.
- `line.bit_period_us`: 500 us has margin to spare. 200 us also ran clean, but that was a
  loopback on one board. Confirm it between two boards before going lower.
