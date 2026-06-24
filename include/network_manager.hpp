#pragma once
#include <Arduino.h>

class NetworkManager {
public:
    static NetworkManager& instance();

    void begin(const char* ssid, const char* password);
    void setHubAddress(const String& host, uint16_t port);
    bool isWiFiConnected() const;
    bool waitForWiFi(unsigned long timeoutMs);

    int httpGet(const String& path, String& response) const;
    int httpPost(const String& path, const String& body, String& response) const;

private:
    NetworkManager() = default;
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    String m_hubHost = "192.168.100.100";
    uint16_t m_hubPort = 9000;
};
