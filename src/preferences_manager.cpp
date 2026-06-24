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

bool PreferencesManager::hasWifiCredentials() {
    Preferences prefs;
    prefs.begin(NVS_NS, true);
    bool has = prefs.isKey(KEY_SSID) && prefs.isKey(KEY_PASS);
    prefs.end();
    return has;
}

WifiCredentials PreferencesManager::loadWifiCredentials() {
    WifiCredentials creds;
    Preferences prefs;
    prefs.begin(NVS_NS, true);
    creds.ssid = prefs.getString(KEY_SSID, "");
    creds.password = prefs.getString(KEY_PASS, "");
    prefs.end();
    return creds;
}

void PreferencesManager::saveWifiCredentials(const String& ssid,
                                              const String& password) {
    Preferences prefs;
    prefs.begin(NVS_NS, false);
    prefs.putString(KEY_SSID, ssid);
    prefs.putString(KEY_PASS, password);
    prefs.end();
}

void PreferencesManager::clearWifiCredentials() {
    Preferences prefs;
    prefs.begin(NVS_NS, false);
    prefs.remove(KEY_SSID);
    prefs.remove(KEY_PASS);
    prefs.end();
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
