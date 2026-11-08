#include "ui_display_screen.hpp"

#include "theme.hpp"
#include "ui_settings.hpp"

namespace {
struct SleepOpt { uint16_t sec; const char* label; };
const SleepOpt kSleep[] = {{0, "Never"},   {15, "15 s"},  {30, "30 s"},   {60, "1 min"},
                           {120, "2 min"}, {300, "5 min"}, {600, "10 min"}, {1800, "30 min"}};
const int kSleepN = sizeof(kSleep) / sizeof(kSleep[0]);

uint8_t g_bright, g_ledBright;
uint16_t g_sleep;
bool g_ledOff;
lv_obj_t* g_sleepLbl = nullptr;

int sleepIndex() {
  for (int i = 0; i < kSleepN; i++)
    if (kSleep[i].sec == g_sleep) return i;
  return 0;
}
void commit() { theme::setDisplay(g_bright, g_ledBright, g_sleep, g_ledOff, true); }
void preview() { theme::setDisplay(g_bright, g_ledBright, g_sleep, g_ledOff, false); }

lv_obj_t* makeRow(lv_obj_t* panel, const char* title) {
  lv_obj_t* row = lv_obj_create(panel);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_height(row, 52);
  lv_obj_set_style_bg_color(row, lv_color_hex(th_panel()), LV_PART_MAIN);
  lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(row, 4, LV_PART_MAIN);
  lv_obj_set_style_pad_all(row, 8, LV_PART_MAIN);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* l = lv_label_create(row);
  lv_label_set_text(l, title);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_10, LV_PART_MAIN);
  lv_obj_set_style_text_color(l, lv_color_hex(th_text()), LV_PART_MAIN);
  lv_obj_align(l, LV_ALIGN_TOP_LEFT, 0, -2);
  return row;
}

void addSlider(lv_obj_t* panel, const char* title, int lo, int hi, int val, bool led) {
  lv_obj_t* row = makeRow(panel, title);
  lv_obj_t* s = lv_slider_create(row);
  lv_slider_set_range(s, lo, hi);
  lv_slider_set_value(s, val, LV_ANIM_OFF);
  lv_obj_set_width(s, 190);
  lv_obj_align(s, LV_ALIGN_BOTTOM_LEFT, 4, -2);
  lv_obj_set_style_bg_color(s, lv_color_hex(th_accent()), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(s, lv_color_hex(th_accent()), LV_PART_KNOB);
  lv_obj_t* v = lv_label_create(row);
  lv_label_set_text_fmt(v, "%d%%", val);
  lv_obj_set_style_text_font(v, &lv_font_montserrat_10, LV_PART_MAIN);
  lv_obj_set_style_text_color(v, lv_color_hex(th_muted()), LV_PART_MAIN);
  lv_obj_align(v, LV_ALIGN_BOTTOM_RIGHT, 0, -2);
  lv_obj_set_user_data(s, v);
  lv_obj_add_event_cb(
      s,
      [](lv_event_t* e) {
        lv_obj_t* sl = (lv_obj_t*)lv_event_get_target(e);
        bool isLed = lv_event_get_user_data(e) != nullptr;
        int x = lv_slider_get_value(sl);
        if (isLed) g_ledBright = x; else g_bright = x;
        lv_label_set_text_fmt((lv_obj_t*)lv_obj_get_user_data(sl), "%d%%", x);
        preview();
      },
      LV_EVENT_VALUE_CHANGED, led ? (void*)1 : nullptr);
  lv_obj_add_event_cb(s, [](lv_event_t*) { commit(); }, LV_EVENT_RELEASED, nullptr);
}

void stepSleep(int dir) {
  int i = sleepIndex() + dir;
  if (i < 0) i = 0;
  if (i >= kSleepN) i = kSleepN - 1;
  g_sleep = kSleep[i].sec;
  if (g_sleepLbl) lv_label_set_text(g_sleepLbl, kSleep[i].label);
  commit();
}

lv_obj_t* stepBtn(lv_obj_t* row, const char* txt, lv_align_t al, int x) {
  lv_obj_t* b = lv_button_create(row);
  lv_obj_set_size(b, 40, 26);
  lv_obj_align(b, al, x, 6);
  lv_obj_set_style_bg_color(b, lv_color_hex(th_surface()), LV_PART_MAIN);
  lv_obj_set_style_radius(b, 4, LV_PART_MAIN);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_color(l, lv_color_hex(th_text()), LV_PART_MAIN);
  lv_obj_center(l);
  return b;
}
}  // namespace

void build_display_screen() {
  lv_obj_t* old_screen = lv_screen_active();
  const Theme& t = theme::get();
  g_bright = t.brightness;
  g_ledBright = t.ledBrightness;
  g_sleep = t.sleepAfter;
  g_ledOff = t.ledSleepOff;

  lv_obj_t* scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, lv_color_hex(th_page()), LV_PART_MAIN);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* back_btn = lv_button_create(scr);
  lv_obj_set_size(back_btn, 36, 28);
  lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 6, 6);
  lv_obj_set_style_bg_color(back_btn, lv_color_hex(th_surface()), LV_PART_MAIN);
  lv_obj_set_style_radius(back_btn, 4, LV_PART_MAIN);
  lv_obj_set_style_border_width(back_btn, 0, LV_PART_MAIN);
  lv_obj_t* back_lbl = lv_label_create(back_btn);
  lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
  lv_obj_set_style_text_color(back_lbl, lv_color_hex(th_text()), LV_PART_MAIN);
  lv_obj_center(back_lbl);
  lv_obj_add_event_cb(back_btn, [](lv_event_t*) { build_settings_screen(); }, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* title = lv_label_create(scr);
  lv_label_set_text(title, "Display");
  lv_obj_set_style_text_color(title, lv_color_hex(th_text()), LV_PART_MAIN);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

  lv_obj_t* div = lv_obj_create(scr);
  lv_obj_set_size(div, 280, 1);
  lv_obj_align(div, LV_ALIGN_TOP_MID, 0, 38);
  lv_obj_set_style_bg_color(div, lv_color_hex(th_surface()), LV_PART_MAIN);
  lv_obj_set_style_border_width(div, 0, LV_PART_MAIN);

  lv_obj_t* panel = lv_obj_create(scr);
  lv_obj_set_size(panel, 290, 190);
  lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 45);
  lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(panel, LV_DIR_VER);
  lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_row(panel, 4, LV_PART_MAIN);

  addSlider(panel, "Screen brightness", 10, 100, g_bright, false);

  // Screen-off timer: [-]  value  [+]
  lv_obj_t* row = makeRow(panel, "Turn the screen off after");
  stepBtn(row, LV_SYMBOL_MINUS, LV_ALIGN_BOTTOM_LEFT, 0);
  g_sleepLbl = lv_label_create(row);
  lv_label_set_text(g_sleepLbl, kSleep[sleepIndex()].label);
  lv_obj_set_style_text_font(g_sleepLbl, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_set_style_text_color(g_sleepLbl, lv_color_hex(th_text()), LV_PART_MAIN);
  lv_obj_align(g_sleepLbl, LV_ALIGN_BOTTOM_MID, 0, -6);
  lv_obj_t* plus = stepBtn(row, LV_SYMBOL_PLUS, LV_ALIGN_BOTTOM_RIGHT, 0);
  lv_obj_add_event_cb(lv_obj_get_child(row, 1), [](lv_event_t*) { stepSleep(-1); }, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(plus, [](lv_event_t*) { stepSleep(1); }, LV_EVENT_CLICKED, nullptr);

  addSlider(panel, "RGB LED brightness", 1, 100, g_ledBright, true);

  lv_obj_t* r3 = makeRow(panel, "Turn the RGB LED off with the screen");
  lv_obj_t* sw = lv_switch_create(r3);
  lv_obj_align(sw, LV_ALIGN_BOTTOM_RIGHT, 0, 2);
  lv_obj_set_size(sw, 40, 20);
  if (g_ledOff) lv_obj_add_state(sw, LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(sw, lv_color_hex(th_accent()), (lv_style_selector_t)LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_add_event_cb(
      sw,
      [](lv_event_t* e) {
        g_ledOff = lv_obj_has_state((lv_obj_t*)lv_event_get_target(e), LV_STATE_CHECKED);
        commit();
      },
      LV_EVENT_VALUE_CHANGED, nullptr);

  lv_screen_load(scr);
  if (old_screen != nullptr) lv_obj_delete(old_screen);
}
