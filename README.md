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

With nothing talking to the USB port, a healthy board prints this every two
seconds. The log stays quiet while TunerStudio is connected, because it shares
the port:

```
=== Mazduino CAN IO Extender ===
Node 0  CAN ID 0x640-0x64F
MCP2515 crystal: 8 MHz (500 kbps max — Haltech bus not supported)
Searching for bus bitrate...
Bus locked 500000 bps | Vbatt 13.8 V | IN 0b10010 | OUT 0x00 (FAILSAFE)
```

`FAILSAFE` on that line is normal until the dash starts sending output commands.

---

## TunerStudio

The USB port speaks TunerStudio's serial protocol at 115200 baud. Create a new
project with `tunerstudio/mazduino-iox.ini` (also attached to each Release) as
the ECU definition; the signature is `mazduino-iox 1`.

- **Sensors** menu: one entry per analog input, AV1–AV10, each with a 2–8 point
  curve from input volts to any value, plus decimals, unit and gauge range.
  The curve shows the live input as a cursor. Burn to keep it; results go out
  in the CAL_A/B/C frames.
- **Module → HALL inputs**: per input, a function — RPM (tach/ignition with a
  cylinder count and 2/4-stroke, or custom pulses per revolution), speed
  (pulses per km) or plain frequency — plus a noise filter for ringing
  tach-adapter signals and smoothing. Results go out in the HALL frame.
- **Outputs** menu, modelled on Speeduino's programmable outputs: per output a
  name and a mode — Dash button (a Racedash CAN Button on `0x648`; each dialog
  shows the byte and ON value to enter), Rule, Dash button OR Rule, or Dash
  button AND Rule (the button only works while the rule holds). A rule is one
  or two comparisons combined with AND/OR/XOR, each with hysteresis, plus an
  on-delay, a minimum or maximum on-time (e.g. a 3 s fuel pump prime), an
  inverted option, and a PWM duty on HS1/HS2. Any output's state can be a
  rule source, so rules chain for more than two conditions. Rules keep
  running without a dash. Output test switches each output by hand.
- **Module** menu: node number and CAN bitrate (auto or fixed). A node set here
  survives reflashing the same build; flashing a build for another node
  replaces it.
- **Gauges and datalog**: battery, AV1–AV10, HALL1–HALL4, calibrated sensors,
  CAN RX/TX rate, error counters and failed sends, switch inputs, output
  state, failsafe and bus lock.
- **Module → Input simulator**: replaces the real inputs with made-up values
  — an auto sweep (RPM, speed, battery, AV1–AV10, blinking indicator bits) or
  values you set by hand. The CAN frames, the dash and the output rules all
  see them, so it checks a dash layout or a rule on the bench. Not saved;
  stops by itself 2 s after TunerStudio goes quiet.
- **Module → CAN monitor**: the six busiest IDs on the bus with frames/s, and
  one watched ID with its rate, length and bytes 0–7. TunerStudio has no hex
  entry or display, so IDs and bytes are decimal there (`0x643` = 1603); use
  the USB console below for hex.

### USB CAN console

With TunerStudio closed, open any serial monitor at 115200 and send `!` to
print every frame the extender receives, candump style:

```
12.345  643  [6]  01 00 00 00 00 00
12.351  18FF0001  [8]  00 11 22 33 44 55 66 77
```

`!643` prints only ID `0x643`; `!` again stops. When the serial link cannot
keep up, skipped frames are reported as `-- dropped N`. Any TunerStudio command
stops the console.

Typical use is a fuel level sender on AV3: fill in its volts at empty, a few
points in between and full, and read the result on the dash with a Custom
Channel on CAL_A (`0x645` on node 0), byte 4.

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
src/Calibration.*   calibration slots, EEPROM, TunerStudio page
src/TsLink.*        TunerStudio serial protocol on USB
src/ConfigLink.*    JSON config link on Serial2
tunerstudio/        TunerStudio ECU definition
src/main.cpp        scheduling and frame packing
```
