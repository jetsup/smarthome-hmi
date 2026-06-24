#pragma once
#include <Arduino.h>

#include <functional>
#include <map>
#include <vector>

struct DiscoveredGateway {
  String id;
  String name;
  uint8_t mac[6];
  int rssi;
  unsigned long lastSeen;
};

using GatewayCallback = std::function<void(const DiscoveredGateway&)>;

struct NodeTelemetry {
  uint16_t value;
  unsigned long lastSeen;
};

class GatewayDiscovery {
 public:
  static GatewayDiscovery& instance();

  bool begin();
  void startScan();
  void stopScan();
  bool isScanning() const { return m_scanning; }

  size_t gatewayCount() const { return m_gateways.size(); }
  const DiscoveredGateway* getGateway(size_t index) const;

  void onGatewayFound(GatewayCallback cb) { m_gatewayCb = cb; }
  void update();

  // Direct ESP-NOW pin command to nodes in the mesh
  static bool sendPinCommand(uint32_t deviceId, uint8_t pin, uint8_t value);
  static void ensureBroadcastPeer();

  // Node telemetry from ESP-NOW
  const NodeTelemetry* getNodeTelemetry(uint32_t deviceId) const;
  bool isNodeOnline(uint32_t deviceId) const;
  size_t telemetryDeviceCount() const { return m_nodeTelemetry.size(); }
  bool getTelemetryDeviceByIndex(size_t index, uint32_t& deviceId,
                                 NodeTelemetry& out) const;

  static constexpr unsigned long NODE_ONLINE_TIMEOUT_MS = 30000;

 private:
  GatewayDiscovery() = default;
  ~GatewayDiscovery() = default;
  GatewayDiscovery(const GatewayDiscovery&) = delete;
  GatewayDiscovery& operator=(const GatewayDiscovery&) = delete;

  int findGatewayByMac(const uint8_t* mac);
  void handleEspNowPacket(const uint8_t* mac, const uint8_t* data, int len);
  static void recvCallback(const uint8_t* mac, const uint8_t* data, int len);

  std::vector<DiscoveredGateway> m_gateways;
  bool m_scanning = false;
  unsigned long m_scanStart = 0;
  GatewayCallback m_gatewayCb;
  static constexpr unsigned long SCAN_DURATION_MS = 15000;

  // Node telemetry tracking
  std::map<uint32_t, NodeTelemetry> m_nodeTelemetry;
};
