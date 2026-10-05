#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

#ifndef NODE_ID
#define NODE_ID 0
#endif
#if NODE_ID < 0 || NODE_ID > 3
#error "NODE_ID must be 0..3 — only four ID blocks are reserved"
#endif

#define FW_VERSION 1

uint8_t settingsNode();
#define CAN_BASE_ID       (0x640u + (settingsNode() * 0x10u))

#define CAN_ID_ANALOG_A   (CAN_BASE_ID + 0x0)
#define CAN_ID_ANALOG_B   (CAN_BASE_ID + 0x1)
#define CAN_ID_ANALOG_C   (CAN_BASE_ID + 0x2)
#define CAN_ID_DIGITAL    (CAN_BASE_ID + 0x3)
#define CAN_ID_FREQ       (CAN_BASE_ID + 0x4)

#define CAN_ID_OUTPUT_CMD (CAN_BASE_ID + 0x8)
#define CAN_ID_HALL       (CAN_BASE_ID + 0x9)
#define CAN_ID_CAL        (CAN_BASE_ID + 0x5)
#define CAN_ID_CAL2       (CAN_BASE_ID + 0x6)
#define CAN_ID_CAL3       (CAN_BASE_ID + 0x7)

#define PIN_CAN_CS   9

#define PIN_CAN_INT  2

#ifdef MCP_CRYSTAL_16MHZ
  #define MCP_CRYSTAL MCP_16MHZ
  #define MCP_SUPPORTS_1MBPS 1
#else
  #define MCP_CRYSTAL MCP_8MHZ
  #define MCP_SUPPORTS_1MBPS 0
#endif

#define PIN_LC1  42
#define PIN_LC2  43
#define PIN_LC3  47
#define PIN_LC4  48
#define PIN_LC5  49

#define PIN_LS1  39
#define PIN_LS2  40

#define PIN_HS1  45
#define PIN_HS2  46

#define PIN_IGN1 23
#define PIN_IGN2 22

#define PIN_HS1_DIAG 28
#define PIN_HS2_DIAG 29

#define PIN_SW1 24
#define PIN_SW2 25
#define PIN_SW3 26
#define PIN_SW4 27

#define PIN_HALL1 18
#define PIN_HALL2 19
#define PIN_HALL3 20
#define PIN_HALL4 21

#define PIN_AV0  A0
#define ANALOG_CHANNEL_COUNT 11

#define VBATT_PRESENT_MV 6000

#define VBATT_DIVIDER_RATIO 4.9f

#ifndef ADC_VREF_MV
#define ADC_VREF_MV 5000
#endif

#define ADC_OVERSAMPLE               4
#define ANALOG_FILTER_TICK_MS        5
#define DIGITAL_SAMPLE_INTERVAL_MS   5
#define FREQ_WINDOW_MS              20

#define TX_FAST_MS                  20

#define TX_DIGITAL_IDLE_MS         500
#define TX_ANALOG_IDLE_MS          200
#define TX_FREQ_INTERVAL_MS         20

#define TX_ANALOG_THRESHOLD_MV      10

#define TX_CAL_THRESHOLD             2

#define OUTPUT_TIMEOUT_MS          500

#endif
