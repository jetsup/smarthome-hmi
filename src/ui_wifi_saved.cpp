#include "theme.hpp"
#include "ui_wifi_saved.hpp"

#include <WiFi.h>

#include "display.hpp"
#include "network_manager.hpp"
#include "preferences_manager.hpp"
#include "ui_settings.hpp"
#include "ui_wifi_setup.hpp"

#define C_BG th_panel()
#define C_CARD th_surface()
#define C_ACCENT th_accent()
#define C_TEXT th_text()
#define C_MUTED th_muted()
#define C_ONACC 0xFFFFFFu
static const uint32_t C_OK = 0x3FB950, C_BAD = 0xF85149;

static lv_obj_t* g_list = nullptr;
static lv_obj_t* g_empty = nullptr;

void cleanup_wifi_saved_screen() {
  g_list = nullptr;
  g_empty = nullptr;
}

static lv_obj_t* make_label(lv_obj_t* parent, const char* text, uint32_t color,
                            const lv_font_t* font) {
  lv_obj_t* l = lv_label_create(parent);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_color(l, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
  return l;
}

static lv_obj_t* icon_button(lv_obj_t* parent, const char* symbol, uint32_t color,
                             lv_event_cb_t cb, int index) {
  lv_obj_t* b = lv_button_create(parent);
  lv_obj_set_size(b, 30, 30);
  lv_obj_set_style_bg_color(b, lv_color_hex(0x3A3A44), LV_PART_MAIN);
  lv_obj_set_style_radius(b, 6, LV_PART_MAIN);
  lv_obj_set_style_border_width(b, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
  lv_obj_t* l = make_label(b, symbol, color, &lv_font_montserrat_12);
  lv_obj_center(l);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void*)(intptr_t)index);
  return b;
}

static void populate();

// Rebuild after the click handler has returned (the row we are in is deleted).
static void rebuild_async(void*) {
  if (g_list) populate();
}

static void on_up(lv_event_t* e) {
  PreferencesManager::instance().moveWifi((int)(intptr_t)lv_event_get_user_data(e), -1);
  lv_async_call(rebuild_async, nullptr);
}
static void on_down(lv_event_t* e) {
  PreferencesManager::instance().moveWifi((int)(intptr_t)lv_event_get_user_data(e), +1);
  lv_async_call(rebuild_async, nullptr);
}
static void on_delete(lv_event_t* e) {
  PreferencesManager::instance().removeWifiAt((int)(intptr_t)lv_event_get_user_data(e));
  lv_async_call(rebuild_async, nullptr);
}

static void populate() {
  if (!g_list) return;
  lv_obj_clean(g_list);
  auto& prefs = PreferencesManager::instance();
  int n = prefs.wifiCount();
  if (g_empty) {
    if (n == 0) lv_obj_clear_flag(g_empty, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(g_empty, LV_OBJ_FLAG_HIDDEN);
  }
  String connected = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : String("");

  for (int i = 0; i < n; i++) {
    String ssid, pass;
    prefs.wifiAt(i, ssid, pass);

    lv_obj_t* row = lv_obj_create(g_list);
    lv_obj_set_size(row, LV_PCT(100), 44);
    lv_obj_set_style_bg_color(row, lv_color_hex(C_CARD), LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(row, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    char num[4];
    snprintf(num, sizeof(num), "%d", i + 1);
    lv_obj_t* badge = make_label(row, num, C_ACCENT, &lv_font_montserrat_14);
    lv_obj_align(badge, LV_ALIGN_LEFT_MID, 10, 0);

    // Long names scroll instead of being cut off.
    lv_obj_t* name = make_label(row, ssid.c_str(), C_TEXT, &lv_font_montserrat_12);
    lv_label_set_long_mode(name, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(name, 110);
    lv_obj_align(name, LV_ALIGN_TOP_LEFT, 32, 6);

    bool on = (ssid == connected);
    lv_obj_t* sub = make_label(row, on ? "Connected" : (i == 0 ? "Preferred" : "Saved"),
                               on ? C_OK : C_MUTED, &lv_font_montserrat_10);
    lv_obj_align(sub, LV_ALIGN_BOTTOM_LEFT, 32, -5);

    // up / down / remove
    lv_obj_t* del = icon_button(row, LV_SYMBOL_CLOSE, C_BAD, on_delete, i);
    lv_obj_align(del, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_t* down = icon_button(row, LV_SYMBOL_DOWN, C_TEXT, on_down, i);
    lv_obj_align(down, LV_ALIGN_RIGHT_MID, -40, 0);
    lv_obj_t* up = icon_button(row, LV_SYMBOL_UP, C_TEXT, on_up, i);
    lv_obj_align(up, LV_ALIGN_RIGHT_MID, -74, 0);
    if (i == 0) lv_obj_add_state(up, LV_STATE_DISABLED);
    if (i == n - 1) lv_obj_add_state(down, LV_STATE_DISABLED);
  }
}

void build_wifi_saved_screen() {
  cleanup_wifi_saved_screen();
  lv_obj_t* old = lv_screen_active();

  lv_obj_t* scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), LV_PART_MAIN);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* back = lv_button_create(scr);
  lv_obj_set_size(back, 40, 30);
  lv_obj_align(back, LV_ALIGN_TOP_LEFT, 6, 6);
  lv_obj_set_style_bg_color(back, lv_color_hex(C_CARD), LV_PART_MAIN);
  lv_obj_set_style_radius(back, 4, LV_PART_MAIN);
  lv_obj_set_style_border_width(back, 0, LV_PART_MAIN);
  lv_obj_center(make_label(back, LV_SYMBOL_LEFT, C_TEXT, &lv_font_montserrat_14));
  lv_obj_add_event_cb(
      back,
      [](lv_event_t*) {
        cleanup_wifi_saved_screen();
        build_settings_screen();
      },
      LV_EVENT_CLICKED, nullptr);

  lv_obj_t* title = make_label(scr, "Wi-Fi networks", C_TEXT, &lv_font_montserrat_14);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 56, 8);
  lv_obj_t* hint = make_label(scr, "Tried in this order at start-up", C_MUTED, &lv_font_montserrat_10);
  lv_obj_align(hint, LV_ALIGN_TOP_LEFT, 56, 26);

  g_list = lv_obj_create(scr);
  lv_obj_set_size(g_list, SCREEN_WIDTH - 16, 134);
  lv_obj_align(g_list, LV_ALIGN_TOP_MID, 0, 46);
  lv_obj_set_style_bg_opa(g_list, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_list, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_list, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_row(g_list, 6, LV_PART_MAIN);
  lv_obj_set_flex_flow(g_list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(g_list, LV_DIR_VER);

  g_empty = make_label(scr, "No saved networks.\nAdd one below.", C_MUTED, &lv_font_montserrat_12);
  lv_obj_set_style_text_align(g_empty, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(g_empty, LV_ALIGN_CENTER, 0, -6);

  lv_obj_t* add = lv_button_create(scr);
  lv_obj_set_size(add, SCREEN_WIDTH - 16, 36);
  lv_obj_align(add, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_obj_set_style_bg_color(add, lv_color_hex(C_ACCENT), LV_PART_MAIN);
  lv_obj_set_style_radius(add, 8, LV_PART_MAIN);
  lv_obj_center(make_label(add, LV_SYMBOL_PLUS "  Add network", C_ONACC, &lv_font_montserrat_14));
  lv_obj_add_event_cb(
      add,
      [](lv_event_t*) {
        cleanup_wifi_saved_screen();
        build_wifi_setup_screen(true);
      },
      LV_EVENT_CLICKED, nullptr);

  populate();

  lv_screen_load(scr);
  if (old && old != scr) lv_obj_delete(old);
}
