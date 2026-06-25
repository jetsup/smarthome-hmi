#include "ui_dashboard.hpp"

#include <WS2812FX.h>

#include <vector>

#include "data_manager.hpp"
#include "gateway_discovery.hpp"
#include "network_manager.hpp"
#include "ui_screens.hpp"

extern WS2812FX ws2812fx;

// UI layout references
static lv_obj_t* sidebar_panel;
static lv_obj_t* nodes_main_panel;
static lv_obj_t* empty_label;
static lv_obj_t* empty_spinner;
static lv_obj_t* placeholder_lbl;
static lv_timer_t* poll_timer = nullptr;
static bool dashboard_active = false;
static bool gateway_online = false;

struct NodeCapability {
  String name;
  String type;
  int pin;
  int value;
};

struct DeviceNode {
  String name;
  String deviceId;
  uint32_t deviceIdNum;
  std::vector<NodeCapability> capabilities;
};

// Track widgets per capability for in-place updates (no flicker)
struct CapWidget {
  lv_obj_t* control;  // switch, slider, bar, or LED dot
  int pin;
  String type;
};

static std::vector<DeviceNode> discovered_nodes;
static size_t active_node_index = 0;
static std::vector<lv_obj_t*> sidebar_buttons;
static std::vector<CapWidget> cap_widgets;
static size_t rendered_node =
    SIZE_MAX;  // which node's widgets are currently built

static bool is_node_online(size_t node_idx) {
  if (node_idx >= discovered_nodes.size()) return false;
  // If gateway/hub is unreachable, all nodes are offline
  if (!gateway_online) return false;
  return GatewayDiscovery::instance().isNodeOnline(
      discovered_nodes[node_idx].deviceIdNum);
}

static void render_node_sidebar();
static void populate_capability_controls(size_t node_idx);
static void update_capability_values(size_t node_idx);
static void node_select_cb(lv_event_t* e);
static void digital_out_toggle_cb(lv_event_t* e);
static void analog_out_slider_cb(lv_event_t* e);
static void sync_nodes_from_data_manager();

static void merge_telemetry() {
  auto& gw = GatewayDiscovery::instance();
  for (auto& node : discovered_nodes) {
    const NodeTelemetry* t = gw.getNodeTelemetry(node.deviceIdNum);
    if (t) {
      for (auto& cap : node.capabilities) {
        if (cap.type == "analogInput" || cap.type == "digitalInput") {
          cap.value = t->value;
        }
      }
    }
  }
}

static void sync_telemetry_devices() {
  auto& gw = GatewayDiscovery::instance();
  size_t n = gw.telemetryDeviceCount();
  if (n == 0) return;
  bool added = false;
  for (size_t i = 0; i < n; i++) {
    uint32_t devId;
    NodeTelemetry tel;
    if (!gw.getTelemetryDeviceByIndex(i, devId, tel)) continue;
    bool found = false;
    for (auto& dn : discovered_nodes) {
      if (dn.deviceIdNum == devId) {
        found = true;
        break;
      }
    }
    if (found) continue;
    DeviceNode dn;
    dn.name = "Device " + String(devId);
    dn.deviceId = String(devId);
    dn.deviceIdNum = devId;
    dn.capabilities.push_back({"Status", "digitalInput", 0, tel.value});
    discovered_nodes.push_back(dn);
    added = true;
  }
  if (added) {
    Serial.printf("[DB] Added telemetry-only devices, total %zu\n",
                  discovered_nodes.size());
  }
}

static void refresh_ui() {
  if (!dashboard_active || !sidebar_panel || !nodes_main_panel) return;
  auto& dm = DataManager::instance();

  if (discovered_nodes.empty()) {
    lv_obj_clear_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(empty_spinner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(placeholder_lbl, LV_OBJ_FLAG_HIDDEN);
    if (dm.hasError()) {
      lv_label_set_text(empty_label, ("Error: " + dm.lastError()).c_str());
    } else if (dm.isInitialized()) {
      lv_label_set_text(empty_label,
                        "No nodes found.\nProvision nodes via web UI.");
    } else {
      lv_label_set_text(empty_label,
                        "No nodes discovered.\nScanning hardware...");
    }
  } else {
    lv_obj_add_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(empty_spinner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(placeholder_lbl, LV_OBJ_FLAG_HIDDEN);
  }

  render_node_sidebar();
  // Force full widget rebuild on refresh_ui (node list changed)
  rendered_node = SIZE_MAX;
  populate_capability_controls(active_node_index);
}

static void poll_data_cb(lv_timer_t* t) {
  if (!dashboard_active) return;

  auto& dm = DataManager::instance();

  // Check gateway/hub connectivity
  gateway_online =
      NetworkManager::instance().isWiFiConnected() && !dm.hasError();

  if (!dm.isInitialized()) {
    dm.onNodesChanged(sync_nodes_from_data_manager);
    if (!dm.fetchNodes()) {
      Serial.printf("[DB] poll: fetchNodes FAILED: %s\n",
                    dm.lastError().c_str());
    }
    return;
  }

  static unsigned long last_full_fetch = 0;
  unsigned long now = millis();

  if (now - last_full_fetch > 30000) {
    last_full_fetch = now;
    dm.onNodesChanged(sync_nodes_from_data_manager);
    dm.fetchNodes();
    return;
  }

  merge_telemetry();
  sync_telemetry_devices();

  if (active_node_index < discovered_nodes.size()) {
    bool detail_ok = dm.fetchNodeDetail(active_node_index);
    // If detail fetch fails, mark gateway as offline
    if (!detail_ok) {
      gateway_online = false;
    }
    const NodeInfo* apiNode = dm.getNode(active_node_index);
    if (apiNode) {
      auto& localNode = discovered_nodes[active_node_index];
      for (const auto& apiCap : apiNode->capabilities) {
        for (auto& localCap : localNode.capabilities) {
          if (localCap.pin == apiCap.pin && localCap.value != apiCap.value) {
            localCap.value = apiCap.value;
            break;
          }
        }
      }
    }
  }

  // Update widget values and disabled state in-place
  update_capability_values(active_node_index);
  // Also refresh sidebar for online status changes
  render_node_sidebar();
}

void build_main_dashboard_layout() {
  dashboard_active = true;

  // Turn off boot LED animation — dashboard is ready
  ws2812fx.setMode(FX_MODE_STATIC);
  ws2812fx.setColor(0x000000);
  ws2812fx.setBrightness(0);

  if (main_screen != nullptr) {
    lv_obj_delete(main_screen);
    main_screen = nullptr;
  }

  main_screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(main_screen, lv_color_hex(0x121214), LV_PART_MAIN);

  sidebar_panel = lv_obj_create(main_screen);
  lv_obj_set_size(sidebar_panel, 95, 200);
  lv_obj_align(sidebar_panel, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_flex_flow(sidebar_panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(sidebar_panel, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(sidebar_panel, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_color(sidebar_panel, lv_color_hex(0x1E1E24),
                            LV_PART_MAIN);
  lv_obj_set_style_border_width(sidebar_panel, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(sidebar_panel, lv_color_hex(0x2D2D35),
                                LV_PART_MAIN);
  lv_obj_set_style_pad_all(sidebar_panel, 4, LV_PART_MAIN);
  lv_obj_set_style_pad_row(sidebar_panel, 6, LV_PART_MAIN);

  lv_obj_t* settings_btn = lv_button_create(main_screen);
  lv_obj_set_size(settings_btn, 95, 36);
  lv_obj_align(settings_btn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_bg_color(settings_btn, lv_color_hex(0x2D2D35), LV_PART_MAIN);
  lv_obj_set_style_radius(settings_btn, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(settings_btn, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(settings_btn, lv_color_hex(0x2D2D35),
                                LV_PART_MAIN);
  lv_obj_t* gear_lbl = lv_label_create(settings_btn);
  lv_label_set_text(gear_lbl, LV_SYMBOL_SETTINGS " Settings");
  lv_obj_set_style_text_font(gear_lbl, &lv_font_montserrat_10, LV_PART_MAIN);
  lv_obj_set_style_text_color(gear_lbl, lv_color_hex(0x8A8A8A), LV_PART_MAIN);
  lv_obj_center(gear_lbl);
  lv_obj_add_event_cb(
      settings_btn,
      [](lv_event_t*) {
        dashboard_active = false;
        sidebar_panel = nullptr;
        nodes_main_panel = nullptr;
        empty_label = nullptr;
        empty_spinner = nullptr;
        placeholder_lbl = nullptr;
        cap_widgets.clear();
        rendered_node = SIZE_MAX;
        main_screen = nullptr;
        if (poll_timer != nullptr) {
          lv_timer_delete(poll_timer);
          poll_timer = nullptr;
        }
        build_settings_screen();
      },
      LV_EVENT_CLICKED, nullptr);

  nodes_main_panel = lv_obj_create(main_screen);
  lv_obj_set_size(nodes_main_panel, 225, 240);
  lv_obj_align(nodes_main_panel, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_set_flex_flow(nodes_main_panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(nodes_main_panel, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(nodes_main_panel, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_bg_color(nodes_main_panel, lv_color_hex(0x121214),
                            LV_PART_MAIN);
  lv_obj_set_style_border_width(nodes_main_panel, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(nodes_main_panel, 6, LV_PART_MAIN);
  lv_obj_set_style_pad_row(nodes_main_panel, 10, LV_PART_MAIN);

  empty_label = lv_label_create(main_screen);
  lv_label_set_text(empty_label, "No nodes discovered.\nScanning hardware...");
  lv_obj_set_style_text_color(empty_label, lv_color_hex(0x8A8A8A),
                              LV_PART_MAIN);
  lv_obj_set_style_text_font(empty_label, &lv_font_montserrat_10, LV_PART_MAIN);
  lv_obj_set_style_text_align(empty_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(empty_label, LV_ALIGN_CENTER, 10, -20);
  lv_obj_add_flag(empty_label, LV_OBJ_FLAG_HIDDEN);

  empty_spinner = lv_spinner_create(main_screen);
  lv_obj_set_size(empty_spinner, 30, 30);
  lv_obj_align(empty_spinner, LV_ALIGN_CENTER, 10, 20);
  lv_obj_set_style_arc_color(empty_spinner, lv_color_hex(0x00A8E8),
                             LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(empty_spinner, lv_color_hex(0x404040),
                             LV_PART_MAIN);
  lv_obj_set_style_arc_width(empty_spinner, 3, LV_PART_MAIN);
  lv_obj_set_style_arc_width(empty_spinner, 3, LV_PART_INDICATOR);
  lv_obj_add_flag(empty_spinner, LV_OBJ_FLAG_HIDDEN);

  placeholder_lbl = lv_label_create(main_screen);
  lv_label_set_text(placeholder_lbl, "Select a node\nfrom the sidebar");
  lv_obj_set_style_text_color(placeholder_lbl, lv_color_hex(0x656565),
                              LV_PART_MAIN);
  lv_obj_set_style_text_font(placeholder_lbl, &lv_font_montserrat_12,
                             LV_PART_MAIN);
  lv_obj_set_style_text_align(placeholder_lbl, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN);
  lv_obj_align(placeholder_lbl, LV_ALIGN_CENTER, 10, 0);
  lv_obj_add_flag(placeholder_lbl, LV_OBJ_FLAG_HIDDEN);

  cap_widgets.clear();
  rendered_node = SIZE_MAX;
  refresh_ui();

  if (poll_timer != nullptr) {
    lv_timer_delete(poll_timer);
  }
  poll_timer = lv_timer_create(poll_data_cb, 5000, nullptr);

  auto& dm = DataManager::instance();
  dm.onNodesChanged(sync_nodes_from_data_manager);
  if (dm.isInitialized()) {
    Serial.printf("[DB] DM already initialized, %zu nodes\n", dm.nodeCount());
    sync_nodes_from_data_manager();
  }
  Serial.printf("[DB] Fetching nodes...\n");
  if (!dm.fetchNodes()) {
    Serial.printf("[DB] fetchNodes failed: %s\n", dm.lastError().c_str());
    refresh_ui();
  } else {
    Serial.printf("[DB] fetchNodes OK, %zu nodes\n", dm.nodeCount());
  }
}

static void render_node_sidebar() {
  if (!dashboard_active || !sidebar_panel) return;
  lv_obj_clean(sidebar_panel);
  sidebar_buttons.clear();

  if (discovered_nodes.empty()) return;

  for (size_t i = 0; i < discovered_nodes.size(); ++i) {
    auto& node = discovered_nodes[i];
    bool online = is_node_online(i);

    lv_obj_t* btn = lv_button_create(sidebar_panel);
    lv_obj_set_width(btn, LV_PCT(100));
    lv_obj_set_height(btn, 48);
    lv_obj_set_style_radius(btn, 4, LV_PART_MAIN);
    lv_obj_add_event_cb(btn, node_select_cb, LV_EVENT_CLICKED, (void*)i);

    if (i == active_node_index) {
      lv_obj_add_state(btn, LV_STATE_CHECKED);
      lv_obj_set_style_bg_color(btn, lv_color_hex(0x00A8E8), LV_PART_MAIN);
    } else {
      lv_obj_set_style_bg_color(btn, lv_color_hex(0x2D2D35), LV_PART_MAIN);
    }

    lv_obj_t* name_lbl = lv_label_create(btn);
    String display_name = node.name.substring(0, 10);
    lv_label_set_text(name_lbl, display_name.c_str());
    lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(name_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(name_lbl, LV_ALIGN_TOP_MID, 0, 4);

    lv_obj_t* status_lbl = lv_label_create(btn);
    lv_label_set_text(status_lbl, online ? "Online" : "Offline");
    lv_obj_set_style_text_font(status_lbl, &lv_font_montserrat_10,
                               LV_PART_MAIN);
    lv_obj_set_style_text_color(
        status_lbl, online ? lv_color_hex(0x00FF00) : lv_color_hex(0xF7D3CD),
        LV_PART_MAIN);
    lv_obj_align(status_lbl, LV_ALIGN_BOTTOM_MID, 0, -4);

    sidebar_buttons.push_back(btn);
  }
}

// Build full capability UI (only called on node switch or full refresh)
static void populate_capability_controls(size_t node_idx) {
  if (!dashboard_active || !nodes_main_panel) return;

  if (discovered_nodes.empty() || node_idx >= discovered_nodes.size()) {
    lv_obj_clean(nodes_main_panel);
    cap_widgets.clear();
    rendered_node = SIZE_MAX;
    lv_obj_clear_flag(placeholder_lbl, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  // If already rendered for this node, just update values in-place
  if (rendered_node == node_idx) {
    update_capability_values(node_idx);
    return;
  }

  lv_obj_clean(nodes_main_panel);
  cap_widgets.clear();
  lv_obj_add_flag(placeholder_lbl, LV_OBJ_FLAG_HIDDEN);

  DeviceNode& node = discovered_nodes[node_idx];
  bool online = is_node_online(node_idx);

  lv_obj_t* node_card = lv_obj_create(nodes_main_panel);
  lv_obj_set_width(node_card, LV_PCT(100));
  lv_obj_set_height(node_card, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(node_card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_color(node_card, lv_color_hex(0x1E1E24), LV_PART_MAIN);
  lv_obj_set_style_border_width(node_card, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(node_card, lv_color_hex(0x2D2D35),
                                LV_PART_MAIN);
  lv_obj_set_style_pad_all(node_card, 6, LV_PART_MAIN);
  lv_obj_set_style_pad_row(node_card, 8, LV_PART_MAIN);

  lv_obj_t* node_title = lv_label_create(node_card);
  String title_text = node.name + (online ? "" : " (Offline)");
  lv_label_set_text(node_title, title_text.c_str());
  lv_obj_set_style_text_font(node_title, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_set_style_text_color(node_title, lv_color_hex(0x00A8E8), LV_PART_MAIN);

  for (size_t ci = 0; ci < node.capabilities.size(); ci++) {
    const auto& cap = node.capabilities[ci];

    lv_obj_t* row = lv_obj_create(node_card);
    lv_obj_set_size(row, LV_PCT(100), 28);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_left(row, 4, LV_PART_MAIN);

    lv_obj_t* cap_lbl = lv_label_create(row);
    lv_label_set_text(cap_lbl, cap.name.c_str());
    lv_obj_set_style_text_font(cap_lbl, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(cap_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

    uintptr_t context_idx = (uintptr_t)ci;
    lv_obj_t* ctrl = nullptr;

    if (cap.type == "digitalOutput") {
      ctrl = lv_switch_create(row);
      lv_obj_set_size(ctrl, 34, 18);
      lv_obj_set_style_bg_color(ctrl, lv_color_hex(0x00A8E8),
                                LV_PART_INDICATOR | LV_STATE_CHECKED);
      if (cap.value > 0) lv_obj_add_state(ctrl, LV_STATE_CHECKED);
      if (!online) lv_obj_add_state(ctrl, LV_STATE_DISABLED);
      lv_obj_add_event_cb(ctrl, digital_out_toggle_cb, LV_EVENT_VALUE_CHANGED,
                          (void*)context_idx);
    } else if (cap.type == "analogOutput") {
      ctrl = lv_slider_create(row);
      lv_obj_set_size(ctrl, 80, 8);
      lv_slider_set_range(ctrl, 0, 255);
      lv_slider_set_value(ctrl, cap.value, LV_ANIM_OFF);
      lv_obj_set_style_bg_color(ctrl, lv_color_hex(0x00A8E8),
                                LV_PART_INDICATOR);
      if (!online) lv_obj_add_state(ctrl, LV_STATE_DISABLED);
      lv_obj_add_event_cb(ctrl, analog_out_slider_cb, LV_EVENT_VALUE_CHANGED,
                          (void*)context_idx);
    } else if (cap.type == "analogInput") {
      ctrl = lv_bar_create(row);
      lv_obj_set_size(ctrl, 60, 6);
      lv_bar_set_range(ctrl, 0, 255);
      lv_bar_set_value(ctrl, cap.value, LV_ANIM_OFF);
      lv_obj_set_style_bg_color(ctrl, lv_color_hex(0x404040), LV_PART_MAIN);
      lv_obj_set_style_bg_color(ctrl, lv_color_hex(0x8A8A8A),
                                LV_PART_INDICATOR);
    } else if (cap.type == "digitalInput") {
      ctrl = lv_obj_create(row);
      lv_obj_set_size(ctrl, 12, 12);
      lv_obj_set_style_radius(ctrl, LV_RADIUS_CIRCLE, LV_PART_MAIN);
      lv_obj_set_style_border_width(ctrl, 0, LV_PART_MAIN);
      lv_obj_set_style_bg_color(
          ctrl, cap.value > 0 ? lv_color_hex(0x00A8E8) : lv_color_hex(0x404040),
          LV_PART_MAIN);
    }

    cap_widgets.push_back({ctrl, cap.pin, cap.type});
  }
  rendered_node = node_idx;
}

// Update capability widget values in-place (no destroy/recreate, no flicker)
static void update_capability_values(size_t node_idx) {
  if (!dashboard_active || !nodes_main_panel) return;
  if (node_idx >= discovered_nodes.size()) return;
  if (rendered_node != node_idx) {
    // Node changed — full rebuild needed
    populate_capability_controls(node_idx);
    return;
  }

  DeviceNode& node = discovered_nodes[node_idx];
  bool online = is_node_online(node_idx);
  // Update title with online status
  for (size_t i = 0; i < cap_widgets.size() && i < node.capabilities.size();
       i++) {
    auto& w = cap_widgets[i];
    auto& cap = node.capabilities[i];
    if (!w.control) continue;

    if (w.type == "digitalOutput") {
      // Update checked state
      if (cap.value > 0 && !lv_obj_has_state(w.control, LV_STATE_CHECKED)) {
        lv_obj_add_state(w.control, LV_STATE_CHECKED);
      } else if (cap.value == 0 &&
                 lv_obj_has_state(w.control, LV_STATE_CHECKED)) {
        lv_obj_clear_state(w.control, LV_STATE_CHECKED);
      }
      // Update disabled state
      if (online && lv_obj_has_state(w.control, LV_STATE_DISABLED)) {
        lv_obj_clear_state(w.control, LV_STATE_DISABLED);
      } else if (!online && !lv_obj_has_state(w.control, LV_STATE_DISABLED)) {
        lv_obj_add_state(w.control, LV_STATE_DISABLED);
      }
    } else if (w.type == "analogOutput") {
      int cur = lv_slider_get_value(w.control);
      if (cur != cap.value) {
        lv_slider_set_value(w.control, cap.value, LV_ANIM_OFF);
      }
      // Update disabled state
      if (online && lv_obj_has_state(w.control, LV_STATE_DISABLED)) {
        lv_obj_clear_state(w.control, LV_STATE_DISABLED);
      } else if (!online && !lv_obj_has_state(w.control, LV_STATE_DISABLED)) {
        lv_obj_add_state(w.control, LV_STATE_DISABLED);
      }
    } else if (w.type == "analogInput") {
      int cur = lv_bar_get_value(w.control);
      if (cur != cap.value) {
        lv_bar_set_value(w.control, cap.value, LV_ANIM_OFF);
      }
    } else if (w.type == "digitalInput") {
      lv_color_t c =
          cap.value > 0 ? lv_color_hex(0x00A8E8) : lv_color_hex(0x404040);
      lv_obj_set_style_bg_color(w.control, c, LV_PART_MAIN);
    }
  }
}

static void node_select_cb(lv_event_t* e) {
  size_t target_idx = (size_t)lv_event_get_user_data(e);
  if (target_idx != active_node_index) {
    active_node_index = target_idx;
    rendered_node = SIZE_MAX;  // force rebuild on next populate
    render_node_sidebar();
    populate_capability_controls(active_node_index);
  }
}

static void send_cmd_with_hub_sync(size_t nodeIdx, size_t capIdx, int value) {
  if (nodeIdx >= discovered_nodes.size()) return;
  // Don't send commands to offline nodes
  if (!is_node_online(nodeIdx)) return;
  auto& node = discovered_nodes[nodeIdx];
  if (capIdx >= node.capabilities.size()) return;
  auto& cap = node.capabilities[capIdx];
  cap.value = value;
  GatewayDiscovery::sendPinCommand(node.deviceIdNum, cap.pin, value);
  if (auto* dmNode = DataManager::instance().getNode(nodeIdx)) {
    if (capIdx < dmNode->capabilities.size()) {
      dmNode->capabilities[capIdx].value = value;
    }
    DataManager::instance().sendCommand(nodeIdx, cap.pin, value);
  }
}

static void digital_out_toggle_cb(lv_event_t* e) {
  lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
  bool is_on = lv_obj_has_state(sw, LV_STATE_CHECKED);
  size_t ci = (size_t)lv_event_get_user_data(e);
  send_cmd_with_hub_sync(active_node_index, ci, is_on ? 255 : 0);
}

static void analog_out_slider_cb(lv_event_t* e) {
  lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
  int value = lv_slider_get_value(slider);
  size_t ci = (size_t)lv_event_get_user_data(e);
  send_cmd_with_hub_sync(active_node_index, ci, value);
}

static void sync_nodes_from_data_manager() {
  auto& dm = DataManager::instance();
  size_t count = dm.nodeCount();
  discovered_nodes.clear();
  for (size_t i = 0; i < count; i++) {
    const NodeInfo* src = dm.getNode(i);
    if (!src) continue;
    DeviceNode dn;
    dn.name = src->name;
    dn.deviceId = src->deviceId;
    dn.deviceIdNum = src->deviceIdNum;
    for (const auto& cap : src->capabilities) {
      dn.capabilities.push_back({cap.name, cap.type, cap.pin, cap.value});
    }
    discovered_nodes.push_back(dn);
  }
  Serial.printf("[DB] Synced %zu nodes from DataManager\n",
                discovered_nodes.size());
  merge_telemetry();
  rendered_node = SIZE_MAX;  // force rebuild
  refresh_ui();
}
