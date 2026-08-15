#pragma once

// One-shot receiver setup, run from setup() before the bridge starts.
//
// Compiles to an empty call unless GNSS_INIT_K803 is set in config.h, so the
// bridge stays a pure byte pipe for receivers that are already configured.

#include <Arduino.h>

void gnssInit(HardwareSerial &ser);
