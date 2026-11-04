#pragma once
#include <Arduino.h>

class NetworkManager {
public:
    static NetworkManager& instance();

    void begin(const char* ssid, const char* password);

    // Connect to the best saved network: scan, then try the saved networks that
    // are in range in priority order, moving to the next if one fails. Keeps
    // retrying (e.g. the router is still booting) until connected. Call
    // update() from loop().
    enum class AutoState { Idle, NoneSaved, Scanning, Connecting, Connected, NoneInRange };
    void beginAuto();
    void update();
    AutoState autoState() const { return m_auto; }
    const String& autoTarget() const { return m_target; }
    void stopAuto() { m_auto = AutoState::Idle; }  // a setup mode owns the radio
    void adopt(const String& ssid) { m_auto = AutoState::Connected; m_target = ssid; m_stateSince = millis(); }
    void resumeAuto();  // restart auto-connect if idle and not connected
    void setHubAddress(const String& host, uint16_t port);
    bool isWiFiConnected() const;
    bool waitForWiFi(unsigned long timeoutMs);

    // Bearer token issued by the hub at pairing; sent on every request.
    void setAuthToken(const String& token) { m_token = token; }
    bool hasAuthToken() const { return !m_token.isEmpty(); }

    int httpGet(const String& path, String& response) const;
    int httpPost(const String& path, const String& body, String& response) const;

    // Fire-and-forget POST executed on a background task so the UI thread
    // (LVGL) never blocks on network I/O. Returns false if the queue is full.
    bool postAsync(const String& path, const String& body);
    void startWorker();

private:
    NetworkManager() = default;
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    AutoState m_auto = AutoState::Idle;
    String m_target;
    int m_candIdx[8] = {0};
    int m_candCount = 0;
    int m_candPos = 0;
    bool m_blind = false;           // second pass: try saved networks even if not seen
    uint32_t m_stateSince = 0;
    void startScan();
    void buildCandidates(int found);
    bool tryNext();

    String m_hubHost = "192.168.100.100";
    uint16_t m_hubPort = 9000;
    String m_token;
    void* m_queue = nullptr;  // QueueHandle_t
    static void workerTask(void* arg);
};
