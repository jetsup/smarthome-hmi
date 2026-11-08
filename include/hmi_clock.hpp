#pragma once
#include <Arduino.h>
#include <time.h>

// The display's clock. Time comes from the gateway (its RTC, internet time or the hub's
// clock, broadcast over ESP-NOW every 30 s); if that is silent the display asks the internet
// itself over Wi-Fi. Shown in local time using the time-zone offset from the lock-screen settings.
namespace hmiclock {
void setFromGateway(uint32_t epoch);
void tick();             // call from loop()
bool valid();            // a real time is known
uint32_t utc();          // seconds since 1970 (0 = unknown)
void local(struct tm& out);  // local broken-down time
}  // namespace hmiclock
