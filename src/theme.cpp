#include "theme.hpp"

#include <Preferences.h>
#include <WS2812FX.h>

#include "display.hpp"

extern LGFX tft;
extern WS2812FX ws2812fx;

static Theme g_theme;

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
  p.end();
}

bool theme::update(const ThemeUpdate& u) {
  uint8_t brightness = constrain(u.brightness, 10, 100);
  uint8_t mode = patternIndex(u.ledMode ? u.ledMode : "solid");
  bool lookChanged = (u.light != g_theme.light) || (u.accent != g_theme.accent) ||
                     (u.background != g_theme.background);
  bool changed = lookChanged || brightness != g_theme.brightness || u.statusLed != g_theme.statusLed ||
                 mode != g_theme.ledMode || u.ledColor != g_theme.ledColor ||
                 u.ledBrightness != g_theme.ledBrightness || u.ledSpeed != g_theme.ledSpeed;
  g_theme.light = u.light;
  g_theme.accent = u.accent;
  g_theme.brightness = brightness;
  g_theme.statusLed = u.statusLed;
  g_theme.background = u.background;
  g_theme.ledMode = mode;
  g_theme.ledColor = u.ledColor;
  g_theme.ledBrightness = constrain(u.ledBrightness, 1, 100);
  g_theme.ledSpeed = constrain(u.ledSpeed, 1, 100);
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
    p.end();
    applyHardware();
  }
  return lookChanged;
}

void theme::applyHardware() {
  tft.setBrightness((uint16_t)g_theme.brightness * 255 / 100);
  // The rainbow LED plays during boot; from the dashboard on it follows the
  // pattern / colour / brightness / speed chosen in the app.
  if (!g_theme.statusLed) {
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
