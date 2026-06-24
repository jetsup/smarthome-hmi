#include "pairing_client.hpp"
#include "network_manager.hpp"
#include <ArduinoJson.h>

PairingClient& PairingClient::instance() {
    static PairingClient inst;
    return inst;
}

PairingResult PairingClient::requestPairing() {
    PairingResult result{false, "", ""};

    String body = "{\"gatewayId\":\"" + m_gatewayId +
                  "\",\"name\":\"" + m_gatewayName + "\"}";
    String resp;
    int code =
        NetworkManager::instance().httpPost("/api/pair/request", body, resp);

    if (code != 200) {
        result.error = "Server error: " + String(code);
        return result;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, resp);
    if (err || !doc["success"].as<bool>()) {
        Serial.printf("[Pairing] Response: code=%d body='%s' err=%s\n",
                      code, resp.c_str(), err.c_str());
        result.error = "Pairing request rejected";
        return result;
    }

    result.success = true;
    result.gatewayName = m_gatewayName;
    return result;
}

PairingResult PairingClient::verifyCode(const String& code) {
    PairingResult result{false, "", ""};

    if (code.length() != 6) {
        result.error = "Code must be 6 digits";
        return result;
    }

    String body = "{\"gatewayId\":\"" + m_gatewayId +
                  "\",\"code\":\"" + code + "\"}";
    String resp;
    int codeVal =
        NetworkManager::instance().httpPost("/api/pair/verify", body, resp);

    if (codeVal != 200) {
        result.error = "Verification failed (" + String(codeVal) + ")";
        return result;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, resp);
    if (err) {
        result.error = "Invalid server response";
        return result;
    }

    result.success = doc["success"].as<bool>();
    if (!result.success) {
        result.error = doc["error"].as<const char*>();
        if (result.error.isEmpty()) result.error = "Code incorrect";
    } else {
        result.gatewayName = m_gatewayName;
        m_paired = true;
    }

    return result;
}
