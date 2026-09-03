# Interface Contract

**Owner:** management (`mgmt/`)
**Authoritative source:** `include/ryb/ryb_ipc.h` — this document mirrors it in prose. If the two ever disagree, the header wins and this file is the bug.

Current version: **`RYB_IPC_VERSION = 0x0001`**

## Why this file is owned by management

All four boards must agree on one wire format. Any change to `include/ryb/` is a pull request against a management-owned path, and **every affected module owner must acknowledge it before merge**. Do not fork the header, do not copy it into a module, do not "temporarily" add a field on a branch. A private edit that reaches hardware costs the team a booked test slot.

Bump `RYB_IPC_VERSION` on any layout change — new field, reordered field, changed width, changed meaning of a value.

## Messages

### `ryb_heart_msg_t` — heartbeat → decision

| Field | Type | Range | Meaning |
|---|---|---|---|
| `bpm` | `uint16_t` | 60..240, or 0 | Beats per minute. `0` means *no lock*: the sensor has no usable signal (warm-up, wrist moved, LED occluded). |
| `trend` | `uint8_t` | 0, 1, 2 | 0 falling, 1 stable, 2 rising — over the heartbeat module's own trend window. |
| `valid` | `uint8_t` | 0, 1 | 0 = ignore this whole message. |

`valid == 0` and `bpm == 0` are different statements. `valid` is about the message; `bpm == 0` is about the signal. A module may send `valid = 1, bpm = 0` to say "I am alive and I currently cannot see a heartbeat" — which is information the decision module needs.

### `ryb_sound_msg_t` — sound → decision

| Field | Type | Range | Meaning |
|---|---|---|---|
| `loudness` | `uint8_t` | 0..100 | Crying loudness as a percentage of full scale. |
| `trend` | `uint8_t` | 0, 1, 2 | 0 falling, 1 stable, 2 rising. |
| `valid` | `uint8_t` | 0, 1 | 0 = ignore this whole message. |

Note the physical quirk: the doll's crying volume is **constant for 50 < S < 100** and only falls off below S = 50. Loudness therefore cannot distinguish high stress levels on its own, and the decision module must not treat "loudness unchanged" as "no progress" above that knee.

### `ryb_motion_cmd_t` — decision → motor

| Field | Type | Range | Meaning |
|---|---|---|---|
| `f_region` | `uint8_t` | 1..5 | Rocking-frequency region. |
| `a_region` | `uint8_t` | 1..5 | Rocking-amplitude region. |

The motor module maps regions to duty cycles using the table in `ryb_config.h` and **clamps every output at 90 % duty**. Exceeding 90 % trips the cradle emergency light. The decision module sends regions, never duty cycles — that translation lives in exactly one place.

## Trend encoding

`0 = falling, 1 = stable, 2 = rising`, shared by both sensing modules. The enum `ryb_trend_t` in `ryb_types.h` names these values, but the wire fields are declared `uint8_t`: enum width is implementation-defined and must not cross the wire.

## Rules that apply to every message

- Fixed-width types only (`uint8_t`, `uint16_t`) for anything crossing the wire.
- Producers clamp to the declared range. Consumers must still range-check — a wire is not a promise.
- No dynamic allocation anywhere in the path.
- A consumer that receives a message it cannot parse drops it and reports `RYB_ERR_IPC`; it does not guess.

## Open questions

Undecided. Each needs an owner and a decision before Demo 1 (15 Oct 2026).

| # | Question | Blocks | Status |
|---|---|---|---|
| 1 | **Physical link type on the backbone** — which of the supplied backbone's lines we use, and the signalling on them. | Everything. Highest priority. | Open |
| 2 | **Update rates** — how often each sensing module publishes, and whether the decision module polls or is pushed to. Sets the trend-window length. | Trend semantics, decision timing | Open |
| 3 | **Sensor front-end topology** — photodiode/phototransistor and gain stage for heartbeat; microphone conditioning chain for sound. | Breadboard layout, ADC scaling | Open |
| 4 | **Byte order and struct packing** — the three structs happen to be naturally aligned with no padding on this toolchain, but nothing has been *decided*. If all four boards are the same PYNQ-Z2 with the same compiler this is free; the moment anything is serialised byte-wise it is not. | Framing in `ryb_ipc.h` | Open |
| 5 | **Framing** — does a header carry `RYB_IPC_VERSION` and a sender id (`ryb_node_t`)? Is there a checksum? Deliberately not written yet. | `ryb_ipc.h` | Open |
| 6 | **Failure behaviour** — what the motor module does when commands stop arriving. Hold last region, or ramp down? | Motor, safety | Open |

## Change log

| Version | Date | Change | Acknowledged by |
|---|---|---|---|
| `0x0001` | 2026-09-03 | Initial contract: `ryb_heart_msg_t`, `ryb_sound_msg_t`, `ryb_motion_cmd_t`. Framing deliberately unspecified. | *pending — members not yet invited* |
