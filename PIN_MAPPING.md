# CAN IO Extender — Firmware Pin Map

Arduino Mega 2560 pins used by the firmware. Source of truth: `include/Config.h`.

## CAN (MCP2515)

| Function | Mega pin |
|---|---|
| SPI CS | D9 |
| INT | D2 |
| SCK / MOSI / MISO | hardware SPI |

## Outputs

| Output | Mega pin | Mode |
|---|---|---|
| LC1 | D42 | on/off |
| LC2 | D43 | on/off |
| LC3 | D47 | on/off |
| LC4 | D48 | on/off |
| LC5 | D49 | on/off |
| LS1 | D39 | on/off |
| LS2 | D40 | on/off |
| HS1 | D45 | on/off or PWM (timer 5) |
| HS2 | D46 | on/off or PWM (timer 5) |
| LOGIC1 | D23 | on/off |
| LOGIC2 | D22 | on/off |

## Inputs

| Input | Mega pin | Notes |
|---|---|---|
| HS1 fault | D28 | |
| HS2 fault | D29 | |
| SW1–SW4 | D24–D27 | Active-low at the pin; reported as 1 = active |
| HALL1–HALL4 | D18–D21 | Interrupt pins, used for frequency |
| Vbatt | A0 | Scaled by 4.9 to battery millivolts |
| AV1–AV10 | A1–A10 | Reported in millivolts at the pin |

## Serial

| Port | Mega pins | Use |
|---|---|---|
| Serial | USB | Status log, 115200 |
| Serial2 | D16 / D17 | Config link, 9600 |
