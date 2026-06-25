#include "ui_system_screen.hpp"

#include "preferences_manager.hpp"
#include "ui_managers.hpp"
#include "ui_screens.hpp"
#include "ui_settings.hpp"
#include "ui_wifi_screen.hpp"

void build_system_screen() {
  lv_obj_t* old_screen = lv_screen_active();

  lv_obj_t* scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x121214), LV_PART_MAIN);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* back_btn = lv_button_create(scr);
  lv_obj_set_size(back_btn, 36, 28);
  lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 6, 6);
  lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x2D2D35), LV_PART_MAIN);
  lv_obj_set_style_radius(back_btn, 4, LV_PART_MAIN);
  lv_obj_set_style_border_width(back_btn, 0, LV_PART_MAIN);
  lv_obj_t* back_lbl = lv_label_create(back_btn);
  lv_label_set_text(back_lbl, LV_SYMBOL_LEFT);
  lv_obj_set_style_text_color(back_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_center(back_lbl);
  lv_obj_add_event_cb(
      back_btn, [](lv_event_t*) { build_settings_screen(); }, LV_EVENT_CLICKED,
      nullptr);

  lv_obj_t* title = lv_label_create(scr);
  lv_label_set_text(title, LV_SYMBOL_SETTINGS " System");
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

  lv_obj_t* reset_btn = lv_button_create(panel);
  lv_obj_set_width(reset_btn, LV_PCT(100));
  lv_obj_set_height(reset_btn, 36);
  lv_obj_set_style_bg_color(reset_btn, lv_color_hex(0x4A1515), LV_PART_MAIN);
  lv_obj_set_style_radius(reset_btn, 4, LV_PART_MAIN);
  lv_obj_set_style_pad_left(reset_btn, 12, LV_PART_MAIN);
  lv_obj_set_style_pad_right(reset_btn, 12, LV_PART_MAIN);

  lv_obj_t* reset_icon = lv_label_create(reset_btn);
  lv_label_set_text(reset_icon, LV_SYMBOL_CLOSE);
  lv_obj_set_style_text_font(reset_icon, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(reset_icon, LV_ALIGN_LEFT_MID, 4, 0);

  lv_obj_t* reset_label = lv_label_create(reset_btn);
  lv_label_set_text(reset_label, "Reset System");
  lv_obj_set_style_text_font(reset_label, &lv_font_montserrat_10, LV_PART_MAIN);
  lv_obj_set_style_text_color(reset_label, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN);
  lv_obj_align(reset_label, LV_ALIGN_LEFT_MID, 30, 0);

  lv_obj_t* desc = lv_label_create(scr);
  lv_label_set_text(desc,
                    "Clears all saved data including Wi-Fi\ncredentials, "
                    "gateway pairing, and cache");
  lv_obj_set_style_text_color(desc, lv_color_hex(0x656565), LV_PART_MAIN);
  lv_obj_set_style_text_font(desc, &lv_font_montserrat_8, LV_PART_MAIN);
  lv_obj_set_style_text_align(desc, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
  lv_obj_align(desc, LV_ALIGN_BOTTOM_MID, 20, -8);

  lv_obj_add_event_cb(
      reset_btn,
      [](lv_event_t* e) {
        lv_obj_t* dialog = lv_obj_create(lv_scr_act());
        lv_obj_set_size(dialog, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(dialog, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dialog, 160, LV_PART_MAIN);
        lv_obj_center(dialog);

        lv_obj_t* container = lv_obj_create(dialog);
        lv_obj_set_size(container, 290, 170);
        lv_obj_remove_flag(container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(container, lv_color_hex(0x1E1E24),
                                  LV_PART_MAIN);
        lv_obj_set_style_border_width(container, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(container, lv_color_hex(0x2D2D35),
                                      LV_PART_MAIN);
        lv_obj_set_style_radius(container, 8, LV_PART_MAIN);
        lv_obj_center(container);

        lv_obj_t* icon = lv_label_create(container);
        lv_label_set_text(icon, LV_SYMBOL_WARNING);
        lv_obj_set_style_text_color(icon, lv_color_hex(0xF0A030), LV_PART_MAIN);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 10);

        lv_obj_t* lbl = lv_label_create(container);
        lv_label_set_text(lbl, "Reset all data?\nThis cannot be undone.");
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, -10);
        lv_obj_set_width(lbl, 270);

        lv_obj_t* btn_row = lv_obj_create(container);
        lv_obj_set_size(btn_row, 270, 36);
        lv_obj_align(btn_row, LV_ALIGN_BOTTOM_MID, 0, -12);
        lv_obj_remove_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn_row, 0, LV_PART_MAIN);
        lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* cancel_btn = lv_button_create(btn_row);
        lv_obj_set_size(cancel_btn, 90, 32);
        lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0x2D2D35),
                                  LV_PART_MAIN);
        lv_obj_set_style_radius(cancel_btn, 4, LV_PART_MAIN);
        lv_obj_t* cancel_lbl = lv_label_create(cancel_btn);
        lv_label_set_text(cancel_lbl, "Cancel");
        lv_obj_set_style_text_color(cancel_lbl, lv_color_hex(0xFFFFFF),
                                    LV_PART_MAIN);
        lv_obj_set_style_text_font(cancel_lbl, &lv_font_montserrat_10,
                                   LV_PART_MAIN);
        lv_obj_center(cancel_lbl);
        lv_obj_add_event_cb(
            cancel_btn,
            [](lv_event_t* e) {
              lv_obj_t* d = (lv_obj_t*)lv_event_get_user_data(e);
              lv_obj_delete(d);
            },
            LV_EVENT_CLICKED, dialog);

        lv_obj_t* ok_btn = lv_button_create(btn_row);
        lv_obj_set_size(ok_btn, 90, 32);
        lv_obj_set_style_bg_color(ok_btn, lv_color_hex(0x8B2020), LV_PART_MAIN);
        lv_obj_set_style_radius(ok_btn, 4, LV_PART_MAIN);
        lv_obj_t* ok_lbl = lv_label_create(ok_btn);
        lv_label_set_text(ok_lbl, "OK");
        lv_obj_set_style_text_color(ok_lbl, lv_color_hex(0xFFFFFF),
                                    LV_PART_MAIN);
        lv_obj_set_style_text_font(ok_lbl, &lv_font_montserrat_10,
                                   LV_PART_MAIN);
        lv_obj_center(ok_lbl);
        lv_obj_add_event_cb(
            ok_btn,
            [](lv_event_t* e) {
              lv_obj_t* d = (lv_obj_t*)lv_event_get_user_data(e);
              lv_obj_delete(d);

              auto& prefs = PreferencesManager::instance();
              prefs.clearSystem();

              build_wifi_screen(false);
            },
            LV_EVENT_CLICKED, dialog);
      },
      LV_EVENT_CLICKED, nullptr);

  lv_screen_load(scr);

  if (old_screen != nullptr) {
    lv_obj_delete(old_screen);
  }
}
