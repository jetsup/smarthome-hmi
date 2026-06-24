#include "network_manager.hpp"
#include <WiFi.h>
#include <HTTPClient.h>

NetworkManager& NetworkManager::instance() {
    static NetworkManager inst;
    return inst;
}

void NetworkManager::begin(const char* ssid, const char* password) {
    WiFi.begin(ssid, password);
    Serial.printf("[NET] Connecting to %s...\n", ssid);
}

void NetworkManager::setHubAddress(const String& host, uint16_t port) {
    m_hubHost = host;
    m_hubPort = port;
}

bool NetworkManager::isWiFiConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

bool NetworkManager::waitForWiFi(unsigned long timeoutMs) {
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
        delay(100);
    }
    return WiFi.status() == WL_CONNECTED;
}

int NetworkManager::httpGet(const String& path, String& response) const {
    if (!isWiFiConnected()) return -1;
    WiFiClient client;
    HTTPClient http;
    String url = "http://" + m_hubHost + ":" + String(m_hubPort) + path;
    http.begin(client, url);
    http.setTimeout(5000);
    int code = http.GET();
    if (code > 0) {
        response = http.getString();
    }
    http.end();
    return code;
}

int NetworkManager::httpPost(const String& path, const String& body,
                             String& response) const {
    if (!isWiFiConnected()) return -1;
    WiFiClient client;
    HTTPClient http;
    String url = "http://" + m_hubHost + ":" + String(m_hubPort) + path;
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(5000);
    int code = http.POST(body);
    if (code > 0) {
        response = http.getString();
    }
    http.end();
    return code;
}
