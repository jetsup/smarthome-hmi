#include <WS2812FX.h>
#include <lvgl.h>

#include "display.hpp"

extern LGFX tft;
extern WS2812FX ws2812fx;

void on_slider_event(lv_event_t* e) {
  lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
  uint16_t value = lv_slider_get_value(slider);
  tft.setBrightness(value);
  ws2812fx.setBrightness(value);
}

void slider_change_cb(lv_event_t* e) {
  lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
  uint16_t value = lv_slider_get_value(slider);
  tft.setBrightness(value);
}

void arc_change_cb(lv_event_t* e) {
  lv_obj_t* arc = (lv_obj_t*)lv_event_get_target(e);
  uint16_t value = lv_arc_get_value(arc);
  ws2812fx.setBrightness(value);
}

void button_click_cb(lv_event_t* e) {
  ws2812fx.setColor(random(0, 255), random(0, 255), random(0, 255));
}

void color_changed_cb(lv_event_t* e) {
  lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(e);
  lv_color_t c = lv_obj_get_style_bg_color(obj, LV_PART_MAIN);
  int32_t color = lv_color_to_u32(c);
  ws2812fx.setColor(color);
}
