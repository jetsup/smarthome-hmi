#include "theme.hpp"
#include "ui_dashboard.hpp"

#include <WS2812FX.h>

#include <vector>

#include "data_manager.hpp"
#include "gateway_discovery.hpp"
#include "assets.hpp"
#include "hmi_clock.hpp"
#include "lockscreen.hpp"
#include "network_manager.hpp"
#include "preferences_manager.hpp"
#include "ui_screens.hpp"

extern WS2812FX ws2812fx;

// UI layout references
static lv_obj_t* top_bar;
static lv_obj_t* top_title;
static lv_obj_t* top_time;
static lv_obj_t* top_dot;
static lv_obj_t* chips_panel;
static lv_obj_t* nodes_main_panel;  // the tile grid
static lv_obj_t* empty_label;
static lv_obj_t* empty_spinner;
static lv_timer_t* poll_timer = nullptr;
static lv_timer_t* clock_timer = nullptr;
static bool dashboard_active = false;
static bool gateway_online = false;

struct NodeCapability {
  String name;
  String type;
  int pin;
  int value;
  String display;
  String symbol;
};

struct DeviceNode {
  String name;
  String deviceId;
  uint32_t deviceIdNum;
  std::vector<NodeCapability> capabilities;
};

// One tile of the grid. A module can make one tile (a switch) or several (a DHT: temperature
// and humidity); tiles are updated in place so nothing flickers.
enum TileKind { T_SWITCH, T_SLIDER, T_GAUGE, T_BOOL, T_TEXT };
struct Tile {
  TileKind kind;
  int cap;        // index into the node's capabilities
  int part;       // which value of the module (0 / 1 for a DHT)
  int pin;
  String type;
  String label;
  const lv_image_dsc_t* icon;
  uint32_t tint;
  float vmin, vmax;
  lv_obj_t* root;
  lv_obj_t* badge;
  lv_obj_t* iconObj;
  lv_obj_t* value;   // big text
  lv_obj_t* ctrl;    // switch / slider / arc
};

static std::vector<DeviceNode> discovered_nodes;
static size_t active_node_index = 0;
static std::vector<lv_obj_t*> sidebar_buttons;
static std::vector<Tile> tiles;
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
static void leave_dashboard();
static void node_select_cb(lv_event_t* e);
static void digital_out_toggle_cb(lv_event_t* e);
static void analog_out_slider_cb(lv_event_t* e);
static void sync_nodes_from_data_manager();

static void merge_telemetry() {
  auto& gw = GatewayDiscovery::instance();
  for (auto& node : discovered_nodes) {
    const NodeTelemetry* t = gw.getNodeTelemetry(node.deviceIdNum);
    // The quick radio telemetry carries ONE value, so it is only right for a node with a
    // single input; nodes with several inputs get each pin's own value from the hub.
    int inputs = 0;
    for (auto& cap : node.capabilities)
      if (cap.type == "analogInput" || cap.type == "digitalInput") inputs++;
    // (a node with several modules reports one number for the lot: the hub's per-module values are right)
    if (t && inputs == 1 && node.capabilities.size() == 1) {
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
    dn.capabilities.push_back({"Status", "digitalInput", 0, tel.value, "", ""});
    discovered_nodes.push_back(dn);
    added = true;
  }
  if (added) {
    Serial.printf("[DB] Added telemetry-only devices, total %zu\n",
                  discovered_nodes.size());
  }
}

// ── look ────────────────────────────────────────────────────────────────────────────────
static uint32_t tint_for(const String& sym) {
  if (sym == "thermometer") return 0xF0883E;
  if (sym == "flame") return 0xF85149;
  if (sym == "droplet" || sym == "waves" || sym == "cloudrain") return 0x4AA3FF;
  if (sym == "radar" || sym == "eye") return 0xA371F7;
  if (sym == "gauge" || sym == "leaf") return 0x3FB950;
  if (sym == "sun" || sym == "lightbulb" || sym == "bell") return 0xE3B341;
  if (sym == "clock") return 0x58A6FF;
  return th_accent();
}

static void style_on(Tile& t, bool on, bool online) {
  if (!t.root) return;
  uint32_t panel = th_panel();
  lv_obj_set_style_bg_color(t.root, lv_color_hex(on ? th_mix(panel, t.tint, 16) : panel), LV_PART_MAIN);
  lv_obj_set_style_border_color(t.root, lv_color_hex(on ? th_mix(panel, t.tint, 55) : th_surface()), LV_PART_MAIN);
  lv_obj_set_style_bg_color(t.badge, lv_color_hex(on ? t.tint : th_mix(panel, t.tint, 22)), LV_PART_MAIN);
  lv_obj_set_style_image_recolor(t.iconObj, lv_color_hex(on ? 0xFFFFFF : t.tint), LV_PART_MAIN);
  lv_obj_set_style_opa(t.root, online ? LV_OPA_COVER : LV_OPA_50, LV_PART_MAIN);
}

// All the numbers in a hub status text: "25.9°C  60%" -> {25.9, 60}
static int numbers_in(const String& s, float* out, int max) {
  int n = 0;
  const char* p = s.c_str();
  while (*p && n < max) {
    if ((*p >= '0' && *p <= '9') || (*p == '-' && p[1] >= '0' && p[1] <= '9')) {
      char* end;
      out[n++] = strtof(p, &end);
      p = end;
    } else {
      p++;
    }
  }
  return n;
}

static void set_label(lv_obj_t* l, const char* t) {
  if (l && strcmp(lv_label_get_text(l), t) != 0) lv_label_set_text(l, t);
}

// Put a module's current value on its tile.
static void update_tile(Tile& t, const NodeCapability& cap, bool online) {
  char buf[24];
  bool on = false;
  switch (t.kind) {
    case T_SWITCH: {
      on = cap.value > 0;
      if (on != lv_obj_has_state(t.ctrl, LV_STATE_CHECKED)) {
        if (on) lv_obj_add_state(t.ctrl, LV_STATE_CHECKED); else lv_obj_clear_state(t.ctrl, LV_STATE_CHECKED);
      }
      set_label(t.value, on ? "On" : "Off");
      break;
    }
    case T_SLIDER: {
      on = cap.value > 0;
      if (lv_slider_get_value(t.ctrl) != cap.value) lv_slider_set_value(t.ctrl, cap.value, LV_ANIM_OFF);
      snprintf(buf, sizeof(buf), "%d%%", cap.value * 100 / 255);
      set_label(t.value, buf);
      break;
    }
    case T_GAUGE: {
      float v = 0;
      bool have = true;
      if (t.type == "dht") {
        float n[4];
        int c = numbers_in(cap.display, n, 4);
        have = c > t.part;
        if (have) v = n[t.part];
      } else {
        v = cap.value;
      }
      float frac = have && t.vmax > t.vmin ? (v - t.vmin) / (t.vmax - t.vmin) : 0;
      frac = frac < 0 ? 0 : (frac > 1 ? 1 : frac);
      lv_arc_set_value(t.ctrl, (int)(frac * 1000));
      if (!have) snprintf(buf, sizeof(buf), "--");
      else if (v == (int)v || fabsf(v) >= 100) snprintf(buf, sizeof(buf), "%d", (int)roundf(v));
      else snprintf(buf, sizeof(buf), "%.1f", v);
      set_label(t.value, buf);
      on = have;
      break;
    }
    case T_BOOL: {
      if (t.type == "ld2410") {
        on = !cap.display.isEmpty() && cap.display != "Clear";
        set_label(t.value, cap.display.isEmpty() ? "--" : cap.display.c_str());
      } else {
        on = cap.value > 0;
        set_label(t.value, on ? "Active" : "Inactive");
      }
      if (on != lv_obj_has_state(t.ctrl, LV_STATE_CHECKED)) {
        if (on) lv_obj_add_state(t.ctrl, LV_STATE_CHECKED); else lv_obj_clear_state(t.ctrl, LV_STATE_CHECKED);
      }
      break;
    }
    case T_TEXT: {
      on = t.type == "neopixel" ? (!cap.display.isEmpty() && cap.display != "Off") : false;
      set_label(t.value, cap.display.isEmpty() ? "--" : cap.display.c_str());
      break;
    }
  }
  if (t.ctrl && (t.kind == T_SWITCH || t.kind == T_SLIDER)) {
    if (online) lv_obj_clear_state(t.ctrl, LV_STATE_DISABLED);
    else lv_obj_add_state(t.ctrl, LV_STATE_DISABLED);
  }
  style_on(t, on, online);
}

static lv_obj_t* tile_label(lv_obj_t* parent, const char* text, const lv_font_t* font, uint32_t color, int x, int y, int w,
                            bool scroll = false) {
  lv_obj_t* l = lv_label_create(parent);
  lv_label_set_text(l, text);
  lv_label_set_long_mode(l, scroll ? LV_LABEL_LONG_SCROLL_CIRCULAR : LV_LABEL_LONG_DOT);
  lv_obj_set_width(l, w);
  lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
  lv_obj_set_style_text_color(l, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_pos(l, x, y);
  return l;
}

static void build_tile(const NodeCapability& cap, int capIdx, int part, TileKind kind, const String& label,
                       const String& symbol, float vmin, float vmax, bool online) {
  Tile t{};
  t.kind = kind;
  t.cap = capIdx;
  t.part = part;
  t.pin = cap.pin;
  t.type = cap.type;
  t.label = label;
  String sym = symbol.isEmpty() ? String(defaultSymbolFor(cap.type.c_str())) : symbol;
  t.icon = symbolImage(sym.c_str());
  t.tint = tint_for(sym);
  t.vmin = vmin;
  t.vmax = vmax;

  const int W = 148, H = 84;
  t.root = lv_obj_create(nodes_main_panel);
  lv_obj_set_size(t.root, W, H);
  lv_obj_remove_flag(t.root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(t.root, 16, LV_PART_MAIN);
  lv_obj_set_style_pad_all(t.root, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(t.root, 1, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(t.root, 0, LV_PART_MAIN);

  t.badge = lv_obj_create(t.root);
  lv_obj_remove_flag(t.badge, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  lv_obj_set_size(t.badge, 36, 36);
  lv_obj_set_pos(t.badge, 10, 8);
  lv_obj_set_style_radius(t.badge, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_border_width(t.badge, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(t.badge, 0, LV_PART_MAIN);
  t.iconObj = lv_image_create(t.badge);
  lv_image_set_src(t.iconObj, t.icon);
  lv_obj_set_style_image_recolor_opa(t.iconObj, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_center(t.iconObj);

  uintptr_t ctx = (uintptr_t)capIdx;
  switch (kind) {
    case T_SWITCH:
    case T_BOOL: {
      t.ctrl = lv_switch_create(t.root);
      lv_obj_set_size(t.ctrl, 38, 22);
      lv_obj_align(t.ctrl, LV_ALIGN_TOP_RIGHT, -10, 14);
      lv_obj_set_style_bg_color(t.ctrl, lv_color_hex(t.tint), (lv_style_selector_t)LV_PART_INDICATOR | LV_STATE_CHECKED);
      if (kind == T_SWITCH) {
        lv_obj_add_event_cb(t.ctrl, digital_out_toggle_cb, LV_EVENT_VALUE_CHANGED, (void*)ctx);
      } else {
        lv_obj_remove_flag(t.ctrl, LV_OBJ_FLAG_CLICKABLE);  // an indicator, not a control
      }
      t.value = tile_label(t.root, "--", &lv_font_montserrat_20, th_text(), 10, 44, 128);
      if (kind == T_BOOL) lv_obj_set_style_text_font(t.value, &lv_font_montserrat_14, LV_PART_MAIN);
      break;
    }
    case T_SLIDER: {
      t.value = tile_label(t.root, "--", &lv_font_montserrat_20, th_text(), 66, 12, 74);
      lv_obj_set_style_text_align(t.value, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
      t.ctrl = lv_slider_create(t.root);
      lv_slider_set_range(t.ctrl, 0, 255);
      lv_obj_set_size(t.ctrl, 126, 8);
      lv_obj_set_pos(t.ctrl, 11, 52);
      lv_obj_set_style_bg_color(t.ctrl, lv_color_hex(th_track()), LV_PART_MAIN);
      lv_obj_set_style_bg_color(t.ctrl, lv_color_hex(t.tint), LV_PART_INDICATOR);
      lv_obj_set_style_bg_color(t.ctrl, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
      lv_obj_set_style_pad_all(t.ctrl, 3, LV_PART_KNOB);
      lv_obj_add_event_cb(t.ctrl, analog_out_slider_cb, LV_EVENT_RELEASED, (void*)ctx);
      lv_obj_add_event_cb(t.ctrl, analog_out_slider_cb, LV_EVENT_VALUE_CHANGED, (void*)ctx);
      break;
    }
    case T_GAUGE: {
      t.ctrl = lv_arc_create(t.root);
      lv_obj_set_size(t.ctrl, 66, 66);
      lv_obj_set_pos(t.ctrl, 76, 9);
      lv_arc_set_rotation(t.ctrl, 135);
      lv_arc_set_bg_angles(t.ctrl, 0, 270);
      lv_arc_set_range(t.ctrl, 0, 1000);
      lv_obj_remove_flag(t.ctrl, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_arc_width(t.ctrl, 7, LV_PART_MAIN);
      lv_obj_set_style_arc_width(t.ctrl, 7, LV_PART_INDICATOR);
      lv_obj_set_style_arc_color(t.ctrl, lv_color_hex(th_track()), LV_PART_MAIN);
      lv_obj_set_style_arc_color(t.ctrl, lv_color_hex(t.tint), LV_PART_INDICATOR);
      lv_obj_set_style_arc_rounded(t.ctrl, true, LV_PART_INDICATOR);
      lv_obj_set_style_arc_rounded(t.ctrl, true, LV_PART_MAIN);
      lv_obj_set_style_opa(t.ctrl, LV_OPA_TRANSP, LV_PART_KNOB);
      t.value = lv_label_create(t.root);
      lv_label_set_text(t.value, "--");
      lv_obj_set_style_text_font(t.value, &lv_font_montserrat_16, LV_PART_MAIN);
      lv_obj_set_style_text_color(t.value, lv_color_hex(th_text()), LV_PART_MAIN);
      lv_obj_set_width(t.value, 50);  // fixed width + centred text: the number stays inside the ring
      lv_obj_set_style_text_align(t.value, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
      lv_obj_align_to(t.value, t.ctrl, LV_ALIGN_CENTER, 0, -3);
      lv_obj_t* unit = lv_label_create(t.root);
      lv_label_set_text(unit, cap.type == "dht" ? (part == 0 ? "°C" : "%") : (cap.type == "analogInput" ? "raw" : ""));
      lv_obj_set_style_text_font(unit, &lv_font_montserrat_10, LV_PART_MAIN);
      lv_obj_set_style_text_color(unit, lv_color_hex(th_muted()), LV_PART_MAIN);
      lv_obj_set_width(unit, 50);
      lv_obj_set_style_text_align(unit, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
      lv_obj_align_to(unit, t.ctrl, LV_ALIGN_CENTER, 0, 13);
      break;
    }
    case T_TEXT: {
      t.value = tile_label(t.root, "--", &lv_font_montserrat_12, th_text(), 10, 48, 128, true);
      break;
    }
  }
  if (kind == T_GAUGE) {
    // beside the ring: wrap onto two lines under the icon instead of being cut or scrolling
    lv_obj_t* lb = tile_label(t.root, label.c_str(), &lv_font_montserrat_10, th_muted(), 10, 48, 62);
    lv_obj_set_height(lb, 26);
  } else {
    tile_label(t.root, label.c_str(), &lv_font_montserrat_10, th_muted(), 10, 67, 128);
  }
  tiles.push_back(t);
  update_tile(tiles.back(), cap, online);
}

static void update_top_bar(lv_timer_t*) {
  if (!dashboard_active || !top_time) return;
  char b[8];
  if (hmiclock::valid()) {
    struct tm t;
    hmiclock::local(t);
    if (lockscreen::get().hour24) snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min);
    else snprintf(b, sizeof(b), "%d:%02d", (t.tm_hour + 11) % 12 + 1, t.tm_min);
  } else {
    snprintf(b, sizeof(b), "--:--");
  }
  set_label(top_time, b);
  lv_obj_set_style_bg_color(top_dot, lv_color_hex(gateway_online ? 0x3FB950 : th_dim()), LV_PART_MAIN);
}

static void refresh_ui() {
  if (!dashboard_active || !chips_panel || !nodes_main_panel) return;
  auto& dm = DataManager::instance();

  if (discovered_nodes.empty()) {
    lv_obj_clean(nodes_main_panel);
    tiles.clear();
    rendered_node = SIZE_MAX;
    lv_obj_clear_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
    bool waiting = true;  // show the spinner while we are still trying
    if (dm.hasError()) {
      String err = dm.lastError();
      Serial.printf("[UI] empty list: %s\n", err.c_str());
      if (err == "Hub offline") {
        lv_label_set_text(empty_label, "Hub offline\nCan't reach the hub.\nTrying again...");
      } else if (err == "No gateway present") {
        lv_label_set_text(empty_label, "No gateway present");
        waiting = false;
      } else {
        lv_label_set_text(empty_label, ("Error: " + err).c_str());
        waiting = false;
      }
    } else if (dm.isInitialized()) {
      lv_label_set_text(empty_label,
                        "No nodes on this display yet.\nAssign nodes to it in the app\nor on the website.");
      waiting = false;
    } else {
      lv_label_set_text(empty_label, "No nodes discovered.\nScanning hardware...");
    }
    if (waiting) lv_obj_clear_flag(empty_spinner, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(empty_spinner, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(empty_spinner, LV_OBJ_FLAG_HIDDEN);
    if (active_node_index >= discovered_nodes.size()) active_node_index = 0;
  }

  render_node_sidebar();
  // Force full widget rebuild on refresh_ui (node list changed)
  rendered_node = SIZE_MAX;
  populate_capability_controls(active_node_index);
}

// Rebuild the dashboard with the new theme/accent (swap screens safely).
static void rebuild_for_theme() {
  lv_obj_t* old = main_screen;
  main_screen = nullptr;  // build_main_dashboard_layout must not delete the live one
  build_main_dashboard_layout();
  lv_screen_load(main_screen);
  if (old && lv_obj_is_valid(old)) lv_obj_delete(old);
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
      if (dm.lastError() == "HTTP 401") {
        // Token rejected by the hub (revoked / secret changed): re-pair.
        Serial.println("[DB] hub rejected token — clearing pairing, restarting");
        PreferencesManager::instance().clearPairedGateway();
        delay(200);
        ESP.restart();
      }
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

  // Every poll (5 s): pick up a rename / appearance change made in the app.
  dm.refreshGatewayName();
  if (dm.takeLookChanged()) {
    rebuild_for_theme();
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
          if (localCap.pin == apiCap.pin && localCap.type == apiCap.type) {
            localCap.display = apiCap.display;
            if (localCap.value != apiCap.value) localCap.value = apiCap.value;
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

static void leave_dashboard() {
  dashboard_active = false;
  top_bar = top_title = top_time = top_dot = nullptr;
  chips_panel = nullptr;
  nodes_main_panel = nullptr;
  empty_label = nullptr;
  empty_spinner = nullptr;
  tiles.clear();
  sidebar_buttons.clear();
  rendered_node = SIZE_MAX;
  main_screen = nullptr;
  if (poll_timer != nullptr) {
    lv_timer_delete(poll_timer);
    poll_timer = nullptr;
  }
  if (clock_timer != nullptr) {
    lv_timer_delete(clock_timer);
    clock_timer = nullptr;
  }
}

static lv_obj_t* icon_button(lv_obj_t* parent, const char* symbol, lv_align_t al, int x) {
  lv_obj_t* b = lv_button_create(parent);
  lv_obj_set_size(b, 30, 26);
  lv_obj_align(b, al, x, 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(th_surface()), LV_PART_MAIN);
  lv_obj_set_style_radius(b, 13, LV_PART_MAIN);
  lv_obj_set_style_border_width(b, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
  lv_obj_t* im = lv_image_create(b);
  lv_image_set_src(im, symbolImage(symbol));
  lv_image_set_scale(im, 200);  // 28 px -> 22 px
  lv_obj_set_style_image_recolor(im, lv_color_hex(th_muted()), LV_PART_MAIN);
  lv_obj_set_style_image_recolor_opa(im, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_center(im);
  return b;
}

void build_main_dashboard_layout() {
  dashboard_active = true;

  // Turn off boot LED animation — dashboard is ready
  theme::applyHardware();  // backlight + status LED as chosen in the app
  lockscreen::arm();       // from now on the display may lock itself

  if (main_screen != nullptr) {
    lv_obj_delete(main_screen);
    main_screen = nullptr;
  }

  main_screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(main_screen, lv_color_hex(th_page()), LV_PART_MAIN);
  lv_obj_remove_flag(main_screen, LV_OBJ_FLAG_SCROLLABLE);

  // ── top bar: gateway, clock, lock, settings ──
  top_bar = lv_obj_create(main_screen);
  lv_obj_set_size(top_bar, SCREEN_WIDTH, 36);
  lv_obj_set_pos(top_bar, 0, 0);
  lv_obj_remove_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(top_bar, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(top_bar, lv_color_hex(th_panel()), LV_PART_MAIN);
  lv_obj_set_style_border_width(top_bar, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(top_bar, 0, LV_PART_MAIN);

  top_dot = lv_obj_create(top_bar);
  lv_obj_remove_flag(top_dot, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  lv_obj_set_size(top_dot, 8, 8);
  lv_obj_set_pos(top_dot, 12, 14);
  lv_obj_set_style_radius(top_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_border_width(top_dot, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(top_dot, lv_color_hex(th_dim()), LV_PART_MAIN);

  auto& prefsMgr = PreferencesManager::instance();
  top_title = lv_label_create(top_bar);
  lv_label_set_text(top_title, prefsMgr.hasPairedGateway() ? prefsMgr.getPairedGatewayName().c_str() : "Home");
  lv_label_set_long_mode(top_title, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_width(top_title, 120);
  lv_obj_set_style_text_font(top_title, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(top_title, lv_color_hex(th_text()), LV_PART_MAIN);
  lv_obj_set_pos(top_title, 28, 9);

  top_time = lv_label_create(top_bar);
  lv_label_set_text(top_time, "--:--");
  lv_obj_set_style_text_font(top_time, &lv_font_montserrat_16, LV_PART_MAIN);
  lv_obj_set_style_text_color(top_time, lv_color_hex(th_accent()), LV_PART_MAIN);
  lv_obj_align(top_time, LV_ALIGN_RIGHT_MID, -86, 0);

  lv_obj_t* lock_btn = icon_button(top_bar, "shield", LV_ALIGN_RIGHT_MID, -44);
  lv_obj_add_event_cb(lock_btn, [](lv_event_t*) { lockscreen::lockNow(); }, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* settings_btn = icon_button(top_bar, "settings", LV_ALIGN_RIGHT_MID, -8);
  lv_obj_add_event_cb(
      settings_btn,
      [](lv_event_t*) {
        leave_dashboard();
        build_settings_screen();
      },
      LV_EVENT_CLICKED, nullptr);

  // ── node chips ──
  chips_panel = lv_obj_create(main_screen);
  lv_obj_set_size(chips_panel, SCREEN_WIDTH, 34);
  lv_obj_set_pos(chips_panel, 0, 38);
  lv_obj_set_flex_flow(chips_panel, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(chips_panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_scroll_dir(chips_panel, LV_DIR_HOR);
  lv_obj_set_scrollbar_mode(chips_panel, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_opa(chips_panel, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(chips_panel, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(chips_panel, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_hor(chips_panel, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_ver(chips_panel, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_column(chips_panel, 6, LV_PART_MAIN);

  // ── tile grid ──
  nodes_main_panel = lv_obj_create(main_screen);
  lv_obj_set_size(nodes_main_panel, SCREEN_WIDTH, SCREEN_HEIGHT - 74);
  lv_obj_set_pos(nodes_main_panel, 0, 74);
  lv_obj_set_flex_flow(nodes_main_panel, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_scroll_dir(nodes_main_panel, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(nodes_main_panel, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_opa(nodes_main_panel, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(nodes_main_panel, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(nodes_main_panel, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_hor(nodes_main_panel, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_top(nodes_main_panel, 2, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(nodes_main_panel, 12, LV_PART_MAIN);
  lv_obj_set_style_pad_row(nodes_main_panel, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_column(nodes_main_panel, 8, LV_PART_MAIN);

  empty_label = lv_label_create(main_screen);
  lv_label_set_text(empty_label, "No nodes discovered.\nScanning hardware...");
  lv_obj_set_style_text_color(empty_label, lv_color_hex(th_muted()), LV_PART_MAIN);
  lv_obj_set_style_text_font(empty_label, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_set_style_text_align(empty_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(empty_label, LV_ALIGN_CENTER, 0, -10);
  lv_obj_add_flag(empty_label, LV_OBJ_FLAG_HIDDEN);

  empty_spinner = lv_spinner_create(main_screen);
  lv_obj_set_size(empty_spinner, 30, 30);
  lv_obj_align(empty_spinner, LV_ALIGN_CENTER, 0, 36);
  lv_obj_set_style_arc_color(empty_spinner, lv_color_hex(th_accent()), LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(empty_spinner, lv_color_hex(th_track()), LV_PART_MAIN);
  lv_obj_set_style_arc_width(empty_spinner, 3, LV_PART_MAIN);
  lv_obj_set_style_arc_width(empty_spinner, 3, LV_PART_INDICATOR);
  lv_obj_add_flag(empty_spinner, LV_OBJ_FLAG_HIDDEN);

  tiles.clear();
  rendered_node = SIZE_MAX;
  refresh_ui();

  if (poll_timer != nullptr) lv_timer_delete(poll_timer);
  poll_timer = lv_timer_create(poll_data_cb, 5000, nullptr);
  if (clock_timer != nullptr) lv_timer_delete(clock_timer);
  clock_timer = lv_timer_create(update_top_bar, 1000, nullptr);
  update_top_bar(nullptr);

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

// Node chips along the top: tap one to see its tiles.
static void render_node_sidebar() {
  if (!dashboard_active || !chips_panel) return;
  lv_obj_clean(chips_panel);
  sidebar_buttons.clear();
  if (discovered_nodes.empty()) return;

  for (size_t i = 0; i < discovered_nodes.size(); ++i) {
    auto& node = discovered_nodes[i];
    bool online = is_node_online(i);
    bool sel = i == active_node_index;

    lv_obj_t* btn = lv_button_create(chips_panel);
    lv_obj_set_height(btn, 26);
    lv_obj_set_width(btn, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(btn, 13, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(btn, 11, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_hex(sel ? th_accent() : th_panel()), LV_PART_MAIN);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btn, 6, LV_PART_MAIN);
    lv_obj_add_event_cb(btn, node_select_cb, LV_EVENT_CLICKED, (void*)i);

    lv_obj_t* dot = lv_obj_create(btn);
    lv_obj_remove_flag(dot, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    lv_obj_set_size(dot, 7, 7);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(dot, lv_color_hex(online ? 0x3FB950 : th_dim()), LV_PART_MAIN);

    lv_obj_t* name_lbl = lv_label_create(btn);
    lv_label_set_text(name_lbl, node.name.c_str());
    lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(name_lbl, lv_color_hex(sel ? 0xFFFFFF : th_text()), LV_PART_MAIN);

    sidebar_buttons.push_back(btn);
  }
}

// Build the tiles for a node (only on node switch or when the list changed).
static void populate_capability_controls(size_t node_idx) {
  if (!dashboard_active || !nodes_main_panel) return;

  if (discovered_nodes.empty() || node_idx >= discovered_nodes.size()) {
    lv_obj_clean(nodes_main_panel);
    tiles.clear();
    rendered_node = SIZE_MAX;
    return;
  }

  // If already rendered for this node, just update values in-place
  if (rendered_node == node_idx) {
    update_capability_values(node_idx);
    return;
  }

  lv_obj_clean(nodes_main_panel);
  lv_obj_scroll_to_y(nodes_main_panel, 0, LV_ANIM_OFF);
  tiles.clear();

  DeviceNode& node = discovered_nodes[node_idx];
  bool online = is_node_online(node_idx);

  // Only what can be watched or controlled gets a tile.
  for (size_t ci = 0; ci < node.capabilities.size(); ci++) {
    const auto& cap = node.capabilities[ci];
    const String& t = cap.type;
    if (t == "digitalOutput" || t == "relay") {
      build_tile(cap, ci, 0, T_SWITCH, cap.name, cap.symbol, 0, 1, online);
    } else if (t == "analogOutput") {
      build_tile(cap, ci, 0, T_SLIDER, cap.name, cap.symbol, 0, 255, online);
    } else if (t == "analogInput") {
      build_tile(cap, ci, 0, T_GAUGE, cap.name, cap.symbol, 0, 4095, online);
    } else if (t == "digitalInput" || t == "ld2410") {
      build_tile(cap, ci, 0, T_BOOL, cap.name, cap.symbol, 0, 1, online);
    } else if (t == "dht") {
      // "TempHum" + "Temperature" would only repeat itself: add the module's name only when it adds something
      String lname = cap.name;
      lname.toLowerCase();
      String base = (cap.name == "dht" || lname.indexOf("temp") >= 0 || lname.indexOf("hum") >= 0) ? String("") : cap.name + " ";
      build_tile(cap, ci, 0, T_GAUGE, base + "Temperature", cap.symbol.isEmpty() ? String("thermometer") : cap.symbol, -10, 50, online);
      build_tile(cap, ci, 1, T_GAUGE, base + "Humidity", "droplet", 0, 100, online);
    } else if (t == "ds3231" || t == "neopixel") {
      build_tile(cap, ci, 0, T_TEXT, cap.name, cap.symbol, 0, 1, online);
    }
  }
  if (tiles.empty()) {
    lv_obj_t* l = lv_label_create(nodes_main_panel);
    lv_label_set_text(l, "Nothing to monitor or\ncontrol on this node.");
    lv_obj_set_style_text_color(l, lv_color_hex(th_muted()), LV_PART_MAIN);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_12, LV_PART_MAIN);
  }
  rendered_node = node_idx;
}

// Update the tiles' values in place (no destroy/recreate, no flicker)
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
  for (auto& t : tiles) {
    if (t.cap < 0 || (size_t)t.cap >= node.capabilities.size()) continue;
    // don't fight the user's finger while a slider is being dragged
    if (t.kind == T_SLIDER && lv_obj_has_state(t.ctrl, LV_STATE_PRESSED)) continue;
    update_tile(t, node.capabilities[t.cap], online);
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

// Sends over ESP-NOW immediately (fast local path) and, when sync_hub is set,
// queues the hub update on the network worker so the UI never blocks.
static void send_cmd_with_hub_sync(size_t nodeIdx, size_t capIdx, int value,
                                   bool sync_hub = true) {
  if (nodeIdx >= discovered_nodes.size()) return;
  // Don't send commands to offline nodes
  if (!is_node_online(nodeIdx)) return;
  auto& node = discovered_nodes[nodeIdx];
  if (capIdx >= node.capabilities.size()) return;
  auto& cap = node.capabilities[capIdx];
  cap.value = value;
  GatewayDiscovery::sendPinCommand(node.deviceIdNum, cap.pin,
                                   value > 255 ? 255 : value);
  if (auto* dmNode = DataManager::instance().getNode(nodeIdx)) {
    if (capIdx < dmNode->capabilities.size()) {
      dmNode->capabilities[capIdx].value = value;
    }
    if (sync_hub) DataManager::instance().sendCommand(nodeIdx, cap.pin, value);
  }
}

static void digital_out_toggle_cb(lv_event_t* e) {
  lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
  bool is_on = lv_obj_has_state(sw, LV_STATE_CHECKED);
  size_t ci = (size_t)lv_event_get_user_data(e);
  send_cmd_with_hub_sync(active_node_index, ci, is_on ? 255 : 0);
  update_capability_values(active_node_index);  // show the new state now, not at the next poll
}

static void analog_out_slider_cb(lv_event_t* e) {
  lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
  int value = lv_slider_get_value(slider);
  size_t ci = (size_t)lv_event_get_user_data(e);
  if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
    // Final value: make sure the node and the hub both have it.
    send_cmd_with_hub_sync(active_node_index, ci, value, true);
    update_capability_values(active_node_index);
    return;
  }
  // While dragging, throttle the radio and skip the hub entirely.
  static uint32_t last_send = 0;
  uint32_t now = millis();
  if (now - last_send < 80) return;
  last_send = now;
  send_cmd_with_hub_sync(active_node_index, ci, value, false);
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
      dn.capabilities.push_back({cap.name, cap.type, cap.pin, cap.value, cap.display, cap.symbol});
    }
    discovered_nodes.push_back(dn);
  }
  Serial.printf("[DB] Synced %zu nodes from DataManager\n",
                discovered_nodes.size());
  merge_telemetry();
  rendered_node = SIZE_MAX;  // force rebuild
  refresh_ui();
}
