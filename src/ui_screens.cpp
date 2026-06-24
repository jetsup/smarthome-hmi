#include "ui_screens.hpp"

lv_obj_t* logo_screen = nullptr;
lv_obj_t* splash_screen = nullptr;
lv_obj_t* main_screen = nullptr;
lv_obj_t* logo_progress_bar = nullptr;

static lv_obj_t* splash_status_label = nullptr;

extern const lv_image_dsc_t logo;

void build_logo_layout() {
    logo_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(logo_screen, lv_color_hex(0x000000), LV_PART_MAIN);

    lv_obj_t* ui_logo = lv_image_create(logo_screen);
    lv_image_set_src(ui_logo, &logo);
    lv_obj_center(ui_logo);

    logo_progress_bar = lv_bar_create(logo_screen);
    lv_obj_set_size(logo_progress_bar, 240, 6);
    lv_obj_align(logo_progress_bar, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_bar_set_range(logo_progress_bar, 0, 100);
    lv_bar_set_value(logo_progress_bar, 0, LV_ANIM_OFF);

    lv_obj_set_style_bg_color(logo_progress_bar, lv_color_hex(0x333333),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_color(logo_progress_bar, lv_color_hex(0x00A8E8),
                              LV_PART_INDICATOR);
}

void build_splash_layout() {
    splash_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(splash_screen, lv_color_hex(0x1E1E24),
                              LV_PART_MAIN);

    lv_obj_t* title = lv_label_create(splash_screen);
    lv_label_set_text(title, "ESP-NOW GATEWAY");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -40);

    lv_obj_t* subtitle = lv_label_create(splash_screen);
    lv_label_set_text(subtitle, "HMI Controller");
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0x00A8E8), LV_PART_MAIN);
    lv_obj_align_to(subtitle, title, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

    lv_obj_t* spinner = lv_spinner_create(splash_screen);
    lv_obj_set_size(spinner, 40, 40);
    lv_obj_align(spinner, LV_ALIGN_CENTER, 0, 40);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(0x00A8E8),
                               LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(0x404040), LV_PART_MAIN);
    lv_obj_set_style_arc_width(spinner, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_width(spinner, 4, LV_PART_INDICATOR);

    splash_status_label = lv_label_create(splash_screen);
    lv_label_set_text(splash_status_label, "Connecting to Wi-Fi...");
    lv_obj_set_style_text_font(splash_status_label, &lv_font_montserrat_10,
                               LV_PART_MAIN);
    lv_obj_set_style_text_color(splash_status_label, lv_color_hex(0x8A8A8A),
                                LV_PART_MAIN);
    lv_obj_align(splash_status_label, LV_ALIGN_BOTTOM_MID, 0, -20);
}

void update_splash_status(const char* text) {
    if (splash_status_label != nullptr) {
        lv_label_set_text(splash_status_label, text);
    }
}
