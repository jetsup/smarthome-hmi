#include <Arduino.h>
#include <WS2812FX.h>
#include <LittleFS.h>
#include <lvgl.h>

#include "HWCDC.h"
#include "config.hpp"
#include "display.hpp"
#include "gateway_discovery.hpp"
#include "hmi_clock.hpp"
#include "lockscreen.hpp"
#include "network_manager.hpp"
#include "preferences_manager.hpp"
#include "theme.hpp"
#include "ui_managers.hpp"
#include "wifi_provisioner.hpp"

#define LED_COUNT 1
#define LED_PIN 0

uint8_t lv_buffer[LV_BUFFER_SIZE];
LGFX tft;
WS2812FX ws2812fx = WS2812FX(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

#ifdef HMI_DEBUG
// Dev builds only: "shot" over the USB serial dumps the screen, "tap x y" / "down x y" / "up"
// inject touches. Lets the UI be checked without hands on the device.
static volatile uint32_t g_shotUntil = 0;
static bool g_fakeDown = false;
static uint16_t g_fakeX = 0, g_fakeY = 0;
static uint32_t g_fakeUpAt = 0;
#endif

void my_disp_flush(lv_display_t* display, const lv_area_t* area,
                   unsigned char* data) {
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);
#ifdef HMI_DEBUG
  if (g_shotUntil && millis() < g_shotUntil) {
    uint16_t hdr[4] = {(uint16_t)area->x1, (uint16_t)area->y1, (uint16_t)w, (uint16_t)h};
    USBSerial.write((const uint8_t*)"SHOT", 4);
    USBSerial.write((const uint8_t*)hdr, sizeof(hdr));
    USBSerial.write((const uint8_t*)data, w * h * 2);
  }
#endif
  tft.pushImageDMA(area->x1, area->y1, w, h, (uint16_t*)data);
  lv_display_flush_ready(display);
}

void my_touchpad_read(lv_indev_t* indev_driver, lv_indev_data_t* data) {
  uint16_t x, y;
  uint8_t touched = tft.getTouch(&x, &y);
#ifdef HMI_DEBUG
  if (g_fakeUpAt && millis() > g_fakeUpAt) { g_fakeDown = false; g_fakeUpAt = 0; }
  if (g_fakeDown) { touched = 1; x = g_fakeX; y = g_fakeY; }
#endif
  static uint32_t last_log = 0;
  if (touched && millis() - last_log > 300) {
    last_log = millis();
    Serial.printf("[TOUCH] x=%u y=%u\n", x, y);
  }
  static bool swallow = false;  // the touch that woke the screen is not a button press
  if (touched && theme::asleep()) {
    theme::wake();
    swallow = true;
  }
  if (!touched) swallow = false;
  if (!touched || swallow) {
    data->state = LV_INDEV_STATE_RELEASED;
  } else {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
  }
}

uint32_t my_tick(void) { return millis(); }

void setup() {
  Serial.begin(115200);
  USBSerial.begin(115200);
  USBSerial.println("Starting HMI");

  PreferencesManager::instance().init();

  // Older firmware paired without a hub token; the hub now requires one, so
  // drop the stale pairing and let the user pair again.
  {
    auto& prefs = PreferencesManager::instance();
    if (prefs.hasPairedGateway() && prefs.getHubToken().isEmpty()) {
      Serial.println("[Boot] paired gateway has no hub token — clearing pairing");
      prefs.clearPairedGateway();
    }
  }

  theme::loadSaved();
  lockscreen::load();
  tft.init();
  tft.setBrightness((uint16_t)theme::get().brightness * 255 / 100);
  tft.setRotation(5);

  LittleFS.begin(true);  // flash storage for the lock-screen picture (formats itself the first time)
  lv_init();
  lv_tick_set_cb(my_tick);

  lv_display_t* lv_display = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_display_set_color_format(lv_display, LV_COLOR_FORMAT_RGB565_SWAPPED);
  lv_display_set_flush_cb(lv_display, my_disp_flush);
  lv_display_set_buffers(lv_display, lv_buffer, NULL, LV_BUFFER_SIZE,
                         LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t* lv_input = lv_indev_create();
  lv_indev_set_type(lv_input, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(lv_input, my_touchpad_read);

  auto& net = NetworkManager::instance();
  net.setHubAddress(HMI_HUB_HOST, HMI_HUB_PORT);
  net.setAuthToken(PreferencesManager::instance().getHubToken());

  GatewayDiscovery::instance().begin();

  trigger_logo_sequence();

  ws2812fx.init();
  ws2812fx.setBrightness(160);
  ws2812fx.setSpeed(3000);
  ws2812fx.setMode(FX_MODE_RAINBOW_CYCLE);
  ws2812fx.start();
}

#ifdef HMI_DEBUG
static void debugPoll() {
  static String line;
  while (USBSerial.available()) {
    char c = USBSerial.read();
    if (c != '\n') { if (c != '\r' && line.length() < 40) line += c; continue; }
    int a = 0, b = 0;
    if (line == "shot") {
      lv_obj_invalidate(lv_screen_active());
      lv_obj_invalidate(lv_layer_top());
      g_shotUntil = millis() + 4000;
    } else if (sscanf(line.c_str(), "tap %d %d", &a, &b) == 2) {
      g_fakeX = a; g_fakeY = b; g_fakeDown = true; g_fakeUpAt = millis() + 150;
    } else if (sscanf(line.c_str(), "down %d %d", &a, &b) == 2) {
      g_fakeX = a; g_fakeY = b; g_fakeDown = true; g_fakeUpAt = 0;
    } else if (line == "up") {
      g_fakeDown = false;
    } else if (line == "wake") {
      theme::wake();
    }
    line = "";
  }
  if (g_shotUntil && millis() > g_shotUntil) {
    g_shotUntil = 0;
    USBSerial.write((const uint8_t*)"SEND", 4);
  }
}
#endif

void loop() {
#ifdef HMI_DEBUG
  debugPoll();
#endif
  NetworkManager::instance().update();
  WifiProvisioner::instance().update();
  GatewayDiscovery::instance().update();
  lv_timer_handler();
  theme::tickPower();
  hmiclock::tick();
  lockscreen::tick();
  delay(5);
  ws2812fx.service();
}
