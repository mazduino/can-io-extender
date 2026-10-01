# Mazduino CAN IO Extender

Adds analog, digital and frequency inputs plus nine switched outputs to a
Mazduino Racedash over the CAN bus it already shares with the ECU.

Built on an Arduino Mega 2560 with an MCP2515/TJA1051 shield. The board finds
the bus bitrate by itself, so one firmware works on a Haltech, rusEFI, MaxxECU,
OBD-II, aRacer or Custom bus with nothing to configure.

- **Pin map:** [PIN_MAPPING.md](PIN_MAPPING.md)
- **Frame layout and CAN ID allocation:** [CAN_PROTOCOL.md](CAN_PROTOCOL.md)

---

## What it gives you

| Capability | Count | Notes |
|---|---|---|
| Analog inputs | 11 | A0 is battery sense; A1–A2 have 2k7 pull-ups for NTC sensors |
| 12 V switch inputs | 4 | SW1–SW4, shared with A3–A6 by jumper |
| Hall / frequency inputs | 4 | HALL1–HALL4, shared with A7–A10 by jumper |
| Low-side outputs | 5 + 2 | LC1–LC5 small, LS1–LS2 high current |
| High-side outputs | 2 | BTS4175 smart switches, PWM capable, with fault reporting |
| Logic outputs | 2 | TC4424A push-pull, 5 V or 12 V by jumper |

Seven outputs reach the main connector at once; two more share pins with the
logic outputs through jumpers J14/J15.

---

## Hardware

- Arduino Mega 2560
- CAN IO Expander shield (`CAN_IO_Expander.kicad_pcb` rev0) — MCP2515 + TJA1051
- 12 V switched supply on J2-24
- CAN H/L twisted pair, 120 Ω at both ends of the bus

### The 8 MHz crystal limit

Board rev0 fits an 8 MHz crystal (Y1). The MCP2515 library only offers rates up
to 500 kbps at 8 MHz — `CAN_1000KBPS` exists solely for `MCP_16MHZ`. **An 8 MHz
board cannot join a Haltech bus**, which runs at 1 Mbps.

Everything else works as built. Fit a 16 MHz crystal and use the
`megaatmega2560-16mhz` environment to add 1 Mbps.

---

## Flash a release

`flash.bat` (Windows) and `flash.sh` (macOS/Linux) ship with every Release.
They download the firmware for the chosen node from the latest Release and
flash it over USB; the Windows script also fetches avrdude on first run.

```sh
./flash.sh                      # node 0, port auto-detected
./flash.sh /dev/cu.usbserial-1 2
./flash.sh /dev/ttyUSB0 build.hex
CRYSTAL=16mhz ./flash.sh
```

```bat
flash.bat                       :: asks for the COM port, node 0
flash.bat COM5 1
```

## Build and flash

Requires [PlatformIO](https://platformio.org/).

```sh
# Board as shipped (8 MHz crystal)
pio run -e megaatmega2560 -t upload

# After swapping Y1 for a 16 MHz crystal — adds 1 Mbps
pio run -e megaatmega2560-16mhz -t upload

# Watch it come up
pio device monitor -b 115200
```

A healthy board prints this every two seconds:

```
=== Mazduino CAN IO Extender ===
Node 0  CAN ID 0x640-0x64F
MCP2515 crystal: 8 MHz (500 kbps max — Haltech bus not supported)
Searching for bus bitrate...
Bus locked 500000 bps | Vbatt 13.8 V | IN 0b10010 | OUT 0x00 (FAILSAFE)
```

`FAILSAFE` on that line is normal until the dash starts sending output commands.

---

## Configuration

All of it is done with build flags — there is nothing to set at runtime.

| Flag | Default | Purpose |
|---|---|---|
| `NODE_ID` | `0` | 0–3. Picks the CAN ID block: `0x640 + node × 0x10` |
| `MCP_CRYSTAL_16MHZ` | unset | Set it when Y1 is a 16 MHz crystal |
| `ADC_VREF_MV` | `5000` | Measure the 5 V rail and put the real figure here for accurate battery voltage |

Two boards on one bus:

```ini
[env:node1]
extends = env:megaatmega2560
build_flags =
	${env.build_flags}
	-D NODE_ID=1
```

---

## Wiring

### Inputs

Each input pin can be **analog or digital**, selected by a jumper. Bridge pins
2–1 for analog, pins 2–3 for digital.

| Connector | Jumper | Analog | Digital |
|---|---|---|---|
| J2-9 | J8 | A3 | SW1 |
| J2-8 | J9 | A4 | SW2 |
| J2-7 | J10 | A5 | SW3 |
| J2-6 | J11 | A6 | SW4 |
| J2-5 | J3 | A7 | HALL1 |
| J2-4 | J4 | A8 | HALL2 |
| J2-3 | J5 | A9 | HALL3 |
| J2-2 | J6 | A10 | HALL4 |

J2-10 and J2-11 are analog only (A2, A1) and carry 2k7 pull-ups, which is what
an NTC coolant or intake temperature sensor wants.

**SW1–SW4 take 12 V directly** — a 15k/4k7 divider and a MOSFET sit in front of
them. Use these for anything switched by vehicle 12 V: turn signals, high beam,
brake light, hand brake.

**HALL1–HALL4** are pulled up to 5 V through 1k, with a 470R series resistor and
a BAT54S clamp. They read 5 V logic and tolerate 12 V, and all four are Mega
interrupt pins, so they double as frequency counters for wheel speed or flow.

### Outputs

| Output | Connector | Type | Notes |
|---|---|---|---|
| LC1 | J2-23 | Low-side, small | AO3400A, flyback fitted |
| LC2 | J2-22 | Low-side, small | |
| LC3 | J2-20 | Low-side, small | |
| LC4 | J2-19 | Low-side, small | Shares the pin with LOGIC1 via J14 |
| LC5 | J2-17 | Low-side, small | Shares the pin with LOGIC2 via J15 |
| LS1 | J2-15 | Low-side, high current | TO-252 NMOS |
| LS2 | J2-16 | Low-side, high current | |
| HS1 | J2-21 | **High-side** | BTS4175, PWM capable, reports faults |
| HS2 | J2-18 | **High-side** | |

Low-side outputs switch the **ground** side of a load whose other end sits at
12 V. High-side outputs supply 12 V and expect the load's other end grounded.
Getting this backwards is the most common wiring mistake.

Only HS1 and HS2 accept a PWM duty; every other output is on/off.

### Logic outputs

LOGIC1 and LOGIC2 (nets IGN1/IGN2) run through a TC4424A push-pull driver.
Jumper **J13** sets their voltage: pin 2–1 for 12 V, pin 2–3 for 5 V.

These are logic-level outputs, not coil drivers. CAN latency rules out real
ignition timing — treat them as a stout 5 V or 12 V signal output.

### CAN and power

| Pin | Signal |
|---|---|
| J2-24 | 12 V switched (ignition) |
| J2-12 | Ground |
| J2-14 | CAN H |
| J2-13 | CAN L |

J12 is an alternative 4-pin CAN + power header. Bridge **J1** to fit the on-board
120 Ω terminator — do that only if this board sits at one physical end of the
bus.

> The board's 5 V rail back-feeds the Mega's 5 V pin. Think before powering the
> Mega from USB and 12 V at the same time.

---

## Using it with a Racedash

### Racedash v2 — indicator lamps

Works with no firmware change. v2 accepts every frame on the bus and lets any
tell-tale be pointed at an arbitrary CAN frame bit.

In DashTune, open **Indicators**, set the lamp to **CAN** mode, and enter:

| Lamp | CAN ID | Byte | Bit |
|---|---|---|---|
| Turn left | 0x643 | 0 | 0 |
| Turn right | 0x643 | 0 | 1 |
| High beam | 0x643 | 0 | 2 |
| Hand brake | 0x643 | 0 | 3 |
| Head light | 0x643 | 0 | 4 |
| Park light | 0x643 | 0 | 5 |

Leave **invert off**. The frame already reports 1 = active, including for
SW1–SW4, whose hardware is active-low.

Wire those six lamps to J2-9, J2-8, J2-7, J2-6, J2-5 and J2-4, with the matching
jumper set to digital.

The board sends this frame at 50 Hz, well inside the 1000 ms staleness window v2
uses, so a lamp goes dark within a second of the CAN link dropping rather than
staying lit.

### Racedash legacy — Custom CAN map

Set the dash protocol to **Custom**, set the bitrate to match the bus, then add
channels. Every value is little-endian, so leave **MSB first (Motorola) off**.

| CAN ID | Byte | Size | Scale | Dash channel |
|---|---|---|---|---|
| 0x640 | 0 | 2 bytes | 0.001 | Voltage (mV → V) |
| 0x640 | 2 | 2 bytes | sensor dependent | e.g. Oil Pressure |
| 0x643 | 0, bit 0 | 1 bit | — | an indicator |
| 0x644 | 0 | 2 bytes | wheel circumference ÷ 10 | VSS |

This only applies while the dash protocol is Custom, so it cannot run alongside
a built-in ECU decoder. Reading the extender at the same time as an ECU protocol
needs native support in the legacy dash firmware — see
[CAN_PROTOCOL.md](CAN_PROTOCOL.md#racedash-legacy-native-decoder--needs-dash-firmware-changes).

---

## Safety

**Outputs fail off.** Every output is switched off if output commands stop
arriving for 500 ms, and the board starts in that state — an output can only
come on after a command has genuinely been received. Without this, a dash that
loses power or a CAN cable that falls off would leave a pump, fan or solenoid
energised indefinitely.

**Nothing is transmitted before the bitrate is known.** Bitrate probing runs in
listen-only mode. A controller guessing the wrong bitrate in normal mode floods
the bus with error frames and takes down traffic that was working fine.

**Check J14/J15 before blaming the firmware.** Output 4 (J2-19) and Output 5
(J2-17) are each shared between an LC output and a logic output. Only one is
connected at a time, so switching LC4 over CAN does nothing at the connector
while J14 is set to LOGIC1.

---

## Troubleshooting

**"Searching for bus bitrate" never stops**

- Check CAN H/L are not swapped, and that both ends of the bus are terminated
- Confirm the ECU is actually transmitting — the board locks on the first valid
  frame, so a silent bus never resolves
- On a Haltech bus this is expected with an 8 MHz crystal; see above

**Bus locks, but the dash shows nothing**

- Confirm the dash and the extender agree on the CAN IDs (`0x640` + node × 0x10)
- If the dash protocol is Custom, make sure its map does not itself use
  0x640–0x67F

**Outputs stay off, serial says FAILSAFE**

- The dash is not sending output commands. Failsafe is correct behaviour here,
  not a fault

**Battery voltage reads wrong**

- Measure the 5 V rail and rebuild with `-D ADC_VREF_MV=<measured mV>`

**A switch input reads inverted**

- Check the jumper is on the digital position (pins 2–3). In the analog position
  the pin floats at whatever the divider leaves it

---

## Repository layout

```
include/Config.h    pins, CAN IDs, timings — every tunable lives here
src/CanLink.*       MCP2515, bitrate auto-detection, TX/RX
src/Inputs.*        analog, digital and frequency sampling
src/Outputs.*       output driving and failsafe
src/main.cpp        scheduling and frame packing
```
