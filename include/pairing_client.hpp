#pragma once
#include <Arduino.h>

struct PairingResult {
    bool success;
    String error;
    String gatewayName;
};

class PairingClient {
public:
    static PairingClient& instance();

    void setGatewayId(const String& id) { m_gatewayId = id; }
    void setGatewayName(const String& name) { m_gatewayName = name; }

    PairingResult requestPairing();
    PairingResult verifyCode(const String& code);

    bool isPaired() const { return m_paired; }
    String pairedGatewayId() const { return m_gatewayId; }
    String pairedGatewayName() const { return m_gatewayName; }
    String gatewayName() const { return m_gatewayName; }

private:
    PairingClient() = default;
    PairingClient(const PairingClient&) = delete;
    PairingClient& operator=(const PairingClient&) = delete;

    String m_gatewayId;
    String m_gatewayName;
    bool m_paired = false;
};
