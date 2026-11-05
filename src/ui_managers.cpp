#include "theme.hpp"
#include <WiFi.h>
#include "ui_managers.hpp"

#include "config.hpp"
#include "data_manager.hpp"
#include "gateway_discovery.hpp"
#include "network_manager.hpp"
#include "wifi_provisioner.hpp"
#include "pairing_client.hpp"
#include "preferences_manager.hpp"
#include "ui_dashboard.hpp"
#include "ui_gateway_scan.hpp"
#include "ui_pair_screens.hpp"
#include "ui_pairing.hpp"
#include "ui_screens.hpp"
#include "ui_wifi_screen.hpp"
#include "ui_wifi_setup.hpp"

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
  // Tell the user what the connection logic is doing.
  {
    auto& nm = NetworkManager::instance();
    String msg;
    switch (nm.autoState()) {
      case NetworkManager::AutoState::Scanning: msg = "Looking for your Wi-Fi..."; break;
      case NetworkManager::AutoState::Connecting: msg = "Connecting to " + nm.autoTarget() + "..."; break;
      case NetworkManager::AutoState::NoneInRange: msg = "Saved Wi-Fi not in range, retrying..."; break;
      case NetworkManager::AutoState::Connected: msg = "Connected to " + nm.autoTarget(); break;
      default: break;
    }
    if (msg.length()) update_splash_status(msg.c_str());
  }
  static uint32_t lastLog = 0;
  if (millis() - lastLog > 2000) {
    lastLog = millis();
    Serial.printf("[UI] splash: wifi=%d paired=%d auto=%d status=%d\n",
                  NetworkManager::instance().isWiFiConnected(),
                  PreferencesManager::instance().hasPairedGateway(),
                  (int)NetworkManager::instance().autoState(), (int)WiFi.status());
  }
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
  // The saved network may still be being found / joined (a scan can miss it once and
  // the join itself can take a couple of tries): keep waiting while that is going on.
  static int extensions = 0;
  {
    auto st = NetworkManager::instance().autoState();
    bool working = st == NetworkManager::AutoState::Scanning || st == NetworkManager::AutoState::Connecting ||
                   st == NetworkManager::AutoState::NoneInRange;
    if (!NetworkManager::instance().isWiFiConnected() && working && PreferencesManager::instance().hasWifiCredentials() && extensions < 4) {
      extensions++;
      lv_timer_set_period(timer, 10000);
      Serial.printf("[UI] still looking for saved Wi-Fi (extension %d)\n", extensions);
      return;
    }
  }

  // This callback IS splash_timer. Forget the handle first so the trigger_*
  // helpers don't delete the running timer a second time (use-after-free).
  if (timer == splash_timer) splash_timer = nullptr;
  if (data_load_timer) {
    lv_timer_delete(data_load_timer);
    data_load_timer = nullptr;
  }
  auto& prefs = PreferencesManager::instance();
  Serial.printf("[UI] splash timeout: wifi=%d paired=%d -> %s\n", NetworkManager::instance().isWiFiConnected(),
                prefs.hasPairedGateway(), NetworkManager::instance().isWiFiConnected() ? "dashboard/scan" : "WI-FI SCREEN");

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

  // Pick the best saved network (priority order, in range) without blocking the UI.
  if (prefs.hasWifiCredentials()) net.beginAuto();

  data_load_timer = lv_timer_create(splash_data_cb, 500, nullptr);
  // With nothing saved there is nothing to wait for: offer setup right away.
  bool haveCreds = prefs.hasWifiCredentials();
  if (!haveCreds) update_splash_status("No Wi-Fi configured.");
  splash_timer = lv_timer_create(splash_timeout_cb, haveCreds ? 20000 : 1200, nullptr);
}

// The Wi-Fi screen is only for when nothing saved can be joined. If the automatic
// connection succeeds in the background (late scan, router just booted…), carry on.
static lv_timer_t* wifi_watch_timer = nullptr;
static void wifi_watch_cb(lv_timer_t* t) {
  if (!NetworkManager::instance().isWiFiConnected()) return;
  wifi_watch_timer = nullptr;
  lv_timer_delete(t);
  WifiProvisioner::instance().stop();
  Serial.println("[UI] Wi-Fi joined in the background - leaving the Wi-Fi screen");
  trigger_after_wifi_connected();
}

void trigger_wifi_settings() {
  if (wifi_watch_timer == nullptr) wifi_watch_timer = lv_timer_create(wifi_watch_cb, 1000, nullptr);
  cleanup_gateway_scan_screen();
  if (data_load_timer != nullptr) {
    lv_timer_delete(data_load_timer);
    data_load_timer = nullptr;
  }
  if (splash_timer != nullptr) {
    lv_timer_delete(splash_timer);
    splash_timer = nullptr;
  }
  cleanup_wifi_screen();
  // build_wifi_setup_screen() deletes whichever screen is active (the splash),
  // so only drop our pointer to it.
  splash_screen = nullptr;
  build_wifi_setup_screen(false);
}

void trigger_after_wifi_connected() {
  if (wifi_watch_timer != nullptr) {
    lv_timer_delete(wifi_watch_timer);
    wifi_watch_timer = nullptr;
  }
  cleanup_wifi_screen();
  cleanup_wifi_setup_screen();
  lv_obj_t* old = lv_screen_active();
  auto& prefs = PreferencesManager::instance();
  if (prefs.hasPairedGateway()) {
    auto& dm = DataManager::instance();
    dm.setGatewayId(prefs.getPairedGatewayId());
    dm.setGatewayName(prefs.getPairedGatewayName());
    dm.fetchNodes();  // falls back to cache if the hub is unreachable
    trigger_dashboard_with_data();
  } else {
    trigger_gateway_scan();
  }
  // Free the setup screen we came from (it holds a QR canvas).
  if (old && old != lv_screen_active() && lv_obj_is_valid(old)) lv_obj_delete(old);
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
  // The pairing chooser replaces (and deletes) whichever screen is showing, so
  // only forget the splash pointer here.
  splash_screen = nullptr;
  build_pair_method_screen(false);
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
  cleanup_pair_screens();
  lv_obj_t* old = lv_screen_active();
  build_main_dashboard_layout();
  lv_screen_load(main_screen);
  // Free the pairing / setup screen we came from (the QR canvas is large).
  if (old && old != main_screen && lv_obj_is_valid(old)) lv_obj_delete(old);
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
