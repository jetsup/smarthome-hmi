#include "preferences_manager.hpp"

#include <Preferences.h>

PreferencesManager& PreferencesManager::instance() {
  static PreferencesManager inst;
  return inst;
}

void PreferencesManager::init() {
  Preferences prefs;
  prefs.begin(NVS_NS, false);
  prefs.end();
}

// ── Saved Wi-Fi networks (priority order) ────────────────────────────────────

void PreferencesManager::loadWifiList(std::vector<WifiCredentials>& out) {
  out.clear();
  Preferences prefs;
  prefs.begin(NVS_NS, true);
  int n = prefs.getInt(KEY_WCNT, -1);
  if (n < 0) {
    // First run of the multi-network firmware: adopt the old single network.
    String s = prefs.getString(KEY_SSID, "");
    String p = prefs.getString(KEY_PASS, "");
    prefs.end();
    if (s.length() > 0) out.push_back({s, p});
    return;
  }
  for (int i = 0; i < n && i < kMaxWifi; i++) {
    String s = prefs.getString(("w_s" + String(i)).c_str(), "");
    String p = prefs.getString(("w_p" + String(i)).c_str(), "");
    if (s.length() > 0) out.push_back({s, p});
  }
  prefs.end();
}

void PreferencesManager::storeWifiList(const std::vector<WifiCredentials>& list) {
  Preferences prefs;
  prefs.begin(NVS_NS, false);
  int n = (int)list.size() < kMaxWifi ? (int)list.size() : kMaxWifi;
  for (int i = 0; i < kMaxWifi; i++) {
    String ks = "w_s" + String(i), kp = "w_p" + String(i);
    if (i < n) {
      prefs.putString(ks.c_str(), list[i].ssid);
      prefs.putString(kp.c_str(), list[i].password);
    } else {
      prefs.remove(ks.c_str());
      prefs.remove(kp.c_str());
    }
  }
  prefs.putInt(KEY_WCNT, n);
  prefs.remove(KEY_SSID);  // legacy keys are superseded
  prefs.remove(KEY_PASS);
  prefs.end();
}

int PreferencesManager::wifiCount() {
  std::vector<WifiCredentials> l;
  loadWifiList(l);
  return (int)l.size();
}

bool PreferencesManager::wifiAt(int index, String& ssid, String& password) {
  std::vector<WifiCredentials> l;
  loadWifiList(l);
  if (index < 0 || index >= (int)l.size()) return false;
  ssid = l[index].ssid;
  password = l[index].password;
  return true;
}

bool PreferencesManager::hasWifiCredentials() { return wifiCount() > 0; }

WifiCredentials PreferencesManager::loadWifiCredentials() {
  WifiCredentials c;
  wifiAt(0, c.ssid, c.password);
  return c;
}

void PreferencesManager::saveWifiCredentials(const String& ssid,
                                             const String& password,
                                             bool makeFirst) {
  std::vector<WifiCredentials> l;
  loadWifiList(l);
  int found = -1;
  for (int i = 0; i < (int)l.size(); i++)
    if (l[i].ssid == ssid) found = i;
  if (found >= 0) {
    l[found].password = password;
    if (makeFirst && found > 0) {
      WifiCredentials c = l[found];
      l.erase(l.begin() + found);
      l.insert(l.begin(), c);
    }
  } else if (makeFirst) {
    l.insert(l.begin(), {ssid, password});
  } else {
    l.push_back({ssid, password});
  }
  if ((int)l.size() > kMaxWifi) l.resize(kMaxWifi);  // drop the lowest priority
  storeWifiList(l);
}

void PreferencesManager::removeWifiAt(int index) {
  std::vector<WifiCredentials> l;
  loadWifiList(l);
  if (index < 0 || index >= (int)l.size()) return;
  l.erase(l.begin() + index);
  storeWifiList(l);
}

void PreferencesManager::moveWifi(int index, int delta) {
  std::vector<WifiCredentials> l;
  loadWifiList(l);
  int to = index + delta;
  if (index < 0 || index >= (int)l.size() || to < 0 || to >= (int)l.size()) return;
  std::swap(l[index], l[to]);
  storeWifiList(l);
}

void PreferencesManager::clearWifiCredentials() {
  std::vector<WifiCredentials> none;
  storeWifiList(none);
}

bool PreferencesManager::hasPairedGateway() {
  Preferences prefs;
  prefs.begin(NVS_NS, true);
  bool has = prefs.isKey(KEY_GW_ID);
  prefs.end();
  return has;
}

String PreferencesManager::getPairedGatewayId() {
  Preferences prefs;
  prefs.begin(NVS_NS, true);
  String id = prefs.getString(KEY_GW_ID, "");
  prefs.end();
  return id;
}

String PreferencesManager::getPairedGatewayName() {
  Preferences prefs;
  prefs.begin(NVS_NS, true);
  String name = prefs.getString(KEY_GW_NAME, "");
  prefs.end();
  return name;
}

void PreferencesManager::setPairedGateway(const String& gatewayId,
                                          const String& name) {
  Preferences prefs;
  prefs.begin(NVS_NS, false);
  prefs.putString(KEY_GW_ID, gatewayId);
  prefs.putString(KEY_GW_NAME, name);
  prefs.end();
}

void PreferencesManager::clearPairedGateway() {
  Preferences prefs;
  prefs.begin(NVS_NS, false);
  prefs.remove(KEY_GW_ID);
  prefs.remove(KEY_GW_NAME);
  prefs.remove(KEY_TOKEN);
  prefs.end();
}

String PreferencesManager::getHubToken() {
  Preferences prefs;
  prefs.begin(NVS_NS, true);
  String tok = prefs.getString(KEY_TOKEN, "");
  prefs.end();
  return tok;
}

void PreferencesManager::setHubToken(const String& token) {
  Preferences prefs;
  prefs.begin(NVS_NS, false);
  prefs.putString(KEY_TOKEN, token);
  prefs.end();
}

bool PreferencesManager::isFirstBoot() {
  Preferences prefs;
  prefs.begin(NVS_NS, true);
  bool first = prefs.getBool(KEY_FIRST, true);
  prefs.end();
  return first;
}

void PreferencesManager::clearFirstBoot() {
  Preferences prefs;
  prefs.begin(NVS_NS, false);
  prefs.putBool(KEY_FIRST, false);
  prefs.end();
}

void PreferencesManager::cacheNodes(const String& json) {
  Preferences prefs;
  prefs.begin(NVS_NS, false);
  prefs.putString(KEY_NODES, json);
  prefs.end();
  Serial.printf("[Prefs] Cached %u bytes of node data\n", json.length());
}

String PreferencesManager::getCachedNodes() {
  Preferences prefs;
  prefs.begin(NVS_NS, true);
  String data = prefs.getString(KEY_NODES, "");
  prefs.end();
  return data;
}

bool PreferencesManager::hasCachedNodes() {
  Preferences prefs;
  prefs.begin(NVS_NS, true);
  bool has = prefs.isKey(KEY_NODES);
  prefs.end();
  return has;
}

void PreferencesManager::clearSystem() {
  Preferences prefs;
  prefs.begin(NVS_NS, false);
  prefs.clear();
  prefs.end();
  Serial.println("[Prefs] All NVS data cleared");
}
