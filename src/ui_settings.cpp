#include "ui_settings.hpp"

#include "config.hpp"
#include "preferences_manager.hpp"
#include "ui_gateway_scan.hpp"
#include "ui_managers.hpp"
#include "ui_screens.hpp"
#include "ui_wifi_screen.hpp"

static void settings_item_cb(lv_event_t* e);

enum SettingsAction {
  ACTION_WIFI = 1,
  ACTION_GATEWAY = 2,
  ACTION_ABOUT = 3,
  ACTION_BACK = 4,
};

void build_settings_screen() {
  // Save old screen for deletion AFTER new screen is loaded
  lv_obj_t* old_screen = lv_screen_active();

  lv_obj_t* scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x121214), LV_PART_MAIN);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(scr);
  lv_label_set_text(title, LV_SYMBOL_SETTINGS " Settings");
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

  lv_obj_t* div = lv_obj_create(scr);
  lv_obj_set_size(div, 280, 1);
  lv_obj_align(div, LV_ALIGN_TOP_MID, 0, 38);
  lv_obj_set_style_bg_color(div, lv_color_hex(0x2D2D35), LV_PART_MAIN);
  lv_obj_set_style_border_width(div, 0, LV_PART_MAIN);

  lv_obj_t* panel = lv_obj_create(scr);
  lv_obj_set_size(panel, 280, 170);
  lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 45);
  lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(panel, LV_DIR_VER);
  lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_row(panel, 2, LV_PART_MAIN);

  struct SettingsEntry {
    const char* icon;
    const char* label;
    int action;
  };

  SettingsEntry entries[] = {
      {LV_SYMBOL_WIFI, "Wi-Fi Settings", ACTION_WIFI},
      {LV_SYMBOL_DRIVE, "Change Gateway", ACTION_GATEWAY},
      {LV_SYMBOL_REFRESH, "About", ACTION_ABOUT},
      {LV_SYMBOL_HOME, "Back to Dashboard", ACTION_BACK},
  };

  for (int i = 0; i < 4; i++) {
    lv_obj_t* item = lv_button_create(panel);
    lv_obj_set_width(item, LV_PCT(100));
    lv_obj_set_height(item, 36);
    lv_obj_set_style_bg_color(item, lv_color_hex(0x1E1E24), LV_PART_MAIN);
    lv_obj_set_style_radius(item, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_left(item, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_right(item, 12, LV_PART_MAIN);
    lv_obj_add_event_cb(item, settings_item_cb, LV_EVENT_CLICKED,
                        (void*)(intptr_t)entries[i].action);

    lv_obj_t* icon_lbl = lv_label_create(item);
    lv_label_set_text(icon_lbl, entries[i].icon);
    lv_obj_set_style_text_font(icon_lbl, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(icon_lbl, LV_ALIGN_LEFT_MID, 4, 0);

    lv_obj_t* label = lv_label_create(item);
    lv_label_set_text(label, entries[i].label);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 30, 0);

    lv_obj_t* chevron = lv_label_create(item);
    lv_label_set_text(chevron, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(chevron, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(chevron, lv_color_hex(0x656565), LV_PART_MAIN);
    lv_obj_align(chevron, LV_ALIGN_RIGHT_MID, -8, 0);
  }

  auto& prefs = PreferencesManager::instance();
  String footer =
      "Gateway: " +
      (prefs.hasPairedGateway() ? prefs.getPairedGatewayName() : "None");
  lv_obj_t* footer_lbl = lv_label_create(scr);
  lv_label_set_text(footer_lbl, footer.c_str());
  lv_obj_set_style_text_color(footer_lbl, lv_color_hex(0x656565), LV_PART_MAIN);
  lv_obj_set_style_text_font(footer_lbl, &lv_font_montserrat_8, LV_PART_MAIN);
  lv_obj_align(footer_lbl, LV_ALIGN_BOTTOM_MID, 0, -10);

  lv_screen_load(scr);

  // Delete old screen now that new one is loaded
  if (old_screen != nullptr) {
    lv_obj_delete(old_screen);
  }
}

static void settings_item_cb(lv_event_t* e) {
  int action = (int)(intptr_t)lv_event_get_user_data(e);

  switch (action) {
    case ACTION_WIFI:
      build_wifi_screen(true);
      break;
    case ACTION_GATEWAY:
      build_gateway_scan_screen(true);
      break;
    case ACTION_ABOUT:
      build_about_screen();
      break;
    case ACTION_BACK:
      trigger_main_dashboard_sequence();
      break;
  }
}

void build_about_screen() {
  lv_obj_t* old_screen = lv_screen_active();

  lv_obj_t* scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x121214), LV_PART_MAIN);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(scr);
  lv_label_set_text(title, "About");
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

  lv_obj_t* info = lv_label_create(scr);
  String text = "SmartHome HMI\n\n";
  text += "Hub: " + String(HMI_HUB_HOST) + ":" + String(HMI_HUB_PORT) + "\n\n";
  text += "Firmware: ESP32-S3\nDisplay: 320x240";
  lv_label_set_text(info, text.c_str());
  lv_obj_set_style_text_color(info, lv_color_hex(0x8A8A8A), LV_PART_MAIN);
  lv_obj_set_style_text_font(info, &lv_font_montserrat_10, LV_PART_MAIN);
  lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(info, LV_ALIGN_CENTER, 0, -10);
  lv_obj_set_width(info, 260);

  lv_obj_t* back_btn = lv_button_create(scr);
  lv_obj_set_size(back_btn, 120, 32);
  lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -30);
  lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x2D2D35), LV_PART_MAIN);
  lv_obj_set_style_radius(back_btn, 4, LV_PART_MAIN);

  lv_obj_t* back_lbl = lv_label_create(back_btn);
  lv_label_set_text(back_lbl, LV_SYMBOL_LEFT " Back");
  lv_obj_set_style_text_color(back_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_10, LV_PART_MAIN);
  lv_obj_center(back_lbl);

  lv_obj_add_event_cb(
      back_btn, [](lv_event_t*) { build_settings_screen(); }, LV_EVENT_CLICKED,
      nullptr);

  lv_screen_load(scr);

  if (old_screen != nullptr) {
    lv_obj_delete(old_screen);
  }
}
