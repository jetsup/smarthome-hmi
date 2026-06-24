#include <Arduino.h>
#include <WS2812FX.h>
#include <lvgl.h>

#include "HWCDC.h"
#include "config.hpp"
#include "display.hpp"
#include "gateway_discovery.hpp"
#include "network_manager.hpp"
#include "preferences_manager.hpp"
#include "ui_managers.hpp"

#define LED_COUNT 1
#define LED_PIN 0

uint8_t lv_buffer[LV_BUFFER_SIZE];
LGFX tft;
WS2812FX ws2812fx = WS2812FX(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

void my_disp_flush(lv_display_t* display, const lv_area_t* area,
                   unsigned char* data) {
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);
    tft.pushImageDMA(area->x1, area->y1, w, h, (uint16_t*)data);
    lv_display_flush_ready(display);
}

void my_touchpad_read(lv_indev_t* indev_driver, lv_indev_data_t* data) {
    uint16_t x, y;
    uint8_t touched = tft.getTouch(&x, &y);
    if (!touched) {
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

    tft.init();
    tft.setBrightness(255);
    tft.setRotation(5);

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

    GatewayDiscovery::instance().begin();

    trigger_logo_sequence();

    ws2812fx.init();
    ws2812fx.setBrightness(255);
    ws2812fx.setSpeed(1000);
    ws2812fx.setColor(0x000000);
    ws2812fx.setMode(FX_MODE_STATIC);
    ws2812fx.start();
}

void loop() {
    GatewayDiscovery::instance().update();
    lv_timer_handler();
    delay(5);
    ws2812fx.service();
}
