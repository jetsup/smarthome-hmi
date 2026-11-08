#include "assets.hpp"

#include <string.h>

const lv_image_dsc_t* symbolImage(const char* key) {
  const lv_image_dsc_t* chip = nullptr;
  for (int i = 0; i < symbol_icon_count; i++) {
    if (key && strcmp(symbol_icons[i].key, key) == 0) return symbol_icons[i].img;
    if (strcmp(symbol_icons[i].key, "chip") == 0) chip = symbol_icons[i].img;
  }
  return chip;
}

const char* defaultSymbolFor(const char* t) {
  if (!strcmp(t, "digitalOutput")) return "power";
  if (!strcmp(t, "relay")) return "plug";
  if (!strcmp(t, "analogOutput")) return "sliders";
  if (!strcmp(t, "neopixel")) return "lightbulb";
  if (!strcmp(t, "buzzer")) return "bell";
  if (!strcmp(t, "dht")) return "thermometer";
  if (!strcmp(t, "ld2410")) return "radar";
  if (!strcmp(t, "ds3231")) return "clock";
  if (!strcmp(t, "analogInput")) return "gauge";
  if (!strcmp(t, "digitalInput")) return "input";
  return "chip";
}
