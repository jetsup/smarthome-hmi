#pragma once
#include <Arduino.h>
#include <functional>
#include <vector>

struct Capability {
    String name;
    String type;
    int pin;
    int value;
};

struct NodeInfo {
    String deviceId;   // string representation (for display/debug)
    uint32_t deviceIdNum = 0; // numeric for ESP-NOW addressing
    String name;
    bool isOnline = false;
    bool isProvisioned = false;
    int value = 0;
    std::vector<Capability> capabilities;
};

using DataCallback = std::function<void()>;

class DataManager {
public:
    static DataManager& instance();

    bool fetchGateways();
    String findHexGatewayId(const String& name);
    void setGatewayId(const String& id) { m_gatewayId = id; }
    void setGatewayName(const String& name) { m_gatewayName = name; }
    String gatewayId() const { return m_gatewayId; }
    bool fetchNodes();
    bool fetchNodeDetail(size_t index);
    bool sendCommand(size_t nodeIndex, int pin, int value);

    size_t nodeCount() const { return m_nodes.size(); }
    const NodeInfo* getNode(size_t index) const;
    NodeInfo* getNode(size_t index);
    size_t activeIndex() const { return m_activeIndex; }
    void setActiveIndex(size_t idx);
    bool isInitialized() const { return m_initialized; }
    bool hasError() const { return m_error; }
    String lastError() const { return m_lastError; }

    void onNodesChanged(DataCallback cb) { m_nodesCb = cb; }
    void onDetailChanged(DataCallback cb) { m_detailCb = cb; }
    void onError(DataCallback cb) { m_errorCb = cb; }

private:
    DataManager() = default;
    DataManager(const DataManager&) = delete;
    DataManager& operator=(const DataManager&) = delete;

    bool parseNodesResponse(const String& json);
    bool parseDetailResponse(const String& json, NodeInfo& out);

    std::vector<NodeInfo> m_nodes;
    size_t m_activeIndex = 0;
    bool m_initialized = false;
    bool m_error = false;
    String m_lastError;
    String m_gatewayId;
    String m_gatewayName;
    DataCallback m_nodesCb;
    DataCallback m_detailCb;
    DataCallback m_errorCb;
};
