#pragma once
#include <Arduino.h>
#include <stdint.h>

// Appearance chosen from the app / website and stored on the hub per display.
struct Theme {
  bool light = false;
  uint32_t accent = 0x00A8E8;
  uint8_t brightness = 100;  // 10-100 %
  bool statusLed = true;
};

namespace theme {
const Theme& get();
// Update from the hub's "appearance" object. Returns true if the LOOK changed
// (theme/accent) and screens must be rebuilt; brightness/LED apply instantly.
bool update(bool light, uint32_t accent, uint8_t brightness, bool statusLed);
void loadSaved();  // at boot, before any screen is built
void applyHardware();  // backlight + status LED
}  // namespace theme

// Palette (dark / light)
static inline uint32_t th_page() { return theme::get().light ? 0xF2F4F7 : 0x121214; }
static inline uint32_t th_panel() { return theme::get().light ? 0xFFFFFF : 0x1E1E24; }
static inline uint32_t th_surface() { return theme::get().light ? 0xE4E7EC : 0x2D2D35; }
static inline uint32_t th_text() { return theme::get().light ? 0x111418 : 0xFFFFFF; }
static inline uint32_t th_muted() { return theme::get().light ? 0x5F6B7A : 0x8A8A8A; }
static inline uint32_t th_dim() { return theme::get().light ? 0x8A94A3 : 0x656565; }
static inline uint32_t th_track() { return theme::get().light ? 0xC9CED6 : 0x404040; }
static inline uint32_t th_accent() { return theme::get().accent; }
