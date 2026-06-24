#include "gateway_discovery.hpp"

#include <WiFi.h>
#include <esp_now.h>

#define MSG_TELEMETRY 1
#define MSG_GATEWAY_ANNOUNCE 7
#define MSG_PIN_CMD 6

struct __attribute__((packed)) EspNowPinCmd {
  uint8_t header;
  uint8_t msgType;
  uint32_t deviceId;
  uint8_t pin;
  uint8_t value;
  uint8_t checksum;
};

static const uint8_t s_broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

struct __attribute__((packed)) GatewayAnnouncePkt {
  uint8_t header;
  uint8_t msgType;
  uint32_t gatewayId;
  char name[8];
  uint8_t checksum;
};

GatewayDiscovery& GatewayDiscovery::instance() {
  static GatewayDiscovery inst;
  return inst;
}

bool GatewayDiscovery::begin() {
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("[GW] esp_now_init FAILED");
    return false;
  }
  esp_now_register_recv_cb(esp_now_recv_cb_t(recvCallback));
  ensureBroadcastPeer();
  Serial.println("[GW] ESP-NOW initialized, listening for announce...");
  return true;
}

void GatewayDiscovery::ensureBroadcastPeer() {
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, s_broadcastMac, 6);
  peer.channel = 0;
  peer.encrypt = false;
  peer.ifidx = WIFI_IF_STA;
  esp_now_add_peer(&peer);
}

bool GatewayDiscovery::sendPinCommand(uint32_t deviceId, uint8_t pin,
                                      uint8_t value) {
  EspNowPinCmd pkt;
  pkt.header = 0xAA;
  pkt.msgType = MSG_PIN_CMD;
  pkt.deviceId = deviceId;
  pkt.pin = pin;
  pkt.value = value;

  uint8_t calc = 0;
  const uint8_t* raw = reinterpret_cast<const uint8_t*>(&pkt);
  for (size_t i = 0; i < sizeof(pkt) - 1; i++) {
    calc ^= raw[i];
  }
  pkt.checksum = calc;

  esp_err_t err = esp_now_send(s_broadcastMac, raw, sizeof(pkt));
  if (err != ESP_OK) {
    Serial.printf("[GW] sendPinCommand failed: %d\n", err);
    return false;
  }
  Serial.printf("[GW] Pin cmd sent: deviceId=%u pin=%u value=%u\n", deviceId,
                pin, value);
  return true;
}

void GatewayDiscovery::startScan() {
  Serial.printf("[GW] Scan started (duration %lu ms)\n", SCAN_DURATION_MS);
  m_gateways.clear();
  m_scanning = true;
  m_scanStart = millis();
}

void GatewayDiscovery::stopScan() {
  Serial.printf("[GW] Scan stopped (%zu gateways found)\n", m_gateways.size());
  m_scanning = false;
}

const DiscoveredGateway* GatewayDiscovery::getGateway(size_t index) const {
  if (index >= m_gateways.size()) return nullptr;
  return &m_gateways[index];
}

int GatewayDiscovery::findGatewayByMac(const uint8_t* mac) {
  for (size_t i = 0; i < m_gateways.size(); i++) {
    if (memcmp(m_gateways[i].mac, mac, 6) == 0) return i;
  }
  return -1;
}

void GatewayDiscovery::update() {
  if (m_scanning && (millis() - m_scanStart > SCAN_DURATION_MS)) {
    m_scanning = false;
  }
}

void GatewayDiscovery::recvCallback(const uint8_t* mac, const uint8_t* data,
                                    int len) {
  instance().handleEspNowPacket(mac, data, len);
}

void GatewayDiscovery::handleEspNowPacket(const uint8_t* mac,
                                          const uint8_t* data, int len) {
  if (len < 1 || data[0] != 0xAA) return;

  uint8_t msgType = data[1];

  // Handle node telemetry (type 1) — 9-byte packet
  if (msgType == MSG_TELEMETRY) {
    if (len < 9) return;
    uint8_t calc = 0;
    for (int i = 0; i < 8; i++) calc ^= data[i];
    if (calc != data[8]) return;

    uint32_t deviceId = (uint32_t)data[2] | ((uint32_t)data[3] << 8) |
                        ((uint32_t)data[4] << 16) | ((uint32_t)data[5] << 24);
    uint16_t value = (uint16_t)data[6] | ((uint16_t)data[7] << 8);

    m_nodeTelemetry[deviceId] = {value, millis()};

    // Also discover unknown nodes so they show up even if unprovisioned
    if (m_nodeTelemetry.size() <= 64) {
      Serial.printf("[GW] Telemetry: deviceId=%u value=%u\n", deviceId, value);
    }
    return;
  }

  // Handle gateway announce (type 7)
  if (msgType != MSG_GATEWAY_ANNOUNCE) return;
  if (len < (int)sizeof(GatewayAnnouncePkt)) return;

  const GatewayAnnouncePkt* pkt =
      reinterpret_cast<const GatewayAnnouncePkt*>(data);

  uint8_t calc = 0;
  for (int i = 0; i < (int)sizeof(GatewayAnnouncePkt) - 1; i++) {
    calc ^= data[i];
  }
  if (calc != pkt->checksum) return;

  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0],
           mac[1], mac[2], mac[3], mac[4], mac[5]);

  int idx = findGatewayByMac(mac);
  if (idx < 0) {
    DiscoveredGateway gw;
    gw.id = String(pkt->gatewayId);
    gw.name = String(pkt->name);
    memcpy(gw.mac, mac, 6);
    gw.rssi = 0;
    gw.lastSeen = millis();
    m_gateways.push_back(gw);

    Serial.printf("[GW] New gateway: %s (%s) id=%s\n", gw.name.c_str(), macStr,
                  gw.id.c_str());

    if (m_gatewayCb) m_gatewayCb(gw);
  } else {
    m_gateways[idx].lastSeen = millis();
    m_gateways[idx].id = String(pkt->gatewayId);
    m_gateways[idx].name = String(pkt->name);
    Serial.printf("[GW] Updated gateway %s (%s)\n",
                  m_gateways[idx].name.c_str(), macStr);
  }
}

const NodeTelemetry* GatewayDiscovery::getNodeTelemetry(
    uint32_t deviceId) const {
  auto it = m_nodeTelemetry.find(deviceId);
  if (it == m_nodeTelemetry.end()) return nullptr;
  return &it->second;
}

bool GatewayDiscovery::isNodeOnline(uint32_t deviceId) const {
  auto* t = getNodeTelemetry(deviceId);
  if (!t) return false;
  return (millis() - t->lastSeen) < NODE_ONLINE_TIMEOUT_MS;
}

bool GatewayDiscovery::getTelemetryDeviceByIndex(size_t index,
                                                 uint32_t& deviceId,
                                                 NodeTelemetry& out) const {
  if (index >= m_nodeTelemetry.size()) return false;
  auto it = m_nodeTelemetry.begin();
  std::advance(it, index);
  deviceId = it->first;
  out = it->second;
  return true;
}
