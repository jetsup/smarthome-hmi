#include "theme.hpp"

#include <Preferences.h>
#include <WS2812FX.h>

#include "display.hpp"

extern LGFX tft;
extern WS2812FX ws2812fx;

static Theme g_theme;
static bool g_dashboard_led_owned = false;

const Theme& theme::get() { return g_theme; }

void theme::loadSaved() {
  Preferences p;
  p.begin("hmi-theme", true);
  g_theme.light = p.getBool("light", false);
  g_theme.accent = p.getUInt("accent", 0x00A8E8);
  g_theme.brightness = p.getUChar("bright", 100);
  g_theme.statusLed = p.getBool("led", true);
  p.end();
}

bool theme::update(bool light, uint32_t accent, uint8_t brightness, bool statusLed) {
  if (brightness < 10) brightness = 10;
  if (brightness > 100) brightness = 100;
  bool lookChanged = (light != g_theme.light) || (accent != g_theme.accent);
  bool changed = lookChanged || brightness != g_theme.brightness || statusLed != g_theme.statusLed;
  g_theme.light = light;
  g_theme.accent = accent;
  g_theme.brightness = brightness;
  g_theme.statusLed = statusLed;
  if (changed) {
    Preferences p;
    p.begin("hmi-theme", false);
    p.putBool("light", light);
    p.putUInt("accent", accent);
    p.putUChar("bright", brightness);
    p.putBool("led", statusLed);
    p.end();
    applyHardware();
  }
  return lookChanged;
}

void theme::applyHardware() {
  tft.setBrightness((uint16_t)g_theme.brightness * 255 / 100);
  // The rainbow LED plays during boot; afterwards the dashboard turns it off.
  // "Status LED on" shows a dim accent glow on the dashboard, off = dark.
  if (g_dashboard_led_owned || true) {
    if (g_theme.statusLed) {
      ws2812fx.setMode(FX_MODE_STATIC);
      ws2812fx.setColor(g_theme.accent);
      ws2812fx.setBrightness(40);
    } else {
      ws2812fx.setMode(FX_MODE_STATIC);
      ws2812fx.setColor(0x000000);
      ws2812fx.setBrightness(0);
    }
  }
}
