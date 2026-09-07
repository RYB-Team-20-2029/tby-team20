# Interface Contract

Owner: management (`mgmt/`)

`include/ryb/ryb_ipc.h` is authoritative. This file is the same thing in prose; if the
two disagree, the header is right and this file is the bug.

Current version: `RYB_IPC_VERSION = 0x0001`

All four boards have to agree on one wire format, so any change to `include/ryb/` is a
PR against a management-owned path and every affected module owner acknowledges it
before merge. Do not fork the header, copy it into a module, or add a field
"temporarily" on a branch. Bump `RYB_IPC_VERSION` on any layout change: new field,
reordered field, changed width, or changed meaning of a value.

## Messages

### `ryb_heart_msg_t`, heartbeat to decision

| Field | Type | Range | Meaning |
|---|---|---|---|
| `bpm` | `uint16_t` | 60..240, or 0 | Beats per minute. `0` means no lock: warm-up, wrist moved, LED occluded. |
| `trend` | `uint8_t` | 0, 1, 2 | Over the heartbeat module's own trend window. |
| `valid` | `uint8_t` | 0, 1 | 0 means ignore the whole message. |

`valid == 0` and `bpm == 0` say different things. `valid` is about the message, `bpm`
is about the signal. Sending `valid = 1, bpm = 0` means "I am alive and I currently
cannot see a heartbeat", which the decision module needs to know.

### `ryb_sound_msg_t`, sound to decision

| Field | Type | Range | Meaning |
|---|---|---|---|
| `loudness` | `uint8_t` | 0..100 | Crying loudness as a percentage of full scale. |
| `trend` | `uint8_t` | 0, 1, 2 | Over the sound module's own trend window. |
| `valid` | `uint8_t` | 0, 1 | 0 means ignore the whole message. |

Crying volume is constant for 50 < S < 100 and only falls off below S = 50, so
loudness cannot separate high stress levels on its own. Above that knee, unchanged
loudness does not mean no progress.

### `ryb_motion_cmd_t`, decision to motor

| Field | Type | Range | Meaning |
|---|---|---|---|
| `f_region` | `uint8_t` | 1..5 | Rocking-frequency region. |
| `a_region` | `uint8_t` | 1..5 | Rocking-amplitude region. |

Decision sends regions, never duty cycles. The region-to-duty translation and the 90 %
clamp live in the motor module and nowhere else.

## Rules for every message

- Trend encoding is `0 = falling, 1 = stable, 2 = rising`, shared by both sensing
  modules. `ryb_trend_t` in `ryb_types.h` names the values, but the wire fields are
  `uint8_t`: enum width is implementation-defined and must not cross the wire.
- Fixed-width types only.
- Producers clamp to the declared range. Consumers still range-check, because a wire
  is not a promise.
- No dynamic allocation anywhere in the path.
- A consumer that cannot parse a message drops it and reports `RYB_ERR_IPC`. It does
  not guess.

## Open questions

Each needs an owner and a decision before Demo 1 on 15 Oct 2026.

| # | Question | Blocks |
|---|---|---|
| 1 | Physical link type on the backbone: which lines we use and the signalling on them. | Everything. Highest priority. |
| 2 | Update rates: how often each sensing module publishes, and whether decision polls or is pushed to. | Trend window length, decision timing |
| 3 | Sensor front ends: photodiode vs phototransistor and gain stage for heartbeat, conditioning chain for sound. | Breadboard layout, ADC scaling |
| 4 | Byte order and struct packing. The three structs happen to be naturally aligned with no padding on this toolchain, but nothing has been decided. Free while all four boards are identical, not free the moment anything is serialised byte-wise. | Framing |
| 5 | Framing: does a header carry `RYB_IPC_VERSION` and a sender id? Is there a checksum? | `ryb_ipc.h` |
| 6 | Failure behaviour: what the motor does when commands stop arriving. Hold the last region, or ramp down? | Motor, safety |

## Change log

| Version | Date | Change | Acknowledged by |
|---|---|---|---|
| `0x0001` | 2026-09-03 | Initial contract: `ryb_heart_msg_t`, `ryb_sound_msg_t`, `ryb_motion_cmd_t`. Framing deliberately unspecified. | management (mgmt/) |
