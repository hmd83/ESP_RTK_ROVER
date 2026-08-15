// Bluetooth Classic (SPP) transport.
//
// This is the profile a plain USB-TTL/HC-05 style adapter presents: the phone
// pairs with it from the system Bluetooth menu, and every GNSS app that offers
// "choose a paired Bluetooth device" can then use it. ESP32 only - the C3, C6
// and S3 have no Bluetooth Classic radio.

#include <Arduino.h>
#include <string.h>

#include "config.h"
#include "link.h"

#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error "Bluetooth Classic is not enabled in this framework build."
#endif
#if !defined(CONFIG_BT_SPP_ENABLED)
#error "SPP is not enabled in this framework build."
#endif

static BluetoothSerial SerialBT;

void linkBegin() {
  SerialBT.begin(LINK_DEVICE_NAME);
#ifdef SPP_PIN
  SerialBT.setPin(SPP_PIN, strlen(SPP_PIN));
#endif
}

bool linkReady() { return SerialBT.hasClient(); }

size_t linkMaxChunk() { return BRIDGE_CHUNK; }

bool linkWrite(const uint8_t *data, size_t len) {
  if (!SerialBT.hasClient()) return false;
  return SerialBT.write(data, len) == len;
}

size_t linkRead(uint8_t *buf, size_t max) {
  size_t n = 0;
  while (n < max && SerialBT.available()) buf[n++] = (uint8_t)SerialBT.read();
  return n;
}

const char *linkName() { return "SPP"; }

// Classic pairings are stored by name on the phone; changing it per board would
// orphan every existing bond, so SPP keeps the plain name.
const char *linkDeviceName() { return LINK_DEVICE_NAME; }
