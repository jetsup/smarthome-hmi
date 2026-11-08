#pragma once
#include <Arduino.h>
#include <stdint.h>

// Appearance chosen from the app / website and stored on the hub per display.
struct Theme {
  bool light = false;
  uint32_t accent = 0x00A8E8;
  uint8_t brightness = 100;  // 10-100 %
  bool statusLed = true;
  int32_t background = -1;  // custom page colour, -1 = theme default
  uint8_t ledMode = 0;      // see theme.cpp (solid, blink, breathe, ...)
  int32_t ledColor = -1;    // onboard RGB colour, -1 = follow the accent
  uint8_t ledBrightness = 40;  // 1-100 %
  uint8_t ledSpeed = 50;       // 1-100
  uint16_t sleepAfter = 0;     // seconds without touch before the screen goes off, 0 = never
  bool ledSleepOff = true;     // switch the RGB LED off while the screen is off
};

// The whole appearance block as received from the hub.
struct ThemeUpdate {
  bool light;
  uint32_t accent;
  uint8_t brightness;
  bool statusLed;
  int32_t background;
  const char* ledMode;
  int32_t ledColor;
  uint8_t ledBrightness;
  uint8_t ledSpeed;
  uint16_t sleepAfter;
  bool ledSleepOff;
};

namespace theme {
const Theme& get();
// Update from the hub's "appearance" object. Returns true if the LOOK changed
// (theme/accent) and screens must be rebuilt; brightness/LED apply instantly.
bool update(const ThemeUpdate& u);
void loadSaved();  // at boot, before any screen is built
void applyHardware();  // backlight + status LED

// Power saving: the screen (and optionally the RGB LED) switch off after a while
// without touch; the first touch wakes them and is not passed on to the UI.
bool asleep();
void wake();
void tickPower();  // call from loop()
// Change the power settings from the display's own Settings screen. `save` stores
// them and tells the hub (so the app and website agree); otherwise it is a live preview.
void setDisplay(uint8_t brightness, uint8_t ledBrightness, uint16_t sleepAfter, bool ledSleepOff, bool save);
}  // namespace theme

// Palette (dark / light, or derived from a custom background colour)
static inline uint32_t th_mix(uint32_t a, uint32_t b, int pct) {  // pct % of b over a
  uint32_t out = 0;
  for (int sh = 16; sh >= 0; sh -= 8) {
    int ca = (a >> sh) & 0xFF, cb = (b >> sh) & 0xFF;
    out |= (uint32_t)(ca + (cb - ca) * pct / 100) << sh;
  }
  return out;
}
static inline bool th_bg_is_light(uint32_t c) {
  return ((c >> 16 & 0xFF) * 299 + (c >> 8 & 0xFF) * 587 + (c & 0xFF) * 114) / 1000 > 140;
}
// "Light" means the page behind the widgets is light - decided by the custom
// background when there is one, otherwise by the theme.
static inline bool th_is_light() {
  const Theme& t = theme::get();
  return t.background >= 0 ? th_bg_is_light((uint32_t)t.background) : t.light;
}
static inline uint32_t th_page() {
  const Theme& t = theme::get();
  if (t.background >= 0) return (uint32_t)t.background;
  return t.light ? 0xF2F4F7 : 0x121214;
}
static inline uint32_t th_panel() {
  const Theme& t = theme::get();
  if (t.background >= 0) return th_mix((uint32_t)t.background, th_is_light() ? 0xFFFFFF : 0xFFFFFF, th_is_light() ? 70 : 7);
  return t.light ? 0xFFFFFF : 0x1E1E24;
}
static inline uint32_t th_surface() {
  const Theme& t = theme::get();
  if (t.background >= 0) return th_mix((uint32_t)t.background, th_is_light() ? 0x000000 : 0xFFFFFF, th_is_light() ? 8 : 14);
  return t.light ? 0xE4E7EC : 0x2D2D35;
}
static inline uint32_t th_text() { return th_is_light() ? 0x111418 : 0xFFFFFF; }
static inline uint32_t th_muted() { return th_is_light() ? 0x5F6B7A : 0x8A8A8A; }
static inline uint32_t th_dim() { return th_is_light() ? 0x8A94A3 : 0x656565; }
static inline uint32_t th_track() { return th_is_light() ? 0xC9CED6 : 0x404040; }
// Accent: a white accent turns near-black on a light page (and black turns
// white on a dark one) so it always stays readable.
static inline uint32_t th_accent() {
  uint32_t a = theme::get().accent;
  bool aLight = th_bg_is_light(a);
  if (th_is_light() && a == 0xFFFFFF) return 0x111418;
  if (!th_is_light() && a == 0x000000) return 0xFFFFFF;
  (void)aLight;
  return a;
}
