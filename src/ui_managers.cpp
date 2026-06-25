#include "ui_managers.hpp"

#include "config.hpp"
#include "data_manager.hpp"
#include "gateway_discovery.hpp"
#include "network_manager.hpp"
#include "pairing_client.hpp"
#include "preferences_manager.hpp"
#include "ui_dashboard.hpp"
#include "ui_gateway_scan.hpp"
#include "ui_pairing.hpp"
#include "ui_screens.hpp"
#include "ui_wifi_screen.hpp"

static void trigger_dashboard_with_data();

static lv_timer_t* logo_timer = nullptr;
static lv_timer_t* splash_timer = nullptr;
static lv_timer_t* progress_update_timer = nullptr;
static lv_timer_t* data_load_timer = nullptr;

// ── Logo screen ──────────────────────────────────────────────────────────────

static void logo_progress_cb(lv_timer_t* timer) {
  if (logo_progress_bar != nullptr) {
    int32_t current_val = lv_bar_get_value(logo_progress_bar);
    if (current_val < 100) {
      lv_bar_set_value(logo_progress_bar, current_val + 6, LV_ANIM_OFF);
    }
  }
}

static void logo_timeout_cb(lv_timer_t* timer) {
  if (progress_update_timer != nullptr) {
    lv_timer_delete(progress_update_timer);
    progress_update_timer = nullptr;
  }

  trigger_splash_sequence();

  if (logo_screen != nullptr) {
    lv_obj_delete(logo_screen);
    logo_screen = nullptr;
    logo_progress_bar = nullptr;
  }
  lv_timer_delete(timer);
}

// ── Splash screen ────────────────────────────────────────────────────────────

static void check_wifi_and_route() {
  auto& prefs = PreferencesManager::instance();

  if (prefs.hasWifiCredentials()) {
    auto creds = prefs.loadWifiCredentials();
    update_splash_status("Connecting to Wi-Fi...");
    NetworkManager::instance().begin(creds.ssid.c_str(),
                                     creds.password.c_str());
  }

  if (NetworkManager::instance().isWiFiConnected()) {
    if (prefs.hasPairedGateway()) {
      update_splash_status("Loading dashboard...");
      auto& dm = DataManager::instance();
      dm.setGatewayId(prefs.getPairedGatewayId());
      dm.setGatewayName(prefs.getPairedGatewayName());
      // Try fetch, falls back to cache if hub offline
      if (!dm.fetchNodes()) {
        Serial.println("[UI] Hub offline, trying cache...");
        if (dm.nodeCount() == 0) {
          update_splash_status("No cached data available.");
          trigger_gateway_scan();
          return;
        }
      }
      trigger_dashboard_with_data();
      return;
    }
    trigger_gateway_scan();
  } else if (prefs.hasWifiCredentials()) {
    update_splash_status("Saved Wi-Fi not available...");
  } else {
    update_splash_status("No Wi-Fi configured.");
  }
}

static void splash_data_cb(lv_timer_t* timer) {
  if (NetworkManager::instance().isWiFiConnected()) {
    auto& prefs = PreferencesManager::instance();

    if (prefs.hasPairedGateway()) {
      update_splash_status("Loading dashboard...");
      auto& dm = DataManager::instance();
      dm.setGatewayId(prefs.getPairedGatewayId());
      dm.setGatewayName(prefs.getPairedGatewayName());
      // Try fetch, falls back to cache if hub offline
      if (!dm.fetchNodes() && dm.nodeCount() == 0) {
        return;  // Wait for splash timeout
      }
      trigger_dashboard_with_data();
      return;
    }

    trigger_gateway_scan();
  }
}

static void splash_timeout_cb(lv_timer_t* timer) {
  if (data_load_timer) {
    lv_timer_delete(data_load_timer);
    data_load_timer = nullptr;
  }
  auto& prefs = PreferencesManager::instance();

  if (NetworkManager::instance().isWiFiConnected()) {
    if (prefs.hasPairedGateway()) {
      auto& dm = DataManager::instance();
      dm.setGatewayId(prefs.getPairedGatewayId());
      dm.setGatewayName(prefs.getPairedGatewayName());
      // Try fetch, falls back to cache; always show dashboard if paired
      dm.fetchNodes();
      trigger_dashboard_with_data();
    } else {
      trigger_gateway_scan();
    }
  } else {
    trigger_wifi_settings();
  }
  lv_timer_delete(timer);
}

// ── Exported triggers ────────────────────────────────────────────────────────

void trigger_logo_sequence() {
  build_logo_layout();
  lv_screen_load(logo_screen);

  progress_update_timer = lv_timer_create(logo_progress_cb, 60, nullptr);
  logo_timer = lv_timer_create(logo_timeout_cb, 1000, nullptr);
}

void trigger_splash_sequence() {
  build_splash_layout();
  lv_screen_load(splash_screen);

  auto& net = NetworkManager::instance();
  auto& prefs = PreferencesManager::instance();

  if (prefs.hasWifiCredentials()) {
    auto creds = prefs.loadWifiCredentials();
    net.begin(creds.ssid.c_str(), creds.password.c_str());
  }

  data_load_timer = lv_timer_create(splash_data_cb, 500, nullptr);
  splash_timer = lv_timer_create(splash_timeout_cb, 20000, nullptr);
}

void trigger_wifi_settings() {
  cleanup_gateway_scan_screen();
  if (data_load_timer != nullptr) {
    lv_timer_delete(data_load_timer);
    data_load_timer = nullptr;
  }
  if (splash_timer != nullptr) {
    lv_timer_delete(splash_timer);
    splash_timer = nullptr;
  }
  if (splash_screen != nullptr) {
    lv_obj_delete(splash_screen);
    splash_screen = nullptr;
  }
  build_wifi_screen();
}

void trigger_gateway_scan() {
  cleanup_wifi_screen();
  if (data_load_timer != nullptr) {
    lv_timer_delete(data_load_timer);
    data_load_timer = nullptr;
  }
  if (splash_timer != nullptr) {
    lv_timer_delete(splash_timer);
    splash_timer = nullptr;
  }
  if (splash_screen != nullptr) {
    lv_obj_delete(splash_screen);
    splash_screen = nullptr;
  }
  build_gateway_scan_screen();
}

void trigger_pairing_flow() {
  show_loading_dialog("Requesting pairing...");

  lv_timer_create(
      [](lv_timer_t* t) {
        auto& pc = PairingClient::instance();
        auto& dm = DataManager::instance();

        // Resolve decimal MAC ID to hex DB ID so TCP delivery and Flutter poll
        // work
        String hexId = dm.findHexGatewayId(pc.gatewayName());
        if (!hexId.isEmpty()) {
          pc.setGatewayId(hexId);
          Serial.printf("[Pairing] Resolved ID to hex: %s\n", hexId.c_str());
        } else {
          Serial.println("[Pairing] Using original ID (TCP/Flutter may fail)");
        }

        auto result = pc.requestPairing();

        hide_loading_dialog();

        if (result.success) {
          show_pairing_code_dialog();
        } else {
          show_error_dialog(
              (result.error + "\nCheck gateway connection").c_str());
        }
        lv_timer_del(t);
      },
      100, nullptr);
}

void trigger_main_dashboard_sequence() {
  cleanup_gateway_scan_screen();
  build_main_dashboard_layout();
  lv_screen_load(main_screen);
}

void trigger_dashboard_with_data() {
  if (data_load_timer != nullptr) {
    lv_timer_delete(data_load_timer);
    data_load_timer = nullptr;
  }
  if (splash_timer != nullptr) {
    lv_timer_delete(splash_timer);
    splash_timer = nullptr;
  }
  if (splash_screen != nullptr) {
    lv_obj_delete(splash_screen);
    splash_screen = nullptr;
  }
  trigger_main_dashboard_sequence();
}
