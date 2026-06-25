#pragma once
#include <Arduino.h>

struct WifiCredentials {
  String ssid;
  String password;
};

class PreferencesManager {
 public:
  static PreferencesManager& instance();

  void init();

  bool hasWifiCredentials();
  WifiCredentials loadWifiCredentials();
  void saveWifiCredentials(const String& ssid, const String& password);
  void clearWifiCredentials();

  bool hasPairedGateway();
  String getPairedGatewayId();
  String getPairedGatewayName();
  void setPairedGateway(const String& gatewayId, const String& name);
  void clearPairedGateway();

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
  static constexpr const char* KEY_SSID = "w_ssid";
  static constexpr const char* KEY_PASS = "w_pass";
  static constexpr const char* KEY_GW_ID = "gw_id";
  static constexpr const char* KEY_GW_NAME = "gw_name";
  static constexpr const char* KEY_FIRST = "first_boot";
  static constexpr const char* KEY_NODES = "nodes";
};
