#include "ui_wifi_screen.hpp"

#include "network_manager.hpp"
#include "preferences_manager.hpp"
#include "ui_managers.hpp"
#include "ui_screens.hpp"

static lv_obj_t* ssid_ta;
static lv_obj_t* pass_ta;
static lv_obj_t* show_pass_cb;
static lv_obj_t* connect_btn;
static lv_obj_t* status_label;
static lv_obj_t* spinner;
static lv_obj_t* kb;

static lv_timer_t* connect_timer = nullptr;
static unsigned long connect_start = 0;

void cleanup_wifi_screen() {
  if (connect_timer != nullptr) {
    lv_timer_del(connect_timer);
    connect_timer = nullptr;
  }
  ssid_ta = nullptr;
  pass_ta = nullptr;
  show_pass_cb = nullptr;
  connect_btn = nullptr;
  status_label = nullptr;
  spinner = nullptr;
  kb = nullptr;
}

static void on_show_pass_changed(lv_event_t* e) {
  bool checked = lv_obj_has_state(show_pass_cb, LV_STATE_CHECKED);
  lv_textarea_set_password_mode(pass_ta, checked ? false : true);
}

static void kb_ready_cb(lv_event_t* e) {
  lv_obj_t* ta = lv_keyboard_get_textarea(kb);
  if (ta) {
    lv_obj_clear_state(ta, LV_STATE_FOCUSED);
  }
  lv_keyboard_set_textarea(kb, NULL);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
}

static void ta_focus_cb(lv_event_t* e) {
  lv_obj_t* ta = (lv_obj_t*)lv_event_get_target(e);
  lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
  lv_keyboard_set_textarea(kb, ta);
  lv_obj_scroll_to_view(ta, LV_ANIM_ON);
}

static void on_connect_clicked(lv_event_t* e) {
  const char* ssid = lv_textarea_get_text(ssid_ta);
  const char* pass = lv_textarea_get_text(pass_ta);

  if (strlen(ssid) == 0) {
    lv_label_set_text(status_label, "Please enter SSID");
    return;
  }

  PreferencesManager::instance().saveWifiCredentials(ssid, pass);
  NetworkManager::instance().begin(ssid, pass);

  lv_obj_add_state(connect_btn, LV_STATE_DISABLED);
  lv_label_set_text(status_label, "Connecting...");
  lv_obj_remove_flag(spinner, LV_OBJ_FLAG_HIDDEN);

  connect_start = millis();
  connect_timer = lv_timer_create(
      [](lv_timer_t* t) {
        if (NetworkManager::instance().isWiFiConnected()) {
          lv_timer_del(connect_timer);
          connect_timer = nullptr;
          trigger_gateway_scan();
        } else if (millis() - connect_start > 30000) {
          lv_timer_del(connect_timer);
          connect_timer = nullptr;
          lv_obj_clear_state(connect_btn, LV_STATE_DISABLED);
          lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
          lv_label_set_text(status_label, "Connection failed - try again");
        }
      },
      500, nullptr);
}

void build_wifi_screen(bool fromSettings) {
  // Clean up any connect timer from a previous visit
  if (connect_timer != nullptr) {
    lv_timer_del(connect_timer);
    connect_timer = nullptr;
  }

  // Save old screen for deletion AFTER new screen is loaded
  lv_obj_t* old_screen = lv_screen_active();

  lv_obj_t* scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x1E1E24), LV_PART_MAIN);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  // Back button when called from settings
  if (fromSettings) {
    lv_obj_t* back_btn = lv_button_create(scr);
    lv_obj_set_size(back_btn, 40, 28);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 6, 6);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x2D2D35), LV_PART_MAIN);
    lv_obj_set_style_radius(back_btn, 4, LV_PART_MAIN);
    lv_obj_set_style_border_width(back_btn, 0, LV_PART_MAIN);
    lv_obj_t* back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(back_lbl);
    lv_obj_add_event_cb(
        back_btn,
        [](lv_event_t*) {
          // Clean up connect timer before navigating away
          if (connect_timer != nullptr) {
            lv_timer_del(connect_timer);
            connect_timer = nullptr;
          }
          build_settings_screen();
        },
        LV_EVENT_CLICKED, nullptr);
  }

  // Scrollable content panel
  lv_obj_t* panel = lv_obj_create(scr);
  lv_obj_set_size(panel, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(panel, 0, fromSettings ? 40 : 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_left(panel, 20, LV_PART_MAIN);
  lv_obj_set_style_pad_right(panel, 20, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(panel, 120, LV_PART_MAIN);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_scroll_dir(panel, LV_DIR_VER);
  lv_obj_set_scroll_snap_y(panel, LV_SCROLL_SNAP_NONE);

  // Title
  lv_obj_t* title = lv_label_create(panel);
  lv_label_set_text(title, "Wi-Fi Settings");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_width(title, LV_PCT(100));
  lv_obj_set_style_pad_top(title, 16, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(title, 14, LV_PART_MAIN);
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

  // SSID label
  lv_obj_t* ssid_label = lv_label_create(panel);
  lv_label_set_text(ssid_label, "SSID");
  lv_obj_set_width(ssid_label, LV_PCT(100));
  lv_obj_set_style_text_color(ssid_label, lv_color_hex(0x8A8A8A), LV_PART_MAIN);
  lv_obj_set_style_text_font(ssid_label, &lv_font_montserrat_10, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(ssid_label, 4, LV_PART_MAIN);

  // SSID textarea
  ssid_ta = lv_textarea_create(panel);
  lv_obj_set_width(ssid_ta, LV_PCT(100));
  lv_obj_set_height(ssid_ta, 36);
  lv_textarea_set_placeholder_text(ssid_ta, "Enter Wi-Fi SSID");
  lv_textarea_set_one_line(ssid_ta, true);
  lv_obj_set_style_bg_color(ssid_ta, lv_color_hex(0x2D2D35), LV_PART_MAIN);
  lv_obj_set_style_text_color(ssid_ta, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_add_event_cb(ssid_ta, ta_focus_cb, LV_EVENT_FOCUSED, nullptr);

  // Password label
  lv_obj_t* pass_label = lv_label_create(panel);
  lv_label_set_text(pass_label, "Password");
  lv_obj_set_width(pass_label, LV_PCT(100));
  lv_obj_set_style_text_color(pass_label, lv_color_hex(0x8A8A8A), LV_PART_MAIN);
  lv_obj_set_style_text_font(pass_label, &lv_font_montserrat_10, LV_PART_MAIN);
  lv_obj_set_style_pad_top(pass_label, 12, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(pass_label, 4, LV_PART_MAIN);

  // Password textarea
  pass_ta = lv_textarea_create(panel);
  lv_obj_set_width(pass_ta, LV_PCT(100));
  lv_obj_set_height(pass_ta, 36);
  lv_textarea_set_placeholder_text(pass_ta, "Enter password");
  lv_textarea_set_password_mode(pass_ta, true);
  lv_textarea_set_one_line(pass_ta, true);
  lv_obj_set_style_bg_color(pass_ta, lv_color_hex(0x2D2D35), LV_PART_MAIN);
  lv_obj_set_style_text_color(pass_ta, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_add_event_cb(pass_ta, ta_focus_cb, LV_EVENT_FOCUSED, nullptr);

  // Show password checkbox
  show_pass_cb = lv_checkbox_create(panel);
  lv_checkbox_set_text(show_pass_cb, "Show password");
  lv_obj_set_width(show_pass_cb, LV_PCT(100));
  lv_obj_set_style_text_color(show_pass_cb, lv_color_hex(0x8A8A8A),
                              LV_PART_MAIN);
  lv_obj_set_style_pad_top(show_pass_cb, 8, LV_PART_MAIN);
  lv_obj_add_event_cb(show_pass_cb, on_show_pass_changed,
                      LV_EVENT_VALUE_CHANGED, nullptr);

  // Connect button
  connect_btn = lv_button_create(panel);
  lv_obj_set_width(connect_btn, 180);
  lv_obj_set_height(connect_btn, 42);
  lv_obj_set_style_bg_color(connect_btn, lv_color_hex(0x00A8E8), LV_PART_MAIN);
  lv_obj_set_style_radius(connect_btn, 6, LV_PART_MAIN);
  lv_obj_set_style_pad_top(connect_btn, 20, LV_PART_MAIN);
  lv_obj_add_event_cb(connect_btn, on_connect_clicked, LV_EVENT_CLICKED,
                      nullptr);

  lv_obj_t* btn_label = lv_label_create(connect_btn);
  lv_label_set_text(btn_label, "Connect");
  lv_obj_set_style_text_color(btn_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_center(btn_label);

  // Status label
  status_label = lv_label_create(panel);
  lv_label_set_text(status_label, "");
  lv_obj_set_style_text_color(status_label, lv_color_hex(0x00A8E8),
                              LV_PART_MAIN);
  lv_obj_set_style_text_font(status_label, &lv_font_montserrat_10,
                             LV_PART_MAIN);
  lv_obj_set_width(status_label, LV_PCT(100));
  lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_pad_top(status_label, 10, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(status_label, 20, LV_PART_MAIN);

  // Spinner
  spinner = lv_spinner_create(scr);
  lv_obj_set_size(spinner, 40, 40);
  lv_obj_align(spinner, LV_ALIGN_BOTTOM_MID, 0, -80);
  lv_obj_set_style_arc_color(spinner, lv_color_hex(0x404040), LV_PART_MAIN);
  lv_obj_set_style_arc_width(spinner, 4, LV_PART_MAIN);
  lv_obj_set_style_arc_color(spinner, lv_color_hex(0x00A8E8),
                             LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(spinner, 4, LV_PART_INDICATOR);
  lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);

  // Keyboard (fixed at bottom, outside scrollable panel, hidden initially)
  kb = lv_keyboard_create(scr);
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(kb, kb_ready_cb, LV_EVENT_READY, nullptr);

  lv_screen_load(scr);

  // Delete old screen now that new one is loaded
  if (old_screen != nullptr) {
    lv_obj_delete(old_screen);
  }
}
