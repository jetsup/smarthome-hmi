#pragma once
#include <Arduino.h>

#include <vector>

struct WifiCredentials {
  String ssid;
  String password;
};

class PreferencesManager {
 public:
  static PreferencesManager& instance();

  void init();

  // Saved Wi-Fi networks, kept in priority order (index 0 is tried first).
  static constexpr int kMaxWifi = 6;
  int wifiCount();
  bool wifiAt(int index, String& ssid, String& password);
  bool hasWifiCredentials();                  // at least one saved network
  WifiCredentials loadWifiCredentials();      // the highest-priority network
  // Add a network, or update its password. `makeFirst` gives it top priority
  // (used when the user has just set a network up on purpose).
  void saveWifiCredentials(const String& ssid, const String& password,
                           bool makeFirst = true);
  void removeWifiAt(int index);
  void moveWifi(int index, int delta);        // delta -1 = higher priority
  void clearWifiCredentials();

  bool hasPairedGateway();
  String getPairedGatewayId();
  String getPairedGatewayName();
  void setPairedGateway(const String& gatewayId, const String& name);
  void clearPairedGateway();

  String getHubToken();
  void setHubToken(const String& token);

  bool isFirstBoot();
  void clearFirstBoot();

  void cacheNodes(const String& json);
  String getCachedNodes();
  bool hasCachedNodes();

  void clearSystem();

 private:
  PreferencesManager() = default;
  PreferencesManager(const PreferencesManager&) = delete;
  PreferencesManager& operator=(const PreferencesManager&) = delete;

  static constexpr const char* NVS_NS = "hmi";
  static constexpr const char* KEY_SSID = "w_ssid";  // legacy single network
  static constexpr const char* KEY_PASS = "w_pass";
  static constexpr const char* KEY_WCNT = "w_cnt";
  void loadWifiList(std::vector<WifiCredentials>& out);
  void storeWifiList(const std::vector<WifiCredentials>& list);
  static constexpr const char* KEY_GW_ID = "gw_id";
  static constexpr const char* KEY_GW_NAME = "gw_name";
  static constexpr const char* KEY_TOKEN = "hub_tok";
  static constexpr const char* KEY_FIRST = "first_boot";
  static constexpr const char* KEY_NODES = "nodes";
};
