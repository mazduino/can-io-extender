# Mazduino CAN IO Extender

Adds analog, digital and frequency inputs plus nine switched outputs to a
Mazduino Racedash over the CAN bus it already shares with the ECU.

Runs on an Arduino Mega 2560 with an MCP2515 CAN controller. It finds
the bus bitrate by itself, so one firmware works on a Haltech, rusEFI, MaxxECU,
OBD-II, aRacer or Custom bus with nothing to configure.

- **Pin map:** [PIN_MAPPING.md](PIN_MAPPING.md)
- **Frame layout and CAN ID allocation:** [CAN_PROTOCOL.md](CAN_PROTOCOL.md)

---

## What it gives you

| Capability | Count | Notes |
|---|---|---|
| Analog inputs | 11 | A0 is battery voltage |
| Switch inputs | 4 | SW1–SW4 |
| Hall / frequency inputs | 4 | HALL1–HALL4 |
| Low-side outputs | 5 + 2 | LC1–LC5, LS1–LS2 |
| High-side outputs | 2 | HS1–HS2, PWM capable, with fault reporting |
| Logic outputs | 2 | LOGIC1–LOGIC2 |

---

## Build and flash

`flash.sh` (macOS/Linux) and `flash.bat` (Windows) build the firmware for the
chosen node with PlatformIO and upload it over USB in one step.

```sh
./flash.sh                          # node 0, port auto-detected
./flash.sh 1                        # node 1
./flash.sh 2 /dev/cu.usbserial-1    # node 2 on a given port
CRYSTAL=16mhz ./flash.sh            # 16 MHz MCP2515 crystal
```

```bat
flash.bat                           :: node 0, port auto-detected
flash.bat 1 COM5
```

Requires [PlatformIO](https://platformio.org/) (`pip install platformio`).

### Manually

```sh
# Board as shipped (8 MHz crystal)
pio run -e megaatmega2560 -t upload

# 16 MHz MCP2515 crystal — adds 1 Mbps
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
| `MCP_CRYSTAL_16MHZ` | unset | Set it for a 16 MHz MCP2515 crystal |
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
SW1–SW4, which are active-low at the pin.

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

---

## Troubleshooting

**"Searching for bus bitrate" never stops**

- Check CAN H/L are not swapped, and that both ends of the bus are terminated
- Confirm the ECU is actually transmitting — the board locks on the first valid
  frame, so a silent bus never resolves
- On a Haltech bus (1 Mbps) this is expected with an 8 MHz build; see
  [CAN_PROTOCOL.md](CAN_PROTOCOL.md#limit-an-8-mhz-crystal-cannot-do-1-mbps)

**Bus locks, but the dash shows nothing**

- Confirm the dash and the extender agree on the CAN IDs (`0x640` + node × 0x10)
- If the dash protocol is Custom, make sure its map does not itself use
  0x640–0x67F

**Outputs stay off, serial says FAILSAFE**

- The dash is not sending output commands. Failsafe is correct behaviour here,
  not a fault

**Battery voltage reads wrong**

- Measure the 5 V rail and rebuild with `-D ADC_VREF_MV=<measured mV>`

---

## Repository layout

```
include/Config.h    pins, CAN IDs, timings — every tunable lives here
src/CanLink.*       MCP2515, bitrate auto-detection, TX/RX
src/Inputs.*        analog, digital and frequency sampling
src/Outputs.*       output driving and failsafe
src/main.cpp        scheduling and frame packing
```
