#pragma once

// ---------------------------------------------------------------------------
// Wiring / UART
// ---------------------------------------------------------------------------
// Defaults below are for the ESP32 DevKitC on UART2 (Serial2 default pins).
// Each build env in platformio.ini may override them via -D flags.
//
//   ESP32 DevKitC          GPIO16 = RX2  <- GNSS TX
//                          GPIO17 = TX2  -> GNSS RX
//
// Do NOT use the pins silkscreened TX0/RX0 (GPIO1/GPIO3): they are wired to the
// onboard USB-serial chip. Putting the GNSS there breaks flashing and the
// serial monitor, and the GNSS will fight the USB bridge for the line.
//
// Note: GPIO16/17 are free on ESP32-WROOM-32 modules but are consumed by PSRAM
// on ESP32-WROVER. On a WROVER, move the GNSS to e.g. GPIO25/GPIO26.

#ifndef GNSS_UART_NUM
#define GNSS_UART_NUM 2
#endif
#ifndef PIN_GNSS_RX
#define PIN_GNSS_RX 16  // ESP32 in  <- GNSS TX  (NMEA / UBX)
#endif
#ifndef PIN_GNSS_TX
#define PIN_GNSS_TX 17  // ESP32 out -> GNSS RX  (RTCM corrections)
#endif

#define GNSS_BAUD 115200

// Tie GNSS GND to the board GND. Power a multi-band RTK receiver from its own
// supply, not the DevKitC 3V3 pin.

// ---------------------------------------------------------------------------
// Receiver init - ComNav K803 Lite as an RTK rover
// ---------------------------------------------------------------------------
// Runs once in setup(), before the bridge starts, and replaces the PC-side
// Python setup script. Safe to repeat every boot: it first pulls the port back
// out of RTCM auto-detect, where the receiver ignores ASCII commands.
//
// Set to 0 for a receiver that is already configured, or one that does not
// speak the ComNav/NovAtel command set (u-blox, Septentrio, ...).
#define GNSS_INIT_K803 1

// The receiver port this ESP32 UART is wired to - LOG targets and the final
// INTERFACEMODE name it. Not the PC's COM port.
#define GNSS_CMD_PORT "COM1"

// Per-command settle time. The whole sequence costs roughly
// 2 s + (commands x this), so ~14 s of boot before the bridge comes up.
#define GNSS_CMD_WAIT_MS 600

// ---------------------------------------------------------------------------
// Phone link name (both SPP and BLE advertise this)
// ---------------------------------------------------------------------------
// BLE builds append "-XXXX", the last two octets of the radio's own address, so
// two rovers in the same field are tellable apart in a scan list and a phone's
// bond for one is never reused for the other. SPP builds use the name as-is.
#define LINK_DEVICE_NAME "ESP32-RTK"

// ---------------------------------------------------------------------------
// Bluetooth Classic / SPP  (LINK_SPP builds; ESP32 only, not C3/C6/S3)
// ---------------------------------------------------------------------------
// Leave undefined for open pairing. Define to force a legacy PIN, which some
// older Android versions still prompt for.
// #define SPP_PIN "1234"

// ---------------------------------------------------------------------------
// BLE  (LINK_BLE builds)
// ---------------------------------------------------------------------------
#define BLE_TX_POWER_DBM 9  // -12..+20; higher = more range, more current
#define BLE_ENABLE_BONDING 1

// Nordic UART Service - what SW Maps, Lefebure, GNSS Master expect over BLE.
#define NUS_SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_RX_CHAR_UUID "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  // phone writes
#define NUS_TX_CHAR_UUID "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  // phone subscribes

#define BLE_NOTIFY_MAX 244  // payload at MTU 247; clamped to real MTU at runtime
#define BLE_DOWNLINK_BUF 4096

// ---------------------------------------------------------------------------
// Buffering
// ---------------------------------------------------------------------------
#define UART_RX_BUF 4096  // GNSS -> us; must absorb a radio stall
#define UART_TX_BUF 2048  // us -> GNSS
#define BRIDGE_CHUNK 512  // max bytes moved per pump in either direction

// Flush a partial packet after this long with no new GNSS bytes, so the tail of
// an NMEA burst is not held back waiting for a full chunk.
#define UPLINK_FLUSH_MS 12

// ---------------------------------------------------------------------------
// Debug / LED
// ---------------------------------------------------------------------------
#define DEBUG_USB 1  // stats + link state on the USB console (UART0)
#define DEBUG_STATS_MS 5000

#define USE_STATUS_LED 1
#ifndef LED_ACTIVE_LOW
#define LED_ACTIVE_LOW 0  // DevKitC GPIO2 LED is active HIGH
#endif
