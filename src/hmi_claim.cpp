#include "hmi_claim.hpp"

#include <ArduinoJson.h>

#include "device_id.hpp"
#include "network_manager.hpp"

ClaimCode hmiRequestClaimCode() {
  ClaimCode out;
  // No name sent: the hub numbers displays ("Display 1", ...) and the owner can rename.
  String body = "{\"deviceId\":\"" + hmiDeviceId() + "\"}";
  String resp;
  int code = NetworkManager::instance().httpPost("/api/hmi/register", body, resp);
  if (code != 200) {
    out.error = code < 0 ? "Cannot reach the hub" : "Hub error " + String(code);
    return out;
  }
  JsonDocument doc;
  if (deserializeJson(doc, resp) || !doc["code"].is<const char*>()) {
    out.error = "Bad reply from the hub";
    return out;
  }
  out.ok = true;
  out.code = String(doc["code"].as<const char*>());
  out.secret = String(doc["secret"] | "");
  out.expiresIn = doc["expiresIn"] | 600;
  return out;
}

ClaimResult hmiPollClaim(const String& secret) {
  ClaimResult r;
  String resp;
  String path = "/api/hmi/register/status?deviceId=" + hmiDeviceId() + "&secret=" + secret;
  int code = NetworkManager::instance().httpGet(path, resp);
  if (code != 200) return r;  // Error (hub unreachable / bad secret)
  JsonDocument doc;
  if (deserializeJson(doc, resp)) return r;
  String st = String(doc["status"] | "");
  if (st == "pending") {
    r.state = ClaimState::Pending;
  } else if (st == "claimed") {
    r.state = ClaimState::Claimed;
    r.token = String(doc["token"] | "");
    r.gatewayId = String(doc["gatewayId"] | "");
    r.gatewayName = String(doc["gatewayName"] | "");
    if (r.token.isEmpty()) r.state = ClaimState::Error;
  } else {
    r.state = ClaimState::Expired;
  }
  return r;
}
