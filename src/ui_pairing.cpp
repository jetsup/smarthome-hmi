#include "ui_pairing.hpp"
#include "data_manager.hpp"
#include "network_manager.hpp"
#include "pairing_client.hpp"
#include "preferences_manager.hpp"
#include "ui_dashboard.hpp"
#include "ui_managers.hpp"
#include "ui_screens.hpp"

static lv_obj_t* code_ta;
static lv_obj_t* code_kb;
static lv_obj_t* code_container;
static lv_obj_t* code_dialog;
static lv_obj_t* loading_dialog = nullptr;

void show_loading_dialog(const char* message) {
    if (loading_dialog) {
        lv_obj_delete(loading_dialog);
        loading_dialog = nullptr;
    }

    loading_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(loading_dialog, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(loading_dialog, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(loading_dialog, 160, LV_PART_MAIN);
    lv_obj_center(loading_dialog);

    lv_obj_t* container = lv_obj_create(loading_dialog);
    lv_obj_set_size(container, 160, 100);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1E1E24), LV_PART_MAIN);
    lv_obj_set_style_border_width(container, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(container, lv_color_hex(0x2D2D35), LV_PART_MAIN);
    lv_obj_set_style_radius(container, 8, LV_PART_MAIN);
    lv_obj_center(container);

    lv_obj_t* spinner = lv_spinner_create(container);
    lv_obj_set_size(spinner, 30, 30);
    lv_obj_align(spinner, LV_ALIGN_TOP_MID, 0, 15);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(0x00A8E8), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(0x404040), LV_PART_MAIN);
    lv_obj_set_style_arc_width(spinner, 3, LV_PART_MAIN);
    lv_obj_set_style_arc_width(spinner, 3, LV_PART_INDICATOR);

    lv_obj_t* lbl = lv_label_create(container);
    lv_label_set_text(lbl, message);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -12);
}

void hide_loading_dialog() {
    if (loading_dialog) {
        lv_obj_delete(loading_dialog);
        loading_dialog = nullptr;
    }
}

void show_error_dialog(const char* message) {
    lv_obj_t* dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(dialog, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(dialog, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dialog, 160, LV_PART_MAIN);
    lv_obj_center(dialog);

    lv_obj_t* container = lv_obj_create(dialog);
    lv_obj_set_size(container, 240, 150);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1E1E24), LV_PART_MAIN);
    lv_obj_set_style_border_width(container, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(container, lv_color_hex(0x2D2D35), LV_PART_MAIN);
    lv_obj_set_style_radius(container, 8, LV_PART_MAIN);
    lv_obj_center(container);

    lv_obj_t* icon = lv_label_create(container);
    lv_label_set_text(icon, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(icon, lv_color_hex(0xF85149), LV_PART_MAIN);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t* lbl = lv_label_create(container);
    lv_label_set_text(lbl, message);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_width(lbl, 220);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_t* ok_btn = lv_button_create(container);
    lv_obj_set_size(ok_btn, 80, 28);
    lv_obj_align(ok_btn, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_set_style_bg_color(ok_btn, lv_color_hex(0x2D2D35), LV_PART_MAIN);
    lv_obj_set_style_radius(ok_btn, 4, LV_PART_MAIN);

    lv_obj_t* ok_lbl = lv_label_create(ok_btn);
    lv_label_set_text(ok_lbl, "OK");
    lv_obj_set_style_text_color(ok_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(ok_lbl);

    lv_obj_add_event_cb(ok_btn, [](lv_event_t* e) {
        lv_obj_t* dlg = (lv_obj_t*)lv_event_get_user_data(e);
        lv_obj_delete(dlg);
    }, LV_EVENT_CLICKED, dialog);
}

void show_pairing_result_dialog(bool success, const char* message) {
    lv_obj_t* dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(dialog, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(dialog, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dialog, 160, LV_PART_MAIN);
    lv_obj_center(dialog);

    lv_obj_t* container = lv_obj_create(dialog);
    lv_obj_set_size(container, 260, 140);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x1E1E24), LV_PART_MAIN);
    lv_obj_set_style_border_width(container, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(container, success ? lv_color_hex(0x238636) : lv_color_hex(0xF85149), LV_PART_MAIN);
    lv_obj_set_style_radius(container, 8, LV_PART_MAIN);
    lv_obj_center(container);

    lv_obj_t* icon = lv_label_create(container);
    lv_label_set_text(icon, success ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(icon, success ? lv_color_hex(0x3FB950) : lv_color_hex(0xF85149), LV_PART_MAIN);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t* lbl = lv_label_create(container);
    lv_label_set_text(lbl, message);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_width(lbl, 240);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_t* ok_btn = lv_button_create(container);
    lv_obj_set_size(ok_btn, 80, 28);
    lv_obj_align(ok_btn, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(ok_btn, success ? lv_color_hex(0x238636) : lv_color_hex(0x2D2D35), LV_PART_MAIN);
    lv_obj_set_style_radius(ok_btn, 4, LV_PART_MAIN);

    lv_obj_t* ok_lbl = lv_label_create(ok_btn);
    lv_label_set_text(ok_lbl, "OK");
    lv_obj_set_style_text_color(ok_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(ok_lbl);

    lv_obj_add_event_cb(ok_btn, [](lv_event_t* e) {
        lv_obj_t* dlg = (lv_obj_t*)lv_event_get_user_data(e);
        lv_obj_delete(dlg);
        // Fetch nodes before showing dashboard
        auto& dm = DataManager::instance();
        auto& prefs = PreferencesManager::instance();
        dm.setGatewayId(prefs.getPairedGatewayId());
        dm.fetchNodes();
        trigger_main_dashboard_sequence();
    }, LV_EVENT_CLICKED, dialog);
}

static void on_pairing_code_submit(lv_event_t* e) {
    const char* code = lv_textarea_get_text(code_ta);
    if (strlen(code) != 6) return;

    hide_loading_dialog();
    show_loading_dialog("Verifying code...");

    lv_timer_t* verify_timer = lv_timer_create([](lv_timer_t* t) {
        auto& pc = PairingClient::instance();
        auto result = pc.verifyCode(lv_textarea_get_text(code_ta));

        hide_loading_dialog();

        if (result.success) {
            PreferencesManager::instance().setPairedGateway(
                pc.pairedGatewayId(), pc.pairedGatewayName());
            show_pairing_result_dialog(true, "Paired successfully!");
        } else {
            show_error_dialog(result.error.c_str());
        }

        lv_timer_del(t);
    }, 100, nullptr);
}

static void code_kb_ready_cb(lv_event_t* e) {
    if (!code_kb) return;
    lv_obj_t* ta = lv_keyboard_get_textarea(code_kb);
    if (ta) lv_obj_clear_state(ta, LV_STATE_FOCUSED);
    lv_obj_add_flag(code_kb, LV_OBJ_FLAG_HIDDEN);
    // Move container back to center when keyboard hides
    if (code_container) lv_obj_center(code_container);
}

static void code_ta_defocus_cb(lv_event_t* e) {
    // If keyboard is hidden and textarea loses focus, restore center
    if (code_kb && lv_obj_has_flag(code_kb, LV_OBJ_FLAG_HIDDEN)) {
        if (code_container) lv_obj_center(code_container);
    }
}

static void code_ta_focus_cb(lv_event_t* e) {
    if (!code_kb) return;
    lv_obj_t* ta = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_clear_flag(code_kb, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(code_kb, ta);
    // Move container up so it's visible above the keyboard
    if (code_container) lv_obj_align(code_container, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_scroll_to_view(ta, LV_ANIM_ON);
}

void show_pairing_code_dialog() {
    code_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(code_dialog, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(code_dialog, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(code_dialog, 160, LV_PART_MAIN);
    lv_obj_center(code_dialog);
    lv_obj_remove_flag(code_dialog, LV_OBJ_FLAG_SCROLLABLE);

    code_container = lv_obj_create(code_dialog);
    lv_obj_set_size(code_container, 240, 180);
    lv_obj_set_style_bg_color(code_container, lv_color_hex(0x1E1E24), LV_PART_MAIN);
    lv_obj_set_style_border_width(code_container, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(code_container, lv_color_hex(0x2D2D35), LV_PART_MAIN);
    lv_obj_set_style_radius(code_container, 8, LV_PART_MAIN);
    lv_obj_center(code_container);
    lv_obj_remove_flag(code_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* instr = lv_label_create(code_container);
    lv_label_set_text(instr, "Enter 6-digit pairing code\nfrom Gateway screen:");
    lv_obj_set_style_text_color(instr, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(instr, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_align(instr, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(instr, LV_ALIGN_TOP_MID, 0, 15);
    lv_obj_set_width(instr, 220);

    code_ta = lv_textarea_create(code_container);
    lv_obj_set_size(code_ta, 180, 36);
    lv_obj_align(code_ta, LV_ALIGN_TOP_MID, 0, 62);
    lv_textarea_set_max_length(code_ta, 6);
    lv_textarea_set_placeholder_text(code_ta, "000000");
    lv_obj_set_style_bg_color(code_ta, lv_color_hex(0x2D2D35), LV_PART_MAIN);
    lv_obj_set_style_text_color(code_ta, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_align(code_ta, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_textarea_set_one_line(code_ta, true);
    lv_obj_add_event_cb(code_ta, code_ta_focus_cb, LV_EVENT_FOCUSED, nullptr);
    lv_obj_add_event_cb(code_ta, code_ta_defocus_cb, LV_EVENT_DEFOCUSED, nullptr);

    lv_obj_t* submit_btn = lv_button_create(code_container);
    lv_obj_set_size(submit_btn, 100, 30);
    lv_obj_align(submit_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(submit_btn, lv_color_hex(0x00A8E8), LV_PART_MAIN);
    lv_obj_set_style_radius(submit_btn, 4, LV_PART_MAIN);
    lv_obj_add_event_cb(submit_btn, on_pairing_code_submit, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* submit_lbl = lv_label_create(submit_btn);
    lv_label_set_text(submit_lbl, "Confirm");
    lv_obj_set_style_text_color(submit_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(submit_lbl);

    // Keyboard (fixed at bottom, hidden initially)
    code_kb = lv_keyboard_create(lv_scr_act());
    lv_obj_align(code_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(code_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(code_kb, code_kb_ready_cb, LV_EVENT_READY, nullptr);
}
