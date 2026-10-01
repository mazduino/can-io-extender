# CAN IO Expander — Pin Mapping

Arduino Mega 2560 shield with MCP2515/TJA1051. Derived from the
`CAN_IO_Expander.kicad_pcb` netlist (rev0).

---

## 1. Arduino Mega 2560 Pin Map (XA1)

### 1.1 CAN / SPI

| Mega pin | Net | Function |
|---|---|---|
| D9 | `SPI_CS` | MCP2515 `/CS` (pin 16) |
| D2 (INT0) | `CAN_INT` | MCP2515 `/INT` (pin 12) |
| SCK (D52) | `SPI_CLK` | MCP2515 SCK |
| MOSI (D51) | `SPI_MOSI` | MCP2515 SI |
| MISO (D50) | `SPI_MISO` | MCP2515 SO |

> SPI comes from the 2x3 ICSP header (SPI_SCK/MOSI/MISO), not from D50–D52 on
> the digital header — D50/D51/D52 themselves are not connected.

### 1.2 Outputs

| Mega pin | Net | Driver | Type | Goes to |
|---|---|---|---|---|
| D42 | `LC1` | Q10 AO3400A (R93 220R gate, R94 10k pulldown) | Low-side, logic-level MOSFET | `OUT_LC1` → J2-23 |
| D43 | `LC2` | Q1 AO3400A (R28 / R29) | Low-side | `OUT_LC2` → J2-22 |
| D47 | `LC3` | Q2 AO3400A (R30 / R31) | Low-side | `OUT_LC3` → J2-20 |
| D48 | `LC4` | Q3 AO3400A (R32 / R33) | Low-side | `OUT_LC4` → J14-3 (via jumper) |
| D49 | `LC5` | Q4 AO3400A (R34 / R35) | Low-side | `OUT_LC5` → J15-3 (via jumper) |
| D39 | `LS1` | Q5 NMOS TO-252 (R12 1k gate, R11 100k pulldown) | Low-side, high current | `OUT_LS1` → J2-15 |
| D40 | `LS2` | Q6 NMOS TO-252 (R17 1k, R16 100k) | Low-side, high current | `OUT_LS2` → J2-16 |
| D45 | `HS1` | IC1 BTS4175SGA (R105 10k, R40 10k pulldown) | **High-side** smart switch | `OUT_HS1` → J2-21 |
| D46 | `HS2` | IC2 BTS4175SGA (R25 10k, R41 10k pulldown) | **High-side** smart switch | `OUT_HS2` → J2-18 |
| D23 | `IGN1` | U3 TC4424A IN_A (R59 1k pulldown) | Logic-level gate driver | `OUT_IGN1` → J14-1 (via R65 100R) |
| D22 | `IGN2` | U3 TC4424A IN_B (R60 1k pulldown) | Logic-level gate driver | `OUT_IGN2` → J15-1 (via R66 100R) |

### 1.3 Diagnostic Inputs

| Mega pin | Net | Source |
|---|---|---|
| D28 | `HS1_Diag` | IC1 pin ST (R106 10k pull-up ke 5V) |
| D29 | `HS2_Diag` | IC2 pin ST (R26 10k pull-up ke 5V) |

### 1.4 Digital Inputs

| Mega pin | Net | Front end | Source |
|---|---|---|---|
| D18 (TX1) | `HALL1` | R22 470R + clamp D17 BAT54S + C37 | `IN_HALL1` ← J3-3, R20 1k pull-up to 5V |
| D19 (RX1) | `HALL2` | R23 470R + D18 + C38 | `IN_HALL2` ← J4-3, R21 1k pull-up |
| D20 (SDA) | `HALL3` | R38 470R + D23 + C36 | `IN_HALL3` ← J5-3, R36 1k pull-up |
| D21 (SCL) | `HALL4` | R39 470R + D24 + C39 | `IN_HALL4` ← J6-3, R37 1k pull-up |
| D24 | `SW1` | Q7 AO3400A drain, R43 10k pull-up to 5V | `IN_SW1` ← J8-3 (R13 15k / R42 4k7, clamp D12) |
| D25 | `SW2` | Q8 drain, R46 10k pull-up | `IN_SW2` ← J9-3 (R44 15k / R45 4k7, clamp D11) |
| D26 | `SW3` | Q9 drain, R49 10k pull-up | `IN_SW3` ← J10-3 (R47 15k / R48 4k7, clamp D14) |
| D27 | `SW4` | Q11 drain, R52 10k pull-up | `IN_SW4` ← J11-3 (R50 15k / R51 4k7, clamp D15) |

> **Important:** the `SW1–SW4` inputs are **inverted**. 12 V present → MOSFET on
> → the Mega pin reads **LOW**. No voltage → the pin reads **HIGH**. The firmware
> normalises this before transmitting, so the CAN frame reports 1 = active.

### 1.5 Analog Inputs

| Mega pin | Net | Series | 5V pull-up | Clamp | Source |
|---|---|---|---|---|---|
| A0 | `AV0` | — | — | D16 | **Vbatt sense**: R19 3k9 from 12V-SW + R18 1k to GND (ratio ≈ 1:4.9) |
| A1 | `AV1` | R1 470R | R53 2k7 | D1 | `IN_AV1` ← J2-11 (analog only) |
| A2 | `AV2` | R2 470R | R54 2k7 | D2 | `IN_AV2` ← J2-10 (analog only) |
| A3 | `AV3` | R3 470R | R55 2k7 | D3 | `IN_AV3` ← J8-1 |
| A4 | `AV4` | R4 470R | R56 2k7 | D4 | `IN_AV4` ← J9-1 |
| A5 | `AV5` | R5 470R | R57 2k7 | D5 | `IN_AV5` ← J10-1 |
| A6 | `AV6` | R6 470R | — | D6 | `IN_AV6` ← J11-1 |
| A7 | `AV7` | R7 470R | — | D7 | `IN_AV7` ← J3-1 |
| A8 | `AV8` | R8 470R | — | D8 | `IN_AV8` ← J4-1 |
| A9 | `AV9` | R9 470R | — | D9 | `IN_AV9` ← J5-1 |
| A10 | `AV10` | R10 470R | — | D10 | `IN_AV10` ← J6-1 |
| A11–A15 | `AV11`–`AV15` | — | — | — | **Not used** (label only, not connected) |

> The 2k7 pull-ups on A1–A5 are meant for NTC sensors (CLT/IAT). A6–A10 have no
> pull-up, for voltage-output sensors (TPS/MAP).

### 1.6 Serial

| Mega pin | Net | Goes to |
|---|---|---|
| D16 (TX2) | `TX2` | J7-2 |
| D17 (RX2) | `RX2` | J7-3 |
| D14 (TX3) | `TX3` | Label only, not connected |
| D15 (RX3) | `RX3` | Label only, not connected |

### 1.7 Power

| Mega pin | Net |
|---|---|
| 5V (x4, including ICSP) | `+5V` — supplied by U8 LM2940S-5.0 on this board |
| GND (x6) | `GND` |
| VIN, AREF, RESET(ICSP) | Not connected |

### 1.8 Unused Mega Pins

`D0, D1, D3, D4, D5, D6, D7, D8, D10, D11, D12, D13, D30–D38, D41, D44, D50–D53, SDA, SCL, AREF, VIN`

---

## 2. J2 — Main Connector, Molex Micro-Fit 3.0, 2x12 (24 pin)

| Pin | Net | Direction | Notes |
|---|---|---|---|
| 1 | — | — | Not used |
| 2 | `IN10` | IN | Via jumper J6 → analog A10 or Hall4 (D21) |
| 3 | `IN9` | IN | Via J5 → A9 or Hall3 (D20) |
| 4 | `IN8` | IN | Via J4 → A8 or Hall2 (D19) |
| 5 | `IN7` | IN | Via J3 → A7 or Hall1 (D18) |
| 6 | `IN6` | IN | Via J11 → A6 or SW4 (D27) |
| 7 | `IN5` | IN | Via J10 → A5 or SW3 (D26) |
| 8 | `IN4` | IN | Via J9 → A4 or SW2 (D25) |
| 9 | `IN3` | IN | Via J8 → A3 or SW1 (D24) |
| 10 | `IN_AV2` | IN | Analog only → A2 (2k7 pull-up) |
| 11 | `IN_AV1` | IN | Analog only → A1 (2k7 pull-up) |
| 12 | `GND` | — | Ground |
| 13 | `CANL` | I/O | CAN Low |
| 14 | `CANH` | I/O | CAN High |
| 15 | `OUT_LS1` | OUT | Low-side, high current (Q5), flyback D13 |
| 16 | `OUT_LS2` | OUT | Low-side, high current (Q6), flyback D27 |
| 17 | `OUT5` | OUT | Via jumper J15: LOGIC2 **or** LC5 |
| 18 | `OUT_HS2` | OUT | High-side IC2 (R24 4k7 pulldown, C35) |
| 19 | `OUT4` | OUT | Via jumper J14: LOGIC1 **or** LC4 |
| 20 | `OUT_LC3` | OUT | Low-side Q2, flyback D20 |
| 21 | `OUT_HS1` | OUT | High-side IC1 (R104 4k7 pulldown, C55) |
| 22 | `OUT_LC2` | OUT | Low-side Q1, flyback D19 |
| 23 | `OUT_LC1` | OUT | Low-side Q10, flyback D31 |
| 24 | `12V-SW` | PWR | 12 V input (switched/IGN) |

---

## 3. Headers & Jumpers

### 3.1 Input Mode Select Jumpers (pin 2 = common/input)

| Jumper | Pin 1 (analog) | Pin 2 (input) | Pin 3 (digital) |
|---|---|---|---|
| J3 | `IN_AV7` → A7 | `IN7` ← J2-5 | `IN_HALL1` → D18 |
| J4 | `IN_AV8` → A8 | `IN8` ← J2-4 | `IN_HALL2` → D19 |
| J5 | `IN_AV9` → A9 | `IN9` ← J2-3 | `IN_HALL3` → D20 |
| J6 | `IN_AV10` → A10 | `IN10` ← J2-2 | `IN_HALL4` → D21 |
| J8 | `IN_AV3` → A3 | `IN3` ← J2-9 | `IN_SW1` → D24 |
| J9 | `IN_AV4` → A4 | `IN4` ← J2-8 | `IN_SW2` → D25 |
| J10 | `IN_AV5` → A5 | `IN5` ← J2-7 | `IN_SW3` → D26 |
| J11 | `IN_AV6` → A6 | `IN6` ← J2-6 | `IN_SW4` → D27 |

Bridge pins 2–1 for analog mode, pins 2–3 for digital mode.

### 3.2 Solder Jumpers (pin 2 = common)

| Jumper | Pin 1 | Pin 2 (common) | Pin 3 | Purpose |
|---|---|---|---|---|
| J13 | `12V-SW` | `Vdrive` | `+5V` | TC4424A supply: 12V or 5V — sets the LOGIC1/LOGIC2 output level |
| J14 | `OUT_IGN1` | `OUT4` → J2-19 | `OUT_LC4` | Output 4: logic driver or low-side MOSFET |
| J15 | `OUT_IGN2` | `OUT5` → J2-17 | `OUT_LC5` | Output 5: logic driver or low-side MOSFET |

### 3.3 Other Headers

| Connector | Pin | Net | Notes |
|---|---|---|---|
| J1 (1x2) | 1 | R15 120R → CANH | CAN terminator — fit the jumper to enable the 120R |
| | 2 | `CANL` | |
| J7 (1x4) | 1 | `+5V` | Serial2 header |
| | 2 | `TX2` → D16 | |
| | 3 | `RX2` → D17 | |
| | 4 | `GND` | |
| J12 (1x4) | 1 | `12V_PROT` | Alternative CAN + power header |
| | 2 | `CANL` | |
| | 3 | `CANH` | |
| | 4 | `GND` | |

---

## 4. ICs — Pin Mapping

### 4.1 U1 MCP2515 (CAN controller, SOIC-18W)

| Pin | Name | Net |
|---|---|---|
| 1 | TXCAN | `CAN_TX` → U2-1 |
| 2 | RXCAN | `CAN_RX` → U2-4 |
| 3–6 | CLKOUT, TX0RTS–TX2RTS | NC |
| 7 | OSC2 | Y1 8 MHz + C1 22pF |
| 8 | OSC1 | Y1 + C2 22pF |
| 9 | VSS | GND |
| 10–11 | RX1BF, RX0BF | NC |
| 12 | /INT | `CAN_INT` → Mega D2 |
| 13 | SCK | `SPI_CLK` |
| 14 | SI | `SPI_MOSI` |
| 15 | SO | `SPI_MISO` |
| 16 | /CS | `SPI_CS` → Mega D9 |
| 17 | /RESET | R14 10k pull-up to 5V |
| 18 | VDD | +5V |

### 4.2 U2 TJA1051T-3 (CAN transceiver, SOIC-8)

| Pin | Name | Net |
|---|---|---|
| 1 | TXD | `CAN_TX` |
| 2 | GND | GND |
| 3 | VCC | +5V |
| 4 | RXD | `CAN_RX` |
| 5 | VIO | +5V |
| 6 | CANL | `CANL` |
| 7 | CANH | `CANH` |
| 8 | S | R58 4k7 → GND (mode normal) |

### 4.3 U3 TC4424A (dual gate driver, SOIC-8)

| Pin | Name | Net |
|---|---|---|
| 1, 8 | NC | — |
| 2 | IN_A | `IGN1` ← Mega D23 |
| 3 | GND | GND |
| 4 | IN_B | `IGN2` ← Mega D22 |
| 5 | OUT_B | R66 100R → `OUT_IGN2` |
| 6 | VDD | `Vdrive` (dipilih J13) |
| 7 | OUT_A | R65 100R → `OUT_IGN1` |

### 4.4 IC1 / IC2 BTS4175SGA (high-side switch)

| Pin | Name | IC1 | IC2 |
|---|---|---|---|
| 1 | GND | via R107 120R → GND | via R27 120R → GND |
| 2 | IN | R105 10k ← `HS1` (D45) | R25 10k ← `HS2` (D46) |
| 3 | OUT | `OUT_HS1` → J2-21 | `OUT_HS2` → J2-18 |
| 4 | ST (status) | `HS1_Diag` → D28 | `HS2_Diag` → D29 |
| 5–8 | VS | `12V-SW` | `12V-SW` |

---

## 5. Power Supply

```
J2-24 (12V-SW) ──┬── D26 SS14 ──> 12V_PROT ──> U8 LM2940S-5.0 ──> +5V
                 │                    │  D25 SMBJ40A (TVS), C40 10uF, C42 47uF
                 │                    └── LED1 (R86 2k7) 12V indicator
                 ├── IC1/IC2 VS (high-side switch)
                 ├── Flyback D13, D19–D22, D27, D31 (cathode to 12V)
                 ├── R19 3k9 → A0 (Vbatt sense)
                 └── J13-1 (Vdrive option)

+5V ──> Arduino Mega 5V rail, MCP2515, TJA1051, all pull-ups
        LED2 (R87 470R) 5V indicator
```

**Note:** this board's +5V rail **back-feeds** the Arduino Mega's 5V pin. Think
before powering the Mega from USB and 12 V at the same time.

---

## 6. I/O Capacity Summary

| Type | Count | Pins |
|---|---|---|
| Low-side outputs, small (AO3400A) | 5 | LC1–LC5 (D42, D43, D47, D48, D49) |
| Low-side outputs, high current (NMOS TO-252) | 2 | LS1–LS2 (D39, D40) |
| High-side outputs + diagnostics (BTS4175) | 2 | HS1–HS2 (D45, D46) |
| Logic outputs (TC4424A) | 2 | LOGIC1–LOGIC2 / nets IGN1–IGN2 (D23, D22) — share output pins with LC4/LC5 |
| Analog-only inputs | 2 | A1, A2 (J2-10, J2-11) |
| Switchable analog/digital inputs | 8 | A3–A10 ↔ SW1–SW4 / Hall1–Hall4 |
| Vbatt sense | 1 | A0 |
| CAN | 1 channel | MCP2515 + TJA1051 |
| Serial | 1 (Serial2) | J7 |

**Physical outputs reaching J2 at once: 7** (LS1, LS2, HS1, HS2, LC1, LC2, LC3)
**plus 2 shared** (OUT4, OUT5).
