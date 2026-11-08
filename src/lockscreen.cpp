#include "lockscreen.hpp"

#include "HWCDC.h"
extern HWCDC USBSerial;

#include <LittleFS.h>
#include <Preferences.h>
#include <math.h>

#include "assets.hpp"
#include "display.hpp"
#include "hmi_clock.hpp"
#include "network_manager.hpp"
#include "theme.hpp"
#include "ui_pairing.hpp"
#include "ui_screens.hpp"

namespace lockscreen {
const uint8_t kNoBackground = 255;
const uint8_t kCustomBackground = 254;
static const char* kBgPath = "/lockbg.bin";
static const char* kBgLvPath = "L:/lockbg.bin";

namespace {
Settings g_s;
bool g_armed = false;

lv_obj_t* g_lock = nullptr;
lv_timer_t* g_timer = nullptr;
lv_obj_t* g_time = nullptr;
lv_obj_t* g_date = nullptr;
lv_obj_t* g_hint = nullptr;
lv_obj_t* g_hour = nullptr;
lv_obj_t* g_min = nullptr;
lv_obj_t* g_sec = nullptr;
lv_point_precise_t g_tick[12][2];
lv_point_precise_t g_hourPts[2], g_minPts[2], g_secPts[2];

const int kFace = 134;   // analog clock diameter
const int kBorder = 2;   // its border: child coordinates start inside it, so the true centre is (kFace - 2*kBorder) / 2

uint32_t ink() { return g_s.background == kNoBackground ? th_text() : 0xFFFFFF; }
uint32_t inkSoft() { return g_s.background == kNoBackground ? th_muted() : 0xD6DCE8; }

// Unlocking is a slide today. A PIN, or a trusted phone running the app being nearby,
// will be checked here before the display opens.
bool authorize() { return true; }

void unlock() {
  if (!g_lock) return;
  if (!authorize()) return;
  if (g_timer) {
    lv_timer_delete(g_timer);
    g_timer = nullptr;
  }
  lv_obj_t* obj = g_lock;
  g_lock = g_time = g_date = g_hint = g_hour = g_min = g_sec = nullptr;
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_values(&a, 255, 0);
  lv_anim_set_duration(&a, 220);
  lv_anim_set_exec_cb(&a, [](void* o, int32_t v) { lv_obj_set_style_opa((lv_obj_t*)o, (lv_opa_t)v, 0); });
  lv_anim_set_completed_cb(&a, [](lv_anim_t* an) { lv_obj_delete((lv_obj_t*)an->var); });
  lv_anim_start(&a);
  lv_display_trigger_activity(nullptr);  // the idle countdown starts again
}

void slider_cb(lv_event_t* e) {
  lv_obj_t* s = (lv_obj_t*)lv_event_get_target(e);
  lv_event_code_t code = lv_event_get_code(e);
  int v = lv_slider_get_value(s);
#ifdef HMI_DEBUG
  if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESSED || code == LV_EVENT_VALUE_CHANGED || code == LV_EVENT_PRESS_LOST)
    USBSerial.printf("[lock] slider ev=%d v=%d\n", (int)code, v);
#endif
  if (code == LV_EVENT_VALUE_CHANGED) {
    if (g_hint) {
      int op = 255 - v * 255 / 55;
      lv_obj_set_style_text_opa(g_hint, op < 0 ? 0 : op, 0);
    }
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    if (v >= 90) {
      unlock();
    } else {
      lv_slider_set_value(s, 0, LV_ANIM_ON);
      if (g_hint) lv_obj_set_style_text_opa(g_hint, LV_OPA_COVER, 0);
    }
  }
}

const char* kDay[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char* kMonth[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

void hand(lv_point_precise_t* pts, float deg, float len, float back = 0) {
  float r = deg * 3.14159265f / 180.0f;
  float cx = (kFace - 2 * kBorder) / 2.0f, cy = cx;
  pts[0].x = (lv_value_precise_t)(cx - back * sinf(r));
  pts[0].y = (lv_value_precise_t)(cy + back * cosf(r));
  pts[1].x = (lv_value_precise_t)(cx + len * sinf(r));
  pts[1].y = (lv_value_precise_t)(cy - len * cosf(r));
}

void update_clock(lv_timer_t*) {
  if (!g_lock) return;
  struct tm t;
  bool ok = hmiclock::valid();
  if (ok) hmiclock::local(t);

  if (g_date) {
    char d[40];
    if (ok) {
      const char* ap = g_s.hour24 ? "" : (t.tm_hour >= 12 ? "PM  " : "AM  ");
      snprintf(d, sizeof(d), "%s%s, %d %s", ap, kDay[t.tm_wday], t.tm_mday, kMonth[t.tm_mon]);
    } else {
      snprintf(d, sizeof(d), "Waiting for the time...");
    }
    lv_label_set_text(g_date, d);
  }
  if (g_time) {
    char b[12];
    if (!ok) snprintf(b, sizeof(b), "--:--");
    else if (g_s.hour24) snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min);
    else snprintf(b, sizeof(b), "%d:%02d", (t.tm_hour + 11) % 12 + 1, t.tm_min);
    lv_label_set_text(g_time, b);
  }
  if (g_hour && g_min && g_sec) {
    float h = ok ? t.tm_hour % 12 + t.tm_min / 60.0f : 0, m = ok ? t.tm_min + t.tm_sec / 60.0f : 0, s = ok ? t.tm_sec : 0;
    hand(g_hourPts, h * 30.0f, kFace * 0.26f);
    hand(g_minPts, m * 6.0f, kFace * 0.38f);
    hand(g_secPts, s * 6.0f, kFace * 0.42f, 12);
    lv_line_set_points(g_hour, g_hourPts, 2);
    lv_line_set_points(g_min, g_minPts, 2);
    lv_line_set_points(g_sec, g_secPts, 2);
    lv_obj_set_style_opa(g_sec, ok ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
  }
}

lv_obj_t* make_line(lv_obj_t* parent, int width, uint32_t color) {
  lv_obj_t* l = lv_line_create(parent);
  lv_obj_set_style_line_width(l, width, 0);
  lv_obj_set_style_line_color(l, lv_color_hex(color), 0);
  lv_obj_set_style_line_rounded(l, true, 0);
  return l;
}

void build_analog() {
  lv_obj_t* face = lv_obj_create(g_lock);
  lv_obj_remove_flag(face, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  lv_obj_set_size(face, kFace, kFace);
  lv_obj_align(face, LV_ALIGN_TOP_MID, 0, 8);
  lv_obj_set_style_radius(face, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_pad_all(face, 0, 0);
  bool plain = g_s.background == kNoBackground;
  lv_obj_set_style_bg_color(face, lv_color_hex(plain ? th_panel() : 0x000000), 0);
  lv_obj_set_style_bg_opa(face, plain ? LV_OPA_COVER : LV_OPA_40, 0);
  lv_obj_set_style_border_width(face, kBorder, 0);
  lv_obj_set_style_border_color(face, lv_color_hex(ink()), 0);
  lv_obj_set_style_border_opa(face, LV_OPA_50, 0);

  float c = (kFace - 2 * kBorder) / 2.0f;
  for (int i = 0; i < 12; i++) {
    float a = i * 30.0f * 3.14159265f / 180.0f;
    bool major = i % 3 == 0;
    float r1 = c - 6, r2 = c - (major ? 16 : 11);
    g_tick[i][0].x = (lv_value_precise_t)(c + r1 * sinf(a));
    g_tick[i][0].y = (lv_value_precise_t)(c - r1 * cosf(a));
    g_tick[i][1].x = (lv_value_precise_t)(c + r2 * sinf(a));
    g_tick[i][1].y = (lv_value_precise_t)(c - r2 * cosf(a));
    lv_obj_t* tk = make_line(face, major ? 3 : 2, major ? ink() : inkSoft());
    lv_line_set_points(tk, g_tick[i], 2);
  }
  g_hour = make_line(face, 5, ink());
  g_min = make_line(face, 4, ink());
  g_sec = make_line(face, 2, th_accent());
  lv_obj_t* dot = lv_obj_create(face);
  lv_obj_remove_flag(dot, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  lv_obj_set_size(dot, 10, 10);
  lv_obj_align(dot, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dot, lv_color_hex(th_accent()), 0);
  lv_obj_set_style_border_width(dot, 0, 0);

  g_date = lv_label_create(g_lock);
  lv_obj_set_style_text_font(g_date, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(g_date, lv_color_hex(ink()), 0);
  lv_obj_align(g_date, LV_ALIGN_TOP_MID, 0, 8 + kFace + 6);
}

void build_digital() {
  g_time = lv_label_create(g_lock);
  lv_obj_set_style_text_font(g_time, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(g_time, lv_color_hex(ink()), 0);
  lv_obj_align(g_time, LV_ALIGN_TOP_MID, 0, 38);

  // a thin accent line under the time
  lv_obj_t* bar = lv_obj_create(g_lock);
  lv_obj_remove_flag(bar, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
  lv_obj_set_size(bar, 44, 3);
  lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 102);
  lv_obj_set_style_radius(bar, 2, 0);
  lv_obj_set_style_bg_color(bar, lv_color_hex(th_accent()), 0);
  lv_obj_set_style_border_width(bar, 0, 0);

  g_date = lv_label_create(g_lock);
  lv_obj_set_style_text_font(g_date, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(g_date, lv_color_hex(inkSoft()), 0);
  lv_obj_align(g_date, LV_ALIGN_TOP_MID, 0, 114);
}

void build_slider() {
  lv_obj_t* s = lv_slider_create(g_lock);
  lv_slider_set_range(s, 0, 100);
  lv_slider_set_value(s, 0, LV_ANIM_OFF);
  lv_obj_set_size(s, 250, 44);
  lv_obj_align(s, LV_ALIGN_BOTTOM_MID, 0, -14);
  lv_obj_set_style_radius(s, 22, LV_PART_MAIN);
  lv_obj_set_style_bg_color(s, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s, LV_OPA_20, LV_PART_MAIN);
  lv_obj_set_style_border_width(s, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(s, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_border_opa(s, LV_OPA_30, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s, LV_OPA_TRANSP, LV_PART_INDICATOR);  // just the knob and the fading hint
  lv_obj_set_style_bg_color(s, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_KNOB);
  lv_obj_set_style_radius(s, LV_RADIUS_CIRCLE, LV_PART_KNOB);
  lv_obj_set_style_pad_all(s, -4, LV_PART_KNOB);   // a little smaller than the track
  lv_obj_set_style_pad_hor(s, 22, LV_PART_MAIN);   // keeps the knob's travel inside the track ends
  lv_obj_set_style_bg_image_src(s, LV_SYMBOL_RIGHT, LV_PART_KNOB);
  lv_obj_set_style_bg_image_recolor(s, lv_color_hex(0x20242C), LV_PART_KNOB);
  lv_obj_set_style_bg_image_recolor_opa(s, LV_OPA_COVER, LV_PART_KNOB);
  lv_obj_add_event_cb(s, slider_cb, LV_EVENT_ALL, nullptr);

  g_hint = lv_label_create(g_lock);
  lv_label_set_text(g_hint, "Slide to unlock");
  lv_obj_set_style_text_font(g_hint, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(g_hint, lv_color_hex(0xFFFFFF), 0);
  lv_obj_remove_flag(g_hint, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_align(g_hint, LV_ALIGN_BOTTOM_MID, 14, -26);
}

void build() {
  g_lock = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(g_lock);
  lv_obj_set_size(g_lock, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_pos(g_lock, 0, 0);
  lv_obj_add_flag(g_lock, LV_OBJ_FLAG_CLICKABLE);  // swallow touches meant for the screen below
  lv_obj_remove_flag(g_lock, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(g_lock, lv_color_hex(th_page()), 0);
  lv_obj_set_style_bg_opa(g_lock, LV_OPA_COVER, 0);

  if (g_s.background == kCustomBackground && !LittleFS.exists(kBgPath)) g_s.background = 0;  // picture was removed
  if (g_s.background < lock_background_count || g_s.background == kCustomBackground) {
    lv_obj_t* img = lv_image_create(g_lock);
    if (g_s.background == kCustomBackground) lv_image_set_src(img, kBgLvPath);
    else lv_image_set_src(img, lock_backgrounds[g_s.background]);
    lv_obj_set_pos(img, 0, 0);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    // a little shade keeps the clock readable on any picture
    lv_obj_t* shade = lv_obj_create(g_lock);
    lv_obj_remove_style_all(shade);
    lv_obj_set_size(shade, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(shade, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(shade, LV_OPA_20, 0);
    lv_obj_remove_flag(shade, LV_OBJ_FLAG_CLICKABLE);
  } else {
    g_s.background = kNoBackground;
  }

  if (g_s.clockStyle == 1) build_analog();
  else build_digital();
  build_slider();

  g_timer = lv_timer_create(update_clock, 1000, nullptr);
  update_clock(nullptr);
}
}  // namespace

const Settings& get() { return g_s; }

void load() {
  Preferences p;
  p.begin("hmi-lock", true);
  g_s.enabled = p.getBool("on", true);
  g_s.lockAfter = p.getUShort("after", 60);
  g_s.clockStyle = p.getUChar("clock", 0);
  g_s.background = p.getUChar("bg", 0);
  g_s.tzMinutes = p.getShort("tz", 180);
  g_s.hour24 = p.getBool("h24", true);
  p.end();
}

void set(const Settings& s) {
  g_s = s;
  Preferences p;
  p.begin("hmi-lock", false);
  p.putBool("on", s.enabled);
  p.putUShort("after", s.lockAfter);
  p.putUChar("clock", s.clockStyle);
  p.putUChar("bg", s.background);
  p.putShort("tz", s.tzMinutes);
  p.putBool("h24", s.hour24);
  p.end();
}

bool customBackgroundExists() { return LittleFS.exists(kBgPath); }

namespace {
volatile bool g_fetching = false;
int g_fetchVersion = 0;

int storedBgVersion() {
  Preferences p;
  p.begin("hmi-lock", true);
  int v = p.getInt("bgv", 0);
  p.end();
  return v;
}
void storeBgVersion(int v) {
  Preferences p;
  p.begin("hmi-lock", false);
  p.putInt("bgv", v);
  p.end();
}

void fetchTask(void*) {
  const size_t kSize = 12 + 320 * 240 * 2;
  bool ok = NetworkManager::instance().httpGetToFile("/api/hmi/lockbg", "/lockbg.tmp", kSize);
  if (ok) {
    LittleFS.remove(kBgPath);
    LittleFS.rename("/lockbg.tmp", kBgPath);
    storeBgVersion(g_fetchVersion);
    Settings s = get();
    s.background = kCustomBackground;  // an uploaded picture is meant to be used
    set(s);
    Serial.printf("[lock] picture v%d downloaded\n", g_fetchVersion);
  } else {
    Serial.println("[lock] picture download failed, will retry");
  }
  g_fetching = false;
  vTaskDelete(nullptr);
}
}  // namespace

void onHubBackgroundVersion(int version) {
  int have = storedBgVersion();
  if (version == have || g_fetching) return;
  if (version == 0) {  // removed in the app / website
    LittleFS.remove(kBgPath);
    storeBgVersion(0);
    if (g_s.background == kCustomBackground) {
      Settings s = g_s;
      s.background = 0;
      set(s);
    }
    return;
  }
  g_fetching = true;
  g_fetchVersion = version;
  xTaskCreatePinnedToCore(fetchTask, "lockbg", 8192, nullptr, 1, nullptr, 0);
}

void arm() {
  g_armed = true;
  lv_display_trigger_activity(nullptr);  // arriving at the dashboard starts a fresh idle period
}

bool locked() { return g_lock != nullptr; }

void lockNow() {
  if (g_lock) return;
  build();
}

void tick() {
  if (g_lock || !g_armed || !g_s.enabled) return;
  if (splash_screen != nullptr || loading_dialog_visible()) return;
  bool idle = lv_display_get_inactive_time(nullptr) >= (uint32_t)g_s.lockAfter * 1000UL;
  if (idle || theme::asleep()) lockNow();
}
}  // namespace lockscreen
