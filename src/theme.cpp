#include "theme.hpp"

#include <Preferences.h>
#include <WS2812FX.h>
#include <lvgl.h>

#include "display.hpp"
#include "network_manager.hpp"

extern LGFX tft;
extern WS2812FX ws2812fx;

static Theme g_theme;
static bool g_asleep = false;
static uint32_t g_holdUntil = 0;  // ignore hub values briefly after a local change

// Onboard RGB patterns; the order of names matches the hub's list.
struct LedPattern { const char* name; uint8_t mode; };
static const LedPattern kPatterns[] = {
  {"solid", FX_MODE_STATIC},        {"blink", FX_MODE_BLINK},
  {"breathe", FX_MODE_BREATH},      {"fade", FX_MODE_FADE},
  {"rainbow", FX_MODE_RAINBOW_CYCLE}, {"random", FX_MODE_RANDOM_COLOR},
  {"strobe", FX_MODE_STROBE},       {"twinkle", FX_MODE_TWINKLE_FADE},
  {"flicker", FX_MODE_FIRE_FLICKER}, {"sparkle", FX_MODE_HYPER_SPARKLE},
};
static const int kPatternCount = sizeof(kPatterns) / sizeof(kPatterns[0]);

static uint8_t patternIndex(const char* name) {
  for (int i = 0; i < kPatternCount; i++)
    if (strcmp(kPatterns[i].name, name) == 0) return i;
  return 0;
}

const Theme& theme::get() { return g_theme; }

void theme::loadSaved() {
  Preferences p;
  p.begin("hmi-theme", true);
  g_theme.light = p.getBool("light", false);
  g_theme.accent = p.getUInt("accent", 0x00A8E8);
  g_theme.brightness = p.getUChar("bright", 100);
  g_theme.statusLed = p.getBool("led", true);
  g_theme.background = p.getInt("bg", -1);
  g_theme.ledMode = p.getUChar("lmode", 0);
  g_theme.ledColor = p.getInt("lcol", -1);
  g_theme.ledBrightness = p.getUChar("lbri", 40);
  g_theme.ledSpeed = p.getUChar("lspd", 50);
  g_theme.sleepAfter = p.getUShort("sleep", 0);
  g_theme.ledSleepOff = p.getBool("lsoff", true);
  p.end();
}

bool theme::update(const ThemeUpdate& u) {
  if (millis() < g_holdUntil) return false;  // a local change is still on its way to the hub
  uint8_t brightness = constrain(u.brightness, 10, 100);
  uint8_t mode = patternIndex(u.ledMode ? u.ledMode : "solid");
  bool lookChanged = (u.light != g_theme.light) || (u.accent != g_theme.accent) ||
                     (u.background != g_theme.background);
  bool changed = lookChanged || brightness != g_theme.brightness || u.statusLed != g_theme.statusLed ||
                 mode != g_theme.ledMode || u.ledColor != g_theme.ledColor ||
                 u.ledBrightness != g_theme.ledBrightness || u.ledSpeed != g_theme.ledSpeed ||
                 u.sleepAfter != g_theme.sleepAfter || u.ledSleepOff != g_theme.ledSleepOff;
  g_theme.light = u.light;
  g_theme.accent = u.accent;
  g_theme.brightness = brightness;
  g_theme.statusLed = u.statusLed;
  g_theme.background = u.background;
  g_theme.ledMode = mode;
  g_theme.ledColor = u.ledColor;
  g_theme.ledBrightness = constrain(u.ledBrightness, 1, 100);
  g_theme.ledSpeed = constrain(u.ledSpeed, 1, 100);
  g_theme.sleepAfter = u.sleepAfter;
  g_theme.ledSleepOff = u.ledSleepOff;
  if (changed) {
    Preferences p;
    p.begin("hmi-theme", false);
    p.putBool("light", g_theme.light);
    p.putUInt("accent", g_theme.accent);
    p.putUChar("bright", g_theme.brightness);
    p.putBool("led", g_theme.statusLed);
    p.putInt("bg", g_theme.background);
    p.putUChar("lmode", g_theme.ledMode);
    p.putInt("lcol", g_theme.ledColor);
    p.putUChar("lbri", g_theme.ledBrightness);
    p.putUChar("lspd", g_theme.ledSpeed);
    p.putUShort("sleep", g_theme.sleepAfter);
    p.putBool("lsoff", g_theme.ledSleepOff);
    p.end();
    applyHardware();
  }
  return lookChanged;
}

void theme::applyHardware() {
  tft.setBrightness(g_asleep ? 0 : (uint16_t)g_theme.brightness * 255 / 100);
  // The rainbow LED plays during boot; from the dashboard on it follows the
  // pattern / colour / brightness / speed chosen in the app.
  if (!g_theme.statusLed || (g_asleep && g_theme.ledSleepOff)) {
    ws2812fx.setMode(FX_MODE_STATIC);
    ws2812fx.setColor(0x000000);
    ws2812fx.setBrightness(0);
    return;
  }
  uint32_t col = g_theme.ledColor >= 0 ? (uint32_t)g_theme.ledColor : g_theme.accent;
  if (col == 0) col = 0xFFFFFF;
  // speed 1..100 -> WS2812FX period 4000 ms (slow) .. 150 ms (fast)
  uint16_t period = 4000 - (uint32_t)(g_theme.ledSpeed - 1) * 3850 / 99;
  ws2812fx.setColor(col);
  ws2812fx.setSpeed(period);
  ws2812fx.setBrightness((uint16_t)g_theme.ledBrightness * 255 / 100);
  ws2812fx.setMode(kPatterns[g_theme.ledMode].mode);
}

bool theme::asleep() { return g_asleep; }

void theme::wake() {
  lv_display_trigger_activity(nullptr);
  if (!g_asleep) return;
  g_asleep = false;
  applyHardware();
}

void theme::tickPower() {
  if (g_theme.sleepAfter == 0) {
    if (g_asleep) wake();
    return;
  }
  if (!g_asleep && lv_display_get_inactive_time(nullptr) >= (uint32_t)g_theme.sleepAfter * 1000UL) {
    g_asleep = true;
    applyHardware();
  }
}

void theme::setDisplay(uint8_t brightness, uint8_t ledBrightness, uint16_t sleepAfter, bool ledSleepOff, bool save) {
  g_theme.brightness = constrain(brightness, 10, 100);
  g_theme.ledBrightness = constrain(ledBrightness, 1, 100);
  g_theme.sleepAfter = sleepAfter;
  g_theme.ledSleepOff = ledSleepOff;
  applyHardware();
  if (!save) return;
  Preferences p;
  p.begin("hmi-theme", false);
  p.putUChar("bright", g_theme.brightness);
  p.putUChar("lbri", g_theme.ledBrightness);
  p.putUShort("sleep", g_theme.sleepAfter);
  p.putBool("lsoff", g_theme.ledSleepOff);
  p.end();
  g_holdUntil = millis() + 8000;
  String body = String("{\"brightness\":") + g_theme.brightness + ",\"ledBrightness\":" + g_theme.ledBrightness +
                ",\"sleepAfter\":" + g_theme.sleepAfter + ",\"ledSleepOff\":" + (g_theme.ledSleepOff ? "true" : "false") + "}";
  NetworkManager::instance().postAsync("/api/hmi/display", body);
}
