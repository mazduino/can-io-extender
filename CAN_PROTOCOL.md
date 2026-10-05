# CAN IO Extender — CAN Protocol

Firmware pin map: [PIN_MAPPING.md](PIN_MAPPING.md)

| | |
|---|---|
| Device | Arduino Mega 2560 + MCP2515 |
| Identifiers | 11-bit standard, **0x640 – 0x67F** (4 nodes × 16 IDs) |
| Byte order | **Little-endian** |
| Bitrate | **auto-detected** — 500k / 250k / 125k, plus 1M with a 16 MHz crystal |
| Node | 0–3, set in TunerStudio or with `-D NODE_ID=n`. Base ID = `0x640 + node × 0x10` |

---

## 1. Why 0x640

Racedash supports six ECU protocols at once, and some of them claim wide ID
blocks. The extender's block has to be free in every one of them, with no
exceptions:

| Block | Owner |
|---|---|
| 0x200–0x209 | rusEFI base broadcast |
| 0x2D0–0x2D3 | aRacer Super X |
| 0x360–0x3EF | Haltech broadcast |
| **0x500–0x5FF** | MaxxECU — racedash calls `watchForRange(0x500, 0x5FF)`, so the **whole block** is spoken for |
| 0x600–0x601 | Racedash CAN Buttons — the dash **transmits** here, it is not only a receiver |
| 0x6F0–0x6FF | Haltech body/chassis (lights, tyre pressure) |
| 0x700–0x78F | EPIC / epicEFI |
| 0x7DF, 0x7E0–0x7EF | OBD-II ISO 15765-4 |

Extended 29-bit identifiers also in use: `0x770000–0x77001F` (rusEFI bench test
and GPS input) and `0xBB20` (rusEFI GDI4). This board uses 11-bit identifiers
only and discards every frame carrying the extended flag, so it never overlaps
those.

Remaining free gaps: `0x20A–0x2CF`, `0x2D4–0x35F`, `0x3F0–0x4FF`,
`0x602–0x6EF`, `0x790–0x7DE`, `0x7F0–0x7FF`.

**0x640–0x67F** comes out of the `0x602–0x6EF` gap because it is the roomiest:
63 IDs of clearance below to 0x601, 113 above to 0x6F0. The space under 0x640 is
deliberately left for CAN Buttons to grow into.

> The **Custom** protocol is the only one that can collide, because its IDs are
> chosen by the user. If you use the Custom CAN map, keep it out of 0x640–0x67F.

---

## 2. Frames

`B` = the node's base ID (`0x640` for node 0).

### Extender → bus

| ID | Name | Rate | Contents |
|---|---|---|---|
| B+0 | ANALOG_A | on change ≥ 10 mV, 25–200 ms | Vbatt, AV1, AV2, AV3 |
| B+1 | ANALOG_B | on change ≥ 10 mV, 25–200 ms | AV4, AV5, AV6, AV7 |
| B+2 | ANALOG_C | on change ≥ 10 mV, 25–200 ms | AV8, AV9, AV10, spare |
| B+3 | DIGITAL | on change, 25–500 ms | digital inputs, diagnostics, output feedback, status |
| B+4 | FREQ | every 50 ms | Hall 1–4 frequency |
| B+5 | CAL_A | on change ≥ 2, 25–200 ms | calibrated AV1–AV4 |
| B+6 | CAL_B | on change ≥ 2, 25–200 ms | calibrated AV5–AV8 |
| B+7 | CAL_C | on change ≥ 2, 25–200 ms | calibrated AV9–AV10, spare |
| B+9 | HALL | every 50 ms | HALL1–HALL4 as rpm, speed, frequency or switch state |

"On change, 25–200 ms" means a frame goes out as soon as a value moves by the
threshold, no faster than every 25 ms, and at least every 200 ms when nothing
changes.

**ANALOG_A / B / C** — four little-endian `uint16` values in **millivolts at the
Mega pin** (0–5000). Vbatt has the ×4.9 scaling already applied, so it is
real battery millivolts (13800 = 13.8 V).

| Byte | ANALOG_A | ANALOG_B | ANALOG_C |
|---|---|---|---|
| 0–1 | Vbatt (battery mV) | AV4 | AV8 |
| 2–3 | AV1 | AV5 | AV9 |
| 4–5 | AV2 | AV6 | AV10 |
| 6–7 | AV3 | AV7 | spare (0) |

**DIGITAL**

| Byte | Contents |
|---|---|
| 0 | bits 0–3 = SW1–SW4, bits 4–7 = HALL1–HALL4: 1 = active for an input set to Switch, else the pin's instantaneous level |
| 1 | bit 0 = HS1_Diag, bit 1 = HS2_Diag (1 = fault reported) |
| 2 | output feedback byte 0 (see OUTPUT_CMD) |
| 3 | output feedback byte 1 |
| 4 | bit 0 = failsafe active, bit 1 = bitrate locked |
| 5–6 | uptime seconds (`uint16`, saturates at 65535) |
| 7 | bits 7–4 = firmware version, bits 3–0 = node id |

> **SW1–SW4 are already normalised.** In hardware, 12 V in turns the MOSFET on
> and the Mega pin reads LOW. This frame reports **1 = active**, so the receiving
> side never has to know about the inversion.

> **Output feedback is not a copy of the command.** The bits reported are the
> ones actually being driven, which is what makes failsafe visible to the dash.

**CAL_A / B / C** — little-endian **signed** `int16` values, one per analog
input: CAL_A carries AV1–AV4, CAL_B AV5–AV8, CAL_C AV9–AV10 (bytes 4–7 are 0).
Each is that input run through its own 2–8 point curve, in whatever unit and
decimals the curve was filled in with. An input with fewer than 2 points reads
0. Curves are set up from TunerStudio (see README).

**HALL** — four little-endian `uint16` values, one per HALL input, measured
from the time between pulses (average of the last four gaps). What each one
holds is that input's Function in TunerStudio:

| Function | Value | Set by |
|---|---|---|
| RPM | rpm | cylinder count and 2/4-stroke, or custom pulses per revolution |
| Speed (VSS) | km/h × 10 | pulses per km |
| Frequency | Hz × 10 | — |
| Switch | 1 while active, else 0 | Polarity: Default = on while the pin is high (12 V); Inverted = on while grounded (clutch, launch control switch) |

A Switch input is not timed: it follows the pin level, through the input's 1 kΩ
pull-up to 5 V, so a switch to ground needs no extra resistor.

An input with no pulse for 1 s reads 0.

**FREQ** — four little-endian `uint16` values in **deci-Hertz** (0.1 Hz per
count, 0–6553.5 Hz). Bytes 0–1 Hall1, 2–3 Hall2, 4–5 Hall3, 6–7 Hall4.

### Bus → extender

| ID | Name | Contents |
|---|---|---|
| B+8 | OUTPUT_CMD | output command from the dash |

| Byte | Contents |
|---|---|
| 0 | bit0=LC1, bit1=LC2, bit2=LC3, bit3=LC4, bit4=LC5, bit5=LS1, bit6=LS2 |
| 1 | bit0=HS1, bit1=HS2, bit2=LOGIC1 (net IGN1), bit3=LOGIC2 (net IGN2) |
| 2 | HS1 duty (1–254 = PWM; 0 or 255 = fully on) |
| 3 | HS2 duty |
| 4–7 | spare |

Any DLC from 1 is accepted; bytes the sender leaves off read as 0. Racedash's
CAN Buttons send a frame only as long as the highest byte a button uses, so a
dash driving only byte 0 sends one byte.

The HS1/HS2 bits in byte 1 switch those outputs on and off; the duty bytes only
shape an output that is already on. Duty 0 therefore means **fully on**, not
off, so a sender that never fills the duty byte gets a working output. Only HS1
(D45) and HS2 (D46) can do hardware PWM (Mega timer 5); every other output is
on/off.

`0x64F` (B+F on node 0) is the M-Link heartbeat a Racedash on M-Link sends every
100 ms; the extender ignores it. IDs `B+A` through `B+E` are held in reserve so
future features do not force the block to move.

---

## 3. Bitrate auto-detection

Racedash runs a different bitrate per protocol: Haltech 1 Mbps, rusEFI /
MaxxECU / OBD-II 500 kbps, aRacer 250 kbps, Custom whatever the user picked.
This board has no idea which ECU is fitted, so it works the bitrate out itself.

Search order: **500k → 250k → 125k** (→ 1M with a 16 MHz crystal), listening for
300 ms on each. One complete frame is enough to lock — CAN's CRC and bit
stuffing make a valid frame at the wrong bitrate effectively impossible.

Probing runs in **listen-only mode**. That is not a detail: in normal mode a
controller on the wrong bitrate transmits error frames and wrecks traffic that
was working. This board never transmits anything before the bitrate is locked.

If the bus falls silent for 3 seconds the lock is dropped and the search starts
again — which covers swapping the ECU or changing the dash protocol.

### Limit: an 8 MHz crystal cannot do 1 Mbps

With an 8 MHz MCP2515 crystal the library only provides rates up to
`CAN_500KBPS` for `MCP_8MHZ`; `CAN_1000KBPS` exists solely for `MCP_16MHZ`.

| Protocol | Bitrate | 8 MHz board | 16 MHz board |
|---|---|---|---|
| rusEFI | 500 kbps | ✅ | ✅ |
| MaxxECU | 500 kbps | ✅ | ✅ |
| OBD-II | 500 kbps | ✅ | ✅ |
| aRacer | 250 kbps | ✅ | ✅ |
| Custom | 125/250/500 kbps | ✅ | ✅ |
| **Haltech** | **1 Mbps** | ❌ | ✅ |
| Custom at 1M | 1 Mbps | ❌ | ✅ |

With a 16 MHz crystal, build the `megaatmega2560-16mhz` environment to get
1 Mbps. An 8 MHz build will never lock onto a Haltech bus and
will keep searching — without disturbing the bus, because the search is
listen-only.

---

## 4. Output safety

**Modes.** Each output has a mode, set in TunerStudio (Outputs menu):

| Mode | Output is on when |
|---|---|
| CAN (default) | the dash's OUTPUT_CMD bit is set |
| Rule | the output's own rule holds |
| CAN or Rule | either |

A rule compares one source — a calibrated AV input, a raw AV voltage, a HALL
value, SW1–SW4 or the battery — against a target with hysteresis, optionally
AND/OR a second comparison, with an on-delay and, for HS1/HS2, a PWM duty.
Rules run on the module alone, so they work with no dash and no CAN at all.

**Failsafe.** The CAN part of every output is dropped if OUTPUT_CMD stops
arriving for 500 ms. Without it, a dash that loses power or a CAN cable that
falls off leaves a pump, fan or solenoid energised indefinitely. The board also
**starts in failsafe** — a CAN-driven output can only come on once a command has
genuinely been received. Rules are not affected: they keep switching from the
module's own inputs.

**Output test.** TunerStudio's Output test takes over every output while it is
on, and switches itself off 2 s after TunerStudio stops talking.

**LOGIC1/LOGIC2** are **logic level outputs, not coil drivers**.

There is no maximum on-time — a logic level has to be holdable indefinitely.
The failsafe above still applies to both.

> The first version of this firmware gave both outputs a 20 ms dwell limit,
> because I had assumed they drove coils. That was wrong and it broke their
> intended use; the limiter has been removed.

---

## 5. Reading the extender from a Racedash

### Racedash v2 — indicator lamps, no firmware change

Racedash v2 already has everything needed, so the extender works as-is:

- Its TWAI filter is `TWAI_FILTER_CONFIG_ACCEPT_ALL()`, so frame 0x643 arrives
  without any ID having to be registered.
- `indicator_source_apply_frame()` runs for **every** received frame and matches
  arbitrary CAN IDs, not just protocol IDs.
- Any tell-tale can be pointed at `{can_id, byte_offset, bit, invert}` per ECU
  protocol — exactly the shape of a one-bit lamp.

Configure it in DashTune → **Indicators**, mode **CAN**:

| Lamp | CAN ID | Byte | Bit | Input |
|---|---|---|---|---|
| Turn left | 0x643 | 0 | 0 | SW1 |
| Turn right | 0x643 | 0 | 1 | SW2 |
| High beam | 0x643 | 0 | 2 | SW3 |
| Hand brake | 0x643 | 0 | 3 | SW4 |
| Head light | 0x643 | 0 | 4 | HALL1, Function Switch |
| Park light | 0x643 | 0 | 5 | HALL2, Function Switch |

Leave **invert off**: this frame already reports 1 = active, including for
SW1–SW4, which are active-low in hardware.

The DIGITAL frame goes out at 50 Hz, comfortably inside the
`DASH_IND_CAN_TIMEOUT_MS` (1000 ms) window v2 uses to blank a lamp whose source
has gone. If the CAN cable falls off, the lamp goes dark within a second instead
of staying lit.

> Until September 2026, `dash_indicator_t` in racedash v2 had no entries for
> high beam, head light or park light — all three were decoded from Haltech
> 0x6F4 into `vehicle_data_t` but nothing ever read them. `DASH_IND_HIGH_BEAM`,
> `DASH_IND_HEAD_LIGHT` and `DASH_IND_PARK_LIGHT` were added alongside this
> board. Turn signals were supported from the start.

### Racedash legacy, Custom CAN map — no dash firmware change

Because the frames are simple and every value is `raw × scale + offset`, the
extender can be read through the existing Custom CAN map. Set the dash protocol
to **Custom**, set the bitrate to match the bus, then fill in the channel table.
Everything is little-endian, so leave **MSB first (Motorola) off**.

Example, node 0:

| CAN ID | Byte | Size | Scale | Offset | Dash channel |
|---|---|---|---|---|---|
| 0x640 | 0 | 2 bytes | 0.001 | 0 | Voltage (mV → V) |
| 0x640 | 2 | 2 bytes | *sensor dependent* | | e.g. Oil Pressure |
| 0x643 | 0 bit 0 | 1 bit | — | — | an indicator, e.g. LCH |
| 0x644 | 0 | 2 bytes | *wheel circumference ÷ 10* | 0 | VSS |

The limitation: this only works while the dash protocol is Custom, so it cannot
run alongside a built-in ECU decoder.

### Racedash legacy, native decoder — needs dash firmware changes

For the extender to be read **at the same time** as any ECU protocol, racedash
needs:

1. `CAN0.watchFor(0x640..0x644)` added to **every** protocol branch in
   `setupCAN()`, not just one.
2. A dispatch to an extender decoder in `handleCANCommunication()`, running
   alongside the ECU decoder rather than replacing it.
3. A way to map generic analog channels onto `DataSource`, since AV1–AV10 could
   be any sensor. The sensible shape is a small mapping table in EEPROM, much
   like the Custom CAN map but specific to the extender.

Point 3 is a design decision that has not been made yet.
