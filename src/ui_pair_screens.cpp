#include "theme.hpp"
#include "ui_pair_screens.hpp"

#include "data_manager.hpp"
#include "device_id.hpp"
#include "display.hpp"
#include "hmi_claim.hpp"
#include "network_manager.hpp"
#include "preferences_manager.hpp"
#include "ui_gateway_scan.hpp"
#include "ui_pairing.hpp"
#include "ui_settings.hpp"

#define C_BG th_panel()
#define C_CARD th_surface()
#define C_ACCENT th_accent()
#define C_TEXT th_text()
#define C_MUTED th_muted()
#define C_ONACC 0xFFFFFFu
static const uint32_t C_OK = 0x3FB950, C_BAD = 0xF85149;

static bool g_from_settings = false;
static lv_timer_t* g_timer = nullptr;
static lv_obj_t* g_code_lbl = nullptr;
static lv_obj_t* g_status = nullptr;
static lv_obj_t* g_qr = nullptr;
static String g_secret;
static uint32_t g_code_at = 0;
static int g_expires_in = 600;

void cleanup_pair_screens() {
  if (g_timer) {
    lv_timer_delete(g_timer);
    g_timer = nullptr;
  }
  g_code_lbl = g_status = g_qr = nullptr;
  g_secret = "";
}

// Replace the active screen. Only the OLD screen's timer is stopped here: the
// caller has already pointed g_code_lbl / g_status / g_qr at the NEW screen's
// widgets, so they must not be reset (doing so left the code label blank and the
// QR stuck on its placeholder).
static void swap_to(lv_obj_t* scr) {
  if (g_timer) {
    lv_timer_delete(g_timer);
    g_timer = nullptr;
  }
  lv_obj_t* old = lv_screen_active();
  lv_screen_load(scr);
  if (old && old != scr && lv_obj_is_valid(old)) lv_obj_delete(old);
}

static lv_obj_t* new_screen() {
  lv_obj_t* scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), LV_PART_MAIN);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  return scr;
}

static lv_obj_t* label(lv_obj_t* parent, const char* text, uint32_t color, const lv_font_t* font) {
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
  lv_obj_center(label(b, LV_SYMBOL_LEFT, C_TEXT, &lv_font_montserrat_14));
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
  return b;
}

// ── Method chooser ───────────────────────────────────────────────────────────

static lv_obj_t* method_button(lv_obj_t* scr, const char* title, const char* sub, int y,
                               lv_event_cb_t cb) {
  lv_obj_t* b = lv_button_create(scr);
  lv_obj_set_size(b, 280, 52);
  lv_obj_align(b, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_set_style_bg_color(b, lv_color_hex(C_CARD), LV_PART_MAIN);
  lv_obj_set_style_radius(b, 8, LV_PART_MAIN);
  lv_obj_set_style_border_width(b, 0, LV_PART_MAIN);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_align(label(b, title, C_TEXT, &lv_font_montserrat_14), LV_ALIGN_TOP_LEFT, 4, 4);
  lv_obj_align(label(b, sub, C_MUTED, &lv_font_montserrat_10), LV_ALIGN_BOTTOM_LEFT, 4, -4);
  return b;
}

void build_pair_method_screen(bool fromSettings) {
  g_from_settings = fromSettings;
  lv_obj_t* scr = new_screen();

  if (fromSettings) {
    back_button(scr, [](lv_event_t*) {
      cleanup_pair_screens();
      build_settings_screen();
    });
  }

  lv_obj_t* title = label(scr, "Pair this display", C_TEXT, &lv_font_montserrat_20);
  lv_obj_align(title, LV_ALIGN_TOP_MID, fromSettings ? 14 : 0, 8);

  method_button(scr, "From the app or website", "Shows a code and QR for you to confirm", 52,
                [](lv_event_t*) { build_claim_screen(g_from_settings); });
  method_button(scr, "Pick a nearby gateway", "Choose it here, then type its 6-digit code", 114,
                [](lv_event_t*) {
                  cleanup_pair_screens();
                  build_gateway_scan_screen(g_from_settings);
                });

  String id = "This display: " + hmiShortId();
  lv_obj_t* idl = label(scr, id.c_str(), C_MUTED, &lv_font_montserrat_10);
  lv_obj_align(idl, LV_ALIGN_BOTTOM_MID, 0, -10);

  swap_to(scr);
}

// ── Claim screen: code + QR, confirmed from the app / website ────────────────

static void set_status(const String& text, uint32_t color) {
  if (!g_status) return;
  lv_label_set_text(g_status, text.c_str());
  lv_obj_set_style_text_color(g_status, lv_color_hex(color), LV_PART_MAIN);
}

static void show_code(const String& code) {
  if (g_code_lbl) {
    String spaced = code.substring(0, 3) + " " + code.substring(3);
    lv_label_set_text(g_code_lbl, spaced.c_str());
  }
  if (g_qr) {
    // The app scans this; the website user types the digits instead.
    String payload = "SHHMI1;c=" + code + ";d=" + hmiDeviceId();
    lv_qrcode_update(g_qr, payload.c_str(), payload.length());
  }
}

static void request_code() {
  set_status("Getting a code...", C_MUTED);
  ClaimCode c = hmiRequestClaimCode();
  if (!c.ok) {
    g_secret = "";
    set_status(c.error + " - retrying", C_BAD);
    return;
  }
  g_secret = c.secret;
  g_code_at = millis();
  g_expires_in = c.expiresIn > 30 ? c.expiresIn : 600;
  show_code(c.code);
  set_status("Waiting for you to confirm...", C_MUTED);
}

static void finish(const ClaimResult& r) {
  auto& prefs = PreferencesManager::instance();
  prefs.setHubToken(r.token);
  prefs.setPairedGateway(r.gatewayId, r.gatewayName);
  NetworkManager::instance().setAuthToken(r.token);
  auto& dm = DataManager::instance();
  dm.setGatewayId(r.gatewayId);
  dm.setGatewayName(r.gatewayName);

  if (g_timer) {  // stop polling; the screen is replaced by the dashboard
    lv_timer_delete(g_timer);
    g_timer = nullptr;
  }
  String msg = "Paired with " + (r.gatewayName.isEmpty() ? String("your gateway") : r.gatewayName) + "!";
  show_pairing_result_dialog(true, msg.c_str());
}

static void tick(lv_timer_t*) {
  if (g_secret.isEmpty()) {
    request_code();
    return;
  }
  // Ask for a fresh code a little before the current one expires.
  if (millis() - g_code_at > (uint32_t)(g_expires_in - 20) * 1000UL) {
    request_code();
    return;
  }
  ClaimResult r = hmiPollClaim(g_secret);
  switch (r.state) {
    case ClaimState::Claimed: finish(r); break;
    case ClaimState::Expired: request_code(); break;
    default: break;
  }
}

void build_claim_screen(bool fromSettings) {
  g_from_settings = fromSettings;
  g_secret = "";  // always start with a fresh code
  lv_obj_t* scr = new_screen();

  back_button(scr, [](lv_event_t*) {
    cleanup_pair_screens();
    build_pair_method_screen(g_from_settings);
  });

  lv_obj_t* title = label(scr, "Pair this display", C_TEXT, &lv_font_montserrat_14);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 56, 12);

  lv_obj_t* steps = label(scr,
      "App: Displays > Pair, scan\n"
      "the QR code.\n"
      "Website: Displays > Pair,\n"
      "then enter this code:",
      C_MUTED, &lv_font_montserrat_10);
  lv_obj_align(steps, LV_ALIGN_TOP_LEFT, 12, 48);

  g_code_lbl = label(scr, "--- ---", C_ACCENT, &lv_font_montserrat_28);
  lv_obj_align(g_code_lbl, LV_ALIGN_TOP_LEFT, 12, 112);

  g_status = label(scr, "Getting a code...", C_MUTED, &lv_font_montserrat_10);
  lv_label_set_long_mode(g_status, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_width(g_status, 150);
  lv_obj_align(g_status, LV_ALIGN_BOTTOM_LEFT, 12, -28);

  String id = "Display " + hmiShortId();
  lv_obj_t* idl = label(scr, id.c_str(), C_MUTED, &lv_font_montserrat_10);
  lv_obj_align(idl, LV_ALIGN_BOTTOM_LEFT, 12, -10);

  g_qr = lv_qrcode_create(scr);
  lv_qrcode_set_size(g_qr, 138);
  lv_qrcode_set_dark_color(g_qr, lv_color_black());
  lv_qrcode_set_light_color(g_qr, lv_color_white());
  lv_obj_set_style_border_color(g_qr, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_border_width(g_qr, 6, LV_PART_MAIN);
  lv_obj_align(g_qr, LV_ALIGN_RIGHT_MID, -10, 6);
  String ph = "SHHMI1;d=" + hmiDeviceId();
  lv_qrcode_update(g_qr, ph.c_str(), ph.length());

  swap_to(scr);

  // Register shortly after the screen is up (it does a short blocking request).
  g_timer = lv_timer_create(
      [](lv_timer_t* t) {
        lv_timer_set_period(t, 3000);  // then poll every 3 s
        tick(t);
      },
      250, nullptr);
}
