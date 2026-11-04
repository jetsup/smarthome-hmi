#include "theme.hpp"
#include "ui_wifi_setup.hpp"

#include "display.hpp"
#include "network_manager.hpp"
#include "ui_managers.hpp"
#include "ui_settings.hpp"
#include "ui_wifi_screen.hpp"
#include "wifi_provisioner.hpp"

static bool g_from_settings = false;
static lv_timer_t* g_timer = nullptr;
static lv_obj_t* g_status = nullptr;

#define C_BG th_panel()
#define C_CARD th_surface()
#define C_ACCENT th_accent()
#define C_TEXT th_text()
#define C_MUTED th_muted()
#define C_ONACC 0xFFFFFFu
static const uint32_t C_OK = 0x3FB950, C_BAD = 0xF85149;

void cleanup_wifi_setup_screen() {
  if (g_timer) {
    lv_timer_delete(g_timer);
    g_timer = nullptr;
  }
  g_status = nullptr;
}

// Load `scr` and drop the screen it replaces.
// Replace the active screen. Only the old timer is stopped: g_status already
// points at the NEW screen's label and must not be cleared.
static void swap_to(lv_obj_t* scr) {
  if (g_timer) {
    lv_timer_delete(g_timer);
    g_timer = nullptr;
  }
  lv_obj_t* old = lv_screen_active();
  lv_screen_load(scr);
  if (old && old != scr) lv_obj_delete(old);
}

static lv_obj_t* new_screen() {
  lv_obj_t* scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), LV_PART_MAIN);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  return scr;
}

static lv_obj_t* label(lv_obj_t* parent, const char* text, uint32_t color,
                       const lv_font_t* font) {
  lv_obj_t* l = lv_label_create(parent);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_color(l, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
  return l;
}

static lv_obj_t* back_button(lv_obj_t* scr, lv_event_cb_t cb) {
  lv_obj_t* b = lv_button_create(scr);
  lv_obj_set_size(b, 40, 30);
  lv_obj_align(b, LV_ALIGN_TOP_LEFT, 6, 6);
  lv_obj_set_style_bg_color(b, lv_color_hex(C_CARD), LV_PART_MAIN);
  lv_obj_set_style_radius(b, 4, LV_PART_MAIN);
  lv_obj_set_style_border_width(b, 0, LV_PART_MAIN);
  lv_obj_t* l = label(b, LV_SYMBOL_LEFT, C_TEXT, &lv_font_montserrat_14);
  lv_obj_center(l);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
  return b;
}

static lv_obj_t* make_qr(lv_obj_t* scr, const String& data, int size) {
  lv_obj_t* qr = lv_qrcode_create(scr);
  lv_qrcode_set_size(qr, size);
  lv_qrcode_set_dark_color(qr, lv_color_black());
  lv_qrcode_set_light_color(qr, lv_color_white());
  lv_qrcode_update(qr, data.c_str(), data.length());
  // White quiet zone so phone cameras can lock on.
  lv_obj_set_style_border_color(qr, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_border_width(qr, 6, LV_PART_MAIN);
  return qr;
}

// ── Shared status polling for the portal / app screens ───────────────────────
static void status_tick(lv_timer_t*) {
  auto& p = WifiProvisioner::instance();
  if (p.takeConnected()) {
    trigger_after_wifi_connected();
    return;
  }
  if (!g_status) return;
  char buf[96];
  switch (p.state()) {
    case WifiProvisioner::State::Idle:
      lv_label_set_text(g_status, "Waiting for your phone...");
      lv_obj_set_style_text_color(g_status, lv_color_hex(C_MUTED), LV_PART_MAIN);
      break;
    case WifiProvisioner::State::Connecting:
      snprintf(buf, sizeof(buf), "Testing \"%s\"...", p.lastSsid().c_str());
      lv_label_set_text(g_status, buf);
      lv_obj_set_style_text_color(g_status, lv_color_hex(C_ACCENT), LV_PART_MAIN);
      break;
    case WifiProvisioner::State::Connected:
      lv_label_set_text(g_status, LV_SYMBOL_OK " Connected!");
      lv_obj_set_style_text_color(g_status, lv_color_hex(C_OK), LV_PART_MAIN);
      break;
    case WifiProvisioner::State::Failed:
      snprintf(buf, sizeof(buf), "Failed: %s\nTry again on your phone.",
               p.lastError().c_str());
      lv_label_set_text(g_status, buf);
      lv_obj_set_style_text_color(g_status, lv_color_hex(C_BAD), LV_PART_MAIN);
      break;
  }
}

static void to_chooser(lv_event_t*) {
  WifiProvisioner::instance().stop();
  build_wifi_setup_screen(g_from_settings);
}

// ── Method 1: hotspot + captive portal ───────────────────────────────────────
static void build_portal_screen() {
  auto& p = WifiProvisioner::instance();
  p.startPortal();

  lv_obj_t* scr = new_screen();
  back_button(scr, to_chooser);
  label(scr, "Set up with browser", C_TEXT, &lv_font_montserrat_14);
  lv_obj_align(lv_obj_get_child(scr, -1), LV_ALIGN_TOP_LEFT, 56, 12);

  make_qr(scr, p.portalWifiQr(), 140);
  lv_obj_align(lv_obj_get_child(scr, -1), LV_ALIGN_RIGHT_MID, -10, 10);

  lv_obj_t* steps = label(
      scr,
      "1. Scan the code with your\n   phone camera to join the\n   HMI hotspot, or join:",
      C_MUTED, &lv_font_montserrat_10);
  lv_obj_align(steps, LV_ALIGN_TOP_LEFT, 12, 50);

  char buf[96];
  snprintf(buf, sizeof(buf), "%s\nPassword: %s", p.portalSsid().c_str(),
           p.portalPass().c_str());
  lv_obj_t* cred = label(scr, buf, C_TEXT, &lv_font_montserrat_10);
  lv_obj_align(cred, LV_ALIGN_TOP_LEFT, 12, 98);

  lv_obj_t* step2 = label(
      scr, "2. A page opens: pick your\n   Wi-Fi and enter its password.",
      C_MUTED, &lv_font_montserrat_10);
  lv_obj_align(step2, LV_ALIGN_TOP_LEFT, 12, 130);

  g_status = label(scr, "Waiting for your phone...", C_MUTED, &lv_font_montserrat_10);
  lv_obj_set_width(g_status, 160);
  lv_obj_align(g_status, LV_ALIGN_BOTTOM_LEFT, 12, -14);

  swap_to(scr);
  g_timer = lv_timer_create(status_tick, 300, nullptr);
}

// ── Method 2: QR for the SmartHome app (Bluetooth LE) ────────────────────────
static void build_app_screen() {
  auto& p = WifiProvisioner::instance();
  p.startBle();

  lv_obj_t* scr = new_screen();
  back_button(scr, to_chooser);
  label(scr, "Set up with the app", C_TEXT, &lv_font_montserrat_14);
  lv_obj_align(lv_obj_get_child(scr, -1), LV_ALIGN_TOP_LEFT, 56, 12);

  make_qr(scr, p.bleQr(), 140);
  lv_obj_align(lv_obj_get_child(scr, -1), LV_ALIGN_RIGHT_MID, -10, 10);

  lv_obj_t* steps = label(
      scr,
      "1. Open the SmartHome app\n2. Tap \"Set up HMI Wi-Fi\"\n3. Scan this code\n4. Pick your Wi-Fi and\n   enter its password",
      C_MUTED, &lv_font_montserrat_10);
  lv_obj_align(steps, LV_ALIGN_TOP_LEFT, 12, 50);

  g_status = label(scr, "Waiting for your phone...", C_MUTED, &lv_font_montserrat_10);
  lv_obj_set_width(g_status, 160);
  lv_obj_align(g_status, LV_ALIGN_BOTTOM_LEFT, 12, -14);

  swap_to(scr);
  g_timer = lv_timer_create(status_tick, 300, nullptr);
}

// ── Chooser ──────────────────────────────────────────────────────────────────
static lv_obj_t* method_button(lv_obj_t* scr, const char* title, const char* sub,
                               int y, lv_event_cb_t cb) {
  lv_obj_t* b = lv_button_create(scr);
  lv_obj_set_size(b, 280, 46);
  lv_obj_align(b, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_set_style_bg_color(b, lv_color_hex(C_CARD), LV_PART_MAIN);
  lv_obj_set_style_radius(b, 8, LV_PART_MAIN);
  lv_obj_set_style_border_width(b, 0, LV_PART_MAIN);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* t = label(b, title, C_TEXT, &lv_font_montserrat_14);
  lv_obj_align(t, LV_ALIGN_TOP_LEFT, 4, 2);
  lv_obj_t* s = label(b, sub, C_MUTED, &lv_font_montserrat_10);
  lv_obj_align(s, LV_ALIGN_BOTTOM_LEFT, 4, -2);
  return b;
}

void build_wifi_setup_screen(bool fromSettings) {
  g_from_settings = fromSettings;
  NetworkManager::instance().resumeAuto();
  lv_obj_t* scr = new_screen();

  if (fromSettings) {
    back_button(scr, [](lv_event_t*) {
      cleanup_wifi_setup_screen();
      build_settings_screen();
    });
  }

  lv_obj_t* title = label(scr, "Connect to Wi-Fi", C_TEXT, &lv_font_montserrat_20);
  lv_obj_align(title, LV_ALIGN_TOP_MID, fromSettings ? 14 : 0, 8);

  method_button(scr, "From your phone browser", "HMI makes a hotspot, you pick Wi-Fi", 46,
                [](lv_event_t*) { build_portal_screen(); });
  method_button(scr, "Scan with the SmartHome app", "QR code + Bluetooth, phone stays online", 100,
                [](lv_event_t*) { build_app_screen(); });
  method_button(scr, "Type it on this screen", "Choose a network and enter the password", 154,
                [](lv_event_t*) { build_wifi_screen(g_from_settings); });

  swap_to(scr);
}
