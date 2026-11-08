#include "theme.hpp"
#include "data_manager.hpp"
#include "lockscreen.hpp"

#include <ArduinoJson.h>

#include "network_manager.hpp"
#include "preferences_manager.hpp"

// code <= 0 means the request never got an answer (hub down / unreachable).
static String httpErrorText(int code) {
  return code <= 0 ? String("Hub offline") : "HTTP " + String(code);
}

DataManager& DataManager::instance() {
  static DataManager inst;
  return inst;
}

bool DataManager::fetchGateways() {
  m_error = false;
  String resp;
  int code = NetworkManager::instance().httpGet("/api/hmi/gateways", resp);
  if (code != 200) {
    m_error = true;
    m_lastError = httpErrorText(code);
    if (m_errorCb) m_errorCb();
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, resp);
  if (err || !doc.is<JsonArray>()) {
    m_error = true;
    m_lastError = "JSON parse failed";
    if (m_errorCb) m_errorCb();
    return false;
  }

  JsonArray arr = doc.as<JsonArray>();
  if (arr.size() == 0) {
    m_error = true;
    m_lastError = "No gateway present";
    if (m_errorCb) m_errorCb();
    return false;
  }

  m_gatewayId = String((arr[0]["id"] | ""));
  return !m_gatewayId.isEmpty();
}

String DataManager::findHexGatewayId(const String& name) {
  if (name.isEmpty()) return "";
  String resp;
  int code = NetworkManager::instance().httpGet("/api/hmi/gateways", resp);
  if (code != 200) return "";
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, resp);
  if (err || !doc.is<JsonArray>()) return "";
  for (JsonObject gw : doc.as<JsonArray>()) {
    String gwName = (gw["name"] | "");
    String gwId = (gw["id"] | "");
    if (gwName.startsWith(name) || name.startsWith(gwName)) {
      return gwId;
    }
  }
  return "";
}

bool DataManager::refreshGatewayName() {
  String resp;
  if (NetworkManager::instance().httpGet("/api/hmi/gateway", resp) != 200) return false;
  JsonDocument doc;
  if (deserializeJson(doc, resp)) return false;
  JsonObject ap = doc["appearance"];
  if (!ap.isNull()) {
    String th = ap["theme"] | "dark";
    String ac = ap["accent"] | "00A8E8";
    uint32_t accent = strtoul(ac.c_str(), nullptr, 16);
    String bgs = ap["background"] | "";
    String lcs = ap["ledColor"] | "";
    ThemeUpdate u;
    u.light = th == "light";
    u.accent = accent;
    u.brightness = ap["brightness"] | 100;
    u.statusLed = ap["statusLed"] | true;
    u.background = bgs.length() == 6 ? (int32_t)strtoul(bgs.c_str(), nullptr, 16) : -1;
    String mode = ap["ledMode"] | "solid";
    u.ledMode = mode.c_str();
    u.ledColor = lcs.length() == 6 ? (int32_t)strtoul(lcs.c_str(), nullptr, 16) : -1;
    u.ledBrightness = ap["ledBrightness"] | 40;
    u.ledSpeed = ap["ledSpeed"] | 50;
    u.sleepAfter = ap["sleepAfter"] | 0;
    u.ledSleepOff = ap["ledSleepOff"] | true;
    if (theme::update(u))
      m_lookChanged = true;
    lockscreen::onHubBackgroundVersion(ap["lockBg"] | 0);
  }
  String name = doc["name"] | "";
  if (name.isEmpty() || name == m_gatewayName) return false;
  Serial.printf("[DM] Gateway renamed: '%s' -> '%s'\n", m_gatewayName.c_str(), name.c_str());
  m_gatewayName = name;
  auto& prefs = PreferencesManager::instance();
  prefs.setPairedGateway(prefs.getPairedGatewayId(), name);
  return true;
}

bool DataManager::fetchNodes() {
  if (m_gatewayId.isEmpty()) {
    if (!fetchGateways()) return false;
  }

  m_error = false;
  String path = "/api/hmi/gateways/" + m_gatewayId + "/all-nodes";
  String resp;
  int code = NetworkManager::instance().httpGet(path, resp);
  if (code != 200) {
    // Try resolving: maybe gateway ID is decimal, need hex
    if (m_gatewayId.length() > 10) {
      // Already looks like hex, fail
      m_error = true;
      m_lastError = httpErrorText(code);
      if (m_errorCb) m_errorCb();
      return false;
    }
    // Try looking up the gateway by saved name via HMI gateways list
    String hexId = findHexGatewayId(m_gatewayName);
    if (hexId.isEmpty()) {
      m_error = true;
      m_lastError = httpErrorText(code);
      if (m_errorCb) m_errorCb();
      return false;
    }
    m_gatewayId = hexId;
    path = "/api/hmi/gateways/" + m_gatewayId + "/all-nodes";
    code = NetworkManager::instance().httpGet(path, resp);
    if (code != 200) {
      // Try loading from cache before giving up
      String cached = PreferencesManager::instance().getCachedNodes();
      if (cached.isEmpty()) {
        m_error = true;
        m_lastError = httpErrorText(code);
        if (m_errorCb) m_errorCb();
        return false;
      }
      Serial.println("[DM] Hub offline, using cached nodes");
      if (!parseNodesResponse(cached)) {
        m_error = true;
        m_lastError = "Cache parse failed";
        if (m_errorCb) m_errorCb();
        return false;
      }
      m_initialized = true;
      if (m_nodesCb) m_nodesCb();
      return true;
    }
  }

  if (!parseNodesResponse(resp)) return false;

  m_initialized = true;
  if (m_nodesCb) m_nodesCb();

  // Cache successful response for offline use
  PreferencesManager::instance().cacheNodes(resp);
  return true;
}

bool DataManager::fetchNodeDetail(size_t index) {
  if (index >= m_nodes.size()) return false;
  NodeInfo& node = m_nodes[index];

  String path = "/api/hmi/nodes/" + node.deviceId;
  String resp;
  int code = NetworkManager::instance().httpGet(path, resp);
  if (code != 200) {
    // Don't set global error — detail fetch is non-critical
    return false;
  }

  if (!parseDetailResponse(resp, node)) return false;

  m_activeIndex = index;
  if (m_detailCb) m_detailCb();
  return true;
}

bool DataManager::sendCommand(size_t nodeIndex, int pin, int value) {
  if (nodeIndex >= m_nodes.size()) return false;
  const NodeInfo& node = m_nodes[nodeIndex];

  String body = "{\"pin\":" + String(pin) + ",\"value\":" + String(value) + "}";
  String path = "/api/hmi/nodes/" + node.deviceId + "/command";
  // Non-blocking: the hub sync happens on the network worker task.
  return NetworkManager::instance().postAsync(path, body);
}

const NodeInfo* DataManager::getNode(size_t index) const {
  if (index >= m_nodes.size()) return nullptr;
  return &m_nodes[index];
}

NodeInfo* DataManager::getNode(size_t index) {
  if (index >= m_nodes.size()) return nullptr;
  return &m_nodes[index];
}

void DataManager::setActiveIndex(size_t idx) { m_activeIndex = idx; }

bool DataManager::parseNodesResponse(const String& json) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    m_error = true;
    m_lastError = "Nodes JSON parse failed";
    if (m_errorCb) m_errorCb();
    return false;
  }

  m_nodes.clear();
  JsonArray arr = doc.as<JsonArray>();

  Serial.printf("[DM] Parsing %d nodes from all-nodes response\n", arr.size());

  for (JsonObject obj : arr) {
    NodeInfo node;
    node.deviceIdNum = obj["deviceId"].as<unsigned int>();
    node.deviceId = String(node.deviceIdNum);
    node.name = (obj["name"] | "");
    node.isOnline = obj["isOnline"] | false;
    node.isProvisioned = obj["isProvisioned"] | false;
    node.value = obj["value"] | 0;

    // Parse capabilities included in the all-nodes response
    JsonArray caps = obj["capabilitiesConfig"].as<JsonArray>();
    if (!caps.isNull()) {
      Serial.printf("[DM]   Node '%s' has %d capabilities:\n",
                    node.name.c_str(), caps.size());
      for (JsonObject cap : caps) {
        Capability c;
        c.name = (cap["label"] | "");
        if (c.name.isEmpty()) c.name = (cap["name"] | "");
        c.type = (cap["type"] | "");
        if (c.name.isEmpty()) c.name = c.type;  // unlabelled module: show its kind
        c.pin = cap["pin"] | -1;
        c.value = cap["value"] | 0;
        c.display = (cap["display"] | "");
        c.symbol = (cap["symbol"] | "");
        node.capabilities.push_back(c);
        Serial.printf("[DM]     Cap: name='%s' type='%s' pin=%d value=%d\n",
                      c.name.c_str(), c.type.c_str(), c.pin, c.value);
      }
    }

    m_nodes.push_back(node);
  }
  return true;
}

bool DataManager::parseDetailResponse(const String& json, NodeInfo& out) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    m_lastError = "Detail JSON parse failed";
    Serial.printf("[DM] Detail JSON parse error: %s\n", err.c_str());
    return false;
  }

  Serial.printf("[DM] Detail response: %s\n", json.substring(0, 400).c_str());

  JsonArray caps = doc["capabilitiesConfig"].as<JsonArray>();
  if (caps.isNull()) {
    // Fallback: try 'capabilities' for backward compatibility
    caps = doc["capabilities"].as<JsonArray>();
    if (caps.isNull()) {
      Serial.println("[DM] No capabilitiesConfig array in detail response");
      return false;
    }
  }

  Serial.printf("[DM] Parsed %d capabilities from detail\n", caps.size());

  for (JsonObject cap : caps) {
    String capName = (cap["label"] | "");  // Go uses 'label'
    if (capName.isEmpty()) capName = (cap["name"] | "");  // fallback
    String capType = (cap["type"] | "");
    if (capName.isEmpty()) capName = capType;
    int capPin = cap["pin"] | -1;
    int capValue = cap["value"] | 0;
    String capDisplay = (cap["display"] | "");
    String capSymbol = (cap["symbol"] | "");

    Serial.printf("[DM]   Cap: name='%s' type='%s' pin=%d value=%d\n",
                  capName.c_str(), capType.c_str(), capPin, capValue);

    bool found = false;
    for (auto& existing : out.capabilities) {
      if (existing.name == capName && existing.pin == capPin) {
        existing.value = capValue;
        existing.display = capDisplay;
        existing.symbol = capSymbol;
        existing.type = capType;
        found = true;
        break;
      }
    }

    if (!found) {
      Capability c;
      c.name = capName;
      c.type = capType;
      c.pin = capPin;
      c.value = capValue;
      c.display = capDisplay;
      c.symbol = capSymbol;
      out.capabilities.push_back(c);
    }
  }

  return true;
}
