#include "hmi_clock.hpp"

#include <WiFi.h>

#include "lockscreen.hpp"

namespace {
uint32_t g_base = 0;       // epoch at g_baseMs
uint32_t g_baseMs = 0;
uint32_t g_lastGateway = 0;  // millis() of the last time the gateway told us
bool g_ntpStarted = false;
}  // namespace

void hmiclock::setFromGateway(uint32_t epoch) {
  if (epoch < 1600000000UL) return;
  g_base = epoch;
  g_baseMs = millis();
  g_lastGateway = millis() ? millis() : 1;
}

bool hmiclock::valid() { return g_base != 0; }

uint32_t hmiclock::utc() {
  if (!g_base) return 0;
  return g_base + (millis() - g_baseMs) / 1000;
}

void hmiclock::local(struct tm& out) {
  time_t t = (time_t)utc() + (time_t)lockscreen::get().tzMinutes * 60;
  gmtime_r(&t, &out);
}

void hmiclock::tick() {
  static uint32_t last = 0;
  if (millis() - last < 5000) return;
  last = millis();
  // The gateway is the better source; ask the internet only if it has been silent.
  bool gatewayFresh = g_lastGateway && millis() - g_lastGateway < 300000UL;
  if (gatewayFresh || WiFi.status() != WL_CONNECTED) return;
  if (!g_ntpStarted) {
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    g_ntpStarted = true;
  }
  time_t now = time(nullptr);
  if (now > 1600000000L) {
    g_base = (uint32_t)now;
    g_baseMs = millis();
  }
}
