#include "ui_gateway_scan.hpp"

#include "gateway_discovery.hpp"
#include "pairing_client.hpp"
#include "ui_managers.hpp"
#include "ui_screens.hpp"
#include "ui_settings.hpp"

static bool g_from_settings = false;

static lv_obj_t* gw_list_panel;
static lv_obj_t* scan_btn;
static lv_obj_t* scan_spinner;
static lv_obj_t* no_gw_label;
static lv_timer_t* scan_timer = nullptr;
static size_t displayed_count = 0;

static void select_gateway_cb(lv_event_t* e) {
  size_t idx = (size_t)lv_event_get_user_data(e);

  auto& disc = GatewayDiscovery::instance();
  const DiscoveredGateway* gw = disc.getGateway(idx);
  if (!gw) return;

  Serial.printf("[UI] Selected gateway: %s (id=%s)\n", gw->name.c_str(),
                gw->id.c_str());

  auto& pc = PairingClient::instance();
  pc.setGatewayId(gw->id);
  pc.setGatewayName(gw->name);

  trigger_pairing_flow();
}

static void add_gateway_card(size_t idx) {
  auto& disc = GatewayDiscovery::instance();
  const DiscoveredGateway* gw = disc.getGateway(idx);
  if (!gw) return;

  lv_obj_t* card = lv_button_create(gw_list_panel);
  lv_obj_set_width(card, LV_PCT(100));
  lv_obj_set_height(card, 44);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x2D2D35), LV_PART_MAIN);
  lv_obj_set_style_radius(card, 6, LV_PART_MAIN);
  lv_obj_add_event_cb(card, select_gateway_cb, LV_EVENT_CLICKED, (void*)idx);

  lv_obj_t* name_lbl = lv_label_create(card);
  lv_label_set_text(name_lbl, gw->name.c_str());
  lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_set_style_text_color(name_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_align(name_lbl, LV_ALIGN_LEFT_MID, 10, 0);

  char id_buf[32];
  snprintf(id_buf, sizeof(id_buf), "ID: %s", gw->id.c_str());
  lv_obj_t* id_lbl = lv_label_create(card);
  lv_label_set_text(id_lbl, id_buf);
  lv_obj_set_style_text_font(id_lbl, &lv_font_montserrat_8, LV_PART_MAIN);
  lv_obj_set_style_text_color(id_lbl, lv_color_hex(0x8A8A8A), LV_PART_MAIN);
  lv_obj_align(id_lbl, LV_ALIGN_RIGHT_MID, -10, 0);

  displayed_count++;
  lv_obj_add_flag(no_gw_label, LV_OBJ_FLAG_HIDDEN);
}

static void sync_gateway_list() {
  if (gw_list_panel == nullptr) return;

  auto& disc = GatewayDiscovery::instance();
  size_t actual = disc.gatewayCount();

  if (actual == displayed_count) return;

  if (actual > displayed_count) {
    // New gateways discovered — add cards for new ones
    for (size_t i = displayed_count; i < actual; i++) {
      add_gateway_card(i);
    }
  } else {
    // Gateways removed (scan restart) — rebuild from scratch
    lv_obj_clean(gw_list_panel);
    no_gw_label = lv_label_create(gw_list_panel);
    lv_label_set_text(no_gw_label, "No gateways found.\nTap Scan to discover.");
    lv_obj_set_style_text_color(no_gw_label, lv_color_hex(0x8A8A8A),
                                LV_PART_MAIN);
    lv_obj_set_style_text_font(no_gw_label, &lv_font_montserrat_10,
                               LV_PART_MAIN);
    lv_obj_center(no_gw_label);
    displayed_count = 0;
    for (size_t i = 0; i < actual; i++) {
      add_gateway_card(i);
    }
  }

  if (actual == 0) {
    lv_obj_clear_flag(no_gw_label, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(no_gw_label, LV_OBJ_FLAG_HIDDEN);
  }
}

static void scan_timer_cb(lv_timer_t* t) {
  // Guard against stale timer after screen transition
  if (t != scan_timer || gw_list_panel == nullptr) return;

  auto& disc = GatewayDiscovery::instance();
  disc.update();
  sync_gateway_list();

  if (!disc.isScanning()) {
    if (scan_btn != nullptr) lv_obj_clear_state(scan_btn, LV_STATE_DISABLED);
    if (scan_spinner != nullptr)
      lv_obj_add_flag(scan_spinner, LV_OBJ_FLAG_HIDDEN);
  }
}

static void on_scan_clicked(lv_event_t* e) {
  auto& disc = GatewayDiscovery::instance();
  disc.startScan();

  // Reset UI: clear panel, re-add placeholder
  if (gw_list_panel == nullptr) return;
  lv_obj_clean(gw_list_panel);
  no_gw_label = lv_label_create(gw_list_panel);
  lv_label_set_text(no_gw_label, "No gateways found.\nTap Scan to discover.");
  lv_obj_set_style_text_color(no_gw_label, lv_color_hex(0x8A8A8A),
                              LV_PART_MAIN);
  lv_obj_set_style_text_font(no_gw_label, &lv_font_montserrat_10, LV_PART_MAIN);
  lv_obj_center(no_gw_label);
  displayed_count = 0;

  lv_obj_add_state(scan_btn, LV_STATE_DISABLED);
  lv_obj_clear_flag(scan_spinner, LV_OBJ_FLAG_HIDDEN);
  Serial.println("[UI] Scan button clicked, restarting scan");
}

void cleanup_gateway_scan_screen() {
  if (scan_timer != nullptr) {
    lv_timer_del(scan_timer);
    scan_timer = nullptr;
  }
  gw_list_panel = nullptr;
  no_gw_label = nullptr;
  scan_btn = nullptr;
  scan_spinner = nullptr;
  displayed_count = 0;
}

void build_gateway_scan_screen(bool fromSettings) {
  g_from_settings = fromSettings;

  // Save old screen for deletion AFTER new screen is loaded
  lv_obj_t* old_screen = nullptr;
  if (fromSettings) {
    old_screen = lv_screen_active();
  }

  lv_obj_t* scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x1E1E24), LV_PART_MAIN);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  // Back button at top when opened from settings
  if (g_from_settings) {
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
        back_btn,
        [](lv_event_t*) {
          // Clean up scan timer before navigating away
          if (scan_timer != nullptr) {
            lv_timer_del(scan_timer);
            scan_timer = nullptr;
          }
          gw_list_panel = nullptr;
          no_gw_label = nullptr;
          build_settings_screen();
        },
        LV_EVENT_CLICKED, nullptr);
  }

  lv_obj_t* title = lv_label_create(scr);
  lv_label_set_text(title, "Select Gateway");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

  lv_obj_t* instr = lv_label_create(scr);
  lv_label_set_text(instr, "Discovered gateways:");
  lv_obj_set_style_text_color(instr, lv_color_hex(0x8A8A8A), LV_PART_MAIN);
  lv_obj_set_style_text_font(instr, &lv_font_montserrat_10, LV_PART_MAIN);
  lv_obj_align(instr, LV_ALIGN_TOP_LEFT, 15, 45);

  scan_btn = lv_button_create(scr);
  lv_obj_set_size(scan_btn, 80, 26);
  lv_obj_align(scan_btn, LV_ALIGN_TOP_RIGHT, -15, 42);
  lv_obj_set_style_bg_color(scan_btn, lv_color_hex(0x00A8E8), LV_PART_MAIN);
  lv_obj_set_style_radius(scan_btn, 4, LV_PART_MAIN);
  lv_obj_add_event_cb(scan_btn, on_scan_clicked, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* scan_lbl = lv_label_create(scan_btn);
  lv_label_set_text(scan_lbl, "Scan");
  lv_obj_set_style_text_font(scan_lbl, &lv_font_montserrat_10, LV_PART_MAIN);
  lv_obj_set_style_text_color(scan_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_center(scan_lbl);

  scan_spinner = lv_spinner_create(scr);
  lv_obj_set_size(scan_spinner, 40, 40);
  lv_obj_align(scan_spinner, LV_ALIGN_CENTER, 0, -20);
  lv_obj_set_style_arc_color(scan_spinner, lv_color_hex(0x404040),
                             LV_PART_MAIN);
  lv_obj_set_style_arc_width(scan_spinner, 4, LV_PART_MAIN);
  lv_obj_set_style_arc_color(scan_spinner, lv_color_hex(0x00A8E8),
                             LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(scan_spinner, 4, LV_PART_INDICATOR);
  lv_obj_add_flag(scan_spinner, LV_OBJ_FLAG_HIDDEN);

  gw_list_panel = lv_obj_create(scr);
  lv_obj_set_size(gw_list_panel, 290, 140);
  lv_obj_align(gw_list_panel, LV_ALIGN_TOP_LEFT, 15, 75);
  lv_obj_set_flex_flow(gw_list_panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(gw_list_panel, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(gw_list_panel, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_bg_color(gw_list_panel, lv_color_hex(0x121214),
                            LV_PART_MAIN);
  lv_obj_set_style_border_width(gw_list_panel, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(gw_list_panel, lv_color_hex(0x2D2D35),
                                LV_PART_MAIN);
  lv_obj_set_style_pad_all(gw_list_panel, 6, LV_PART_MAIN);
  lv_obj_set_style_pad_row(gw_list_panel, 6, LV_PART_MAIN);

  no_gw_label = lv_label_create(gw_list_panel);
  lv_label_set_text(no_gw_label, "No gateways found.\nTap Scan to discover.");
  lv_obj_set_style_text_color(no_gw_label, lv_color_hex(0x8A8A8A),
                              LV_PART_MAIN);
  lv_obj_set_style_text_font(no_gw_label, &lv_font_montserrat_10, LV_PART_MAIN);
  lv_obj_center(no_gw_label);

  displayed_count = 0;

  // Load new screen BEFORE deleting old one (LVGL needs a valid active screen)
  lv_screen_load(scr);

  // Now safe to delete old screen
  if (old_screen != nullptr) {
    lv_obj_delete(old_screen);
  }

  // Kill any previous scan timer before creating a new one
  if (scan_timer != nullptr) {
    lv_timer_del(scan_timer);
    scan_timer = nullptr;
  }
  scan_timer = lv_timer_create(scan_timer_cb, 500, nullptr);

  // Start auto-scan
  auto& disc = GatewayDiscovery::instance();
  disc.startScan();
  lv_obj_add_state(scan_btn, LV_STATE_DISABLED);
  lv_obj_clear_flag(scan_spinner, LV_OBJ_FLAG_HIDDEN);
  Serial.println("[UI] Gateway scan screen shown, auto-scan started");
}
