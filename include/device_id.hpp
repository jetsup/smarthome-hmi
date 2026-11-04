#pragma once
#include <Arduino.h>

// Stable hardware identity of this display (its MAC, 12 hex chars). The hub
// uses it to recognise the same display if it is paired again.
inline String hmiDeviceId() {
  uint64_t m = ESP.getEfuseMac();
  char b[16];
  snprintf(b, sizeof(b), "%02X%02X%02X%02X%02X%02X", (uint8_t)(m), (uint8_t)(m >> 8),
           (uint8_t)(m >> 16), (uint8_t)(m >> 24), (uint8_t)(m >> 32), (uint8_t)(m >> 40));
  return String(b);
}

// Last 4 hex digits, shown on screen so the user can tell displays apart.
inline String hmiShortId() {
  String id = hmiDeviceId();
  return id.substring(id.length() - 4);
}
