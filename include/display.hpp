#pragma once

#include "Arduino_GFX_Library.h"
#include "TouchDrvCSTXXX.hpp"

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

#define LV_BUFFER_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT * 2 / 10)

#define LCD_DC 41
#define LCD_CS 42
#define LCD_SCK 40
#define LCD_MOSI 45
#define LCD_MISO -1
#define LCD_RST 39

#define LCD_BL 13
#define BL_CHANNEL 1

#define LCD_IM0 47
#define LCD_IM1 48

#define TOUCH_SDA 1
#define TOUCH_SCL 3
#define TOUCH_INT 4
#define TOUCH_RST 2

#define TOUCH_ADDR 0x2E

class LGFX {
 public:
  Arduino_DataBus* bus = new Arduino_ESP32SPIDMA(
      LCD_DC /* DC */, LCD_CS /* CS */, LCD_SCK /* SCK */, LCD_MOSI /* MOSI */,
      LCD_MISO /* MISO */, SPI2_HOST /* spi_num */);

  // Use native height and width
  Arduino_GFX* gfx =
      new Arduino_ST7789(bus, LCD_RST /* RST */, 5 /* rotation */,
                         false /* IPS */, 240 /* width */, 320 /* height */);
  TouchDrvCST816 touch;

  LGFX() {}

  void pushImageDMA(int32_t x, int32_t y, int32_t w, int32_t h,
                    uint16_t* data) {
    gfx->draw16bitBeRGBBitmap(x, y, data, w, h);
  }

  bool getTouch(uint16_t* x, uint16_t* y) {
    int16_t x_arr[5], y_arr[5];
    uint8_t touched =
        touch.getPoint(x_arr, y_arr, touch.getSupportTouchPoint());

    if (touched) {
      // For rotation 5, the raw touch hardware coordinates are usually swapped
      // We map the raw inputs to our 320x240 landscape plane
      uint16_t raw_x = x_arr[0];
      uint16_t raw_y = y_arr[0];

      // --- ORIENTATION TRANSFORM FOR ROTATION 5 ---
      // Swap X and Y, and invert the new X axis against the screen width
      *x = SCREEN_WIDTH - raw_y;
      *y = raw_x;

      // Optional: Print coordinates to Serial Monitor to debug alignment
      // Serial.printf("Touch mapped -> X: %d, Y: %d\n", *x, *y);
    }

    return touched;
  }

  void setBrightness(uint8_t brightness) { ledcWrite(BL_CHANNEL, brightness); }

  void init(void) {
    pinMode(LCD_IM0, OUTPUT);
    digitalWrite(LCD_IM0, LOW);
    pinMode(LCD_IM1, OUTPUT);
    digitalWrite(LCD_IM1, HIGH);

    ledcSetup(BL_CHANNEL, 12000, 8);
    ledcAttachPin(LCD_BL, BL_CHANNEL);

    gfx->begin();
    setRotation(5);

    touch.setPins(TOUCH_RST, TOUCH_INT);
    touch.begin(Wire, TOUCH_ADDR, TOUCH_SDA, TOUCH_SCL);
  }

  void setRotation(uint8_t r) {
    gfx->setRotation(r);
    switch (r) {
      case 1:
        r = ST7789_MADCTL_MX | ST7789_MADCTL_MV | ST7789_MADCTL_RGB;
        break;
      case 2:
        r = ST7789_MADCTL_MX | ST7789_MADCTL_MY | ST7789_MADCTL_RGB;
        break;
      case 3:
        r = ST7789_MADCTL_MY | ST7789_MADCTL_MV | ST7789_MADCTL_RGB;
        break;
      case 4:
        r = ST7789_MADCTL_MX | ST7789_MADCTL_RGB;
        break;
      case 5:
        r = ST7789_MADCTL_MX | ST7789_MADCTL_MY | ST7789_MADCTL_MV |
            ST7789_MADCTL_RGB;
        break;
      case 6:
        r = ST7789_MADCTL_MY | ST7789_MADCTL_RGB;
        break;
      case 7:
        r = ST7789_MADCTL_MV | ST7789_MADCTL_RGB;
        break;
      default:  // case 0:
        r = ST7789_MADCTL_RGB;
        break;
    }
    bus->beginWrite();
    bus->writeC8D8(ST7789_MADCTL, r | 1 << 3);
    bus->endWrite();
  }
};
