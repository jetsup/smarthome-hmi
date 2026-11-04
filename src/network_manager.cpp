#include "network_manager.hpp"
#include "preferences_manager.hpp"
#include <WiFi.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace {
struct AsyncReq {
    char path[96];
    char body[160];
};
constexpr int kQueueDepth = 8;
constexpr uint16_t kHttpTimeoutMs = 2000;
}  // namespace

NetworkManager& NetworkManager::instance() {
    static NetworkManager inst;
    return inst;
}

void NetworkManager::begin(const char* ssid, const char* password) {
    WiFi.begin(ssid, password);
    Serial.printf("[NET] Connecting to %s...\n", ssid);
}

// ── Automatic connection to saved networks ───────────────────────────────────

void NetworkManager::startScan() {
    WiFi.mode(WIFI_STA);
    WiFi.scanDelete();
    WiFi.scanNetworks(true);  // async: the UI keeps running
    m_auto = AutoState::Scanning;
    m_stateSince = millis();
}

void NetworkManager::beginAuto() {
    auto& prefs = PreferencesManager::instance();
    if (!prefs.hasWifiCredentials()) {
        m_auto = AutoState::NoneSaved;
        return;
    }
    m_blind = false;
    startScan();
}

// Saved networks that were seen in the scan, in priority order. On the second
// pass (nothing seen) every saved network is a candidate, which also covers
// hidden networks.
void NetworkManager::buildCandidates(int found) {
    auto& prefs = PreferencesManager::instance();
    m_candCount = 0;
    m_candPos = 0;
    int saved = prefs.wifiCount();
    for (int k = 0; k < saved && m_candCount < 8; k++) {
        String s, p;
        if (!prefs.wifiAt(k, s, p)) continue;
        bool seen = false;
        for (int i = 0; i < found; i++)
            if (WiFi.SSID(i) == s) { seen = true; break; }
        if (seen || m_blind) m_candIdx[m_candCount++] = k;
    }
    WiFi.scanDelete();
}

bool NetworkManager::tryNext() {
    if (m_candPos >= m_candCount) return false;
    String s, p;
    if (!PreferencesManager::instance().wifiAt(m_candIdx[m_candPos++], s, p)) return tryNext();
    m_target = s;
    WiFi.disconnect(false);
    WiFi.begin(s.c_str(), p.c_str());
    Serial.printf("[NET] Trying saved network %d/%d: %s\n", m_candPos, m_candCount, s.c_str());
    m_auto = AutoState::Connecting;
    m_stateSince = millis();
    return true;
}

void NetworkManager::resumeAuto() {
    if (m_auto == AutoState::Idle && !isWiFiConnected() &&
        PreferencesManager::instance().hasWifiCredentials()) {
        beginAuto();
    }
}

void NetworkManager::update() {
    switch (m_auto) {
        case AutoState::Scanning: {
            int n = WiFi.scanComplete();
            if (n == WIFI_SCAN_RUNNING) {
                if (millis() - m_stateSince > 15000) startScan();  // stuck: rescan
                break;
            }
            buildCandidates(n > 0 ? n : 0);
            if (m_candCount == 0 || !tryNext()) {
                m_auto = AutoState::NoneInRange;
                m_stateSince = millis();
                m_blind = !m_blind;  // next pass: try them blind once
            }
            break;
        }
        case AutoState::Connecting:
            if (WiFi.status() == WL_CONNECTED) {
                m_auto = AutoState::Connected;
                m_blind = false;
                Serial.printf("[NET] Connected to %s\n", m_target.c_str());
            } else if (millis() - m_stateSince > 12000) {
                if (!tryNext()) {  // all candidates failed: start over shortly
                    m_auto = AutoState::NoneInRange;
                    m_stateSince = millis();
                }
            }
            break;
        case AutoState::NoneInRange:
            if (millis() - m_stateSince > 6000) startScan();  // keep looking
            break;
        case AutoState::Connected:
            // Lost the link for a while (router rebooted, moved out of range):
            // pick the best network again rather than waiting on one SSID.
            if (WiFi.status() != WL_CONNECTED) {
                if (millis() - m_stateSince > 20000) { m_blind = false; startScan(); }
            } else {
                m_stateSince = millis();
            }
            break;
        default:
            break;
    }
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
    http.setConnectTimeout(kHttpTimeoutMs);
    http.setTimeout(kHttpTimeoutMs);
    if (!m_token.isEmpty()) http.addHeader("Authorization", "Bearer " + m_token);
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
    if (!m_token.isEmpty()) http.addHeader("Authorization", "Bearer " + m_token);
    http.setConnectTimeout(kHttpTimeoutMs);
    http.setTimeout(kHttpTimeoutMs);
    int code = http.POST(body);
    if (code > 0) {
        response = http.getString();
    }
    http.end();
    return code;
}

void NetworkManager::startWorker() {
    if (m_queue) return;
    m_queue = xQueueCreate(kQueueDepth, sizeof(AsyncReq));
    xTaskCreatePinnedToCore(workerTask, "net_worker", 8192, this, 1, nullptr, 0);
}

bool NetworkManager::postAsync(const String& path, const String& body) {
    if (!m_queue) startWorker();
    if (path.length() >= sizeof(AsyncReq::path) ||
        body.length() >= sizeof(AsyncReq::body)) {
        return false;
    }
    AsyncReq req = {};
    strlcpy(req.path, path.c_str(), sizeof(req.path));
    strlcpy(req.body, body.c_str(), sizeof(req.body));
    return xQueueSend((QueueHandle_t)m_queue, &req, 0) == pdTRUE;
}

void NetworkManager::workerTask(void* arg) {
    auto* self = static_cast<NetworkManager*>(arg);
    AsyncReq req;
    for (;;) {
        if (xQueueReceive((QueueHandle_t)self->m_queue, &req, portMAX_DELAY) == pdTRUE) {
            String resp;
            int code = self->httpPost(req.path, req.body, resp);
            if (code != 200) {
                Serial.printf("[NET] async POST %s -> %d\n", req.path, code);
            }
        }
    }
}
