#pragma once

#include "Arduino_GFX_Library.h"
#include <Wire.h>

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
  bool touch_ok = false;

  LGFX() {}

  void pushImageDMA(int32_t x, int32_t y, int32_t w, int32_t h,
                    uint16_t* data) {
    gfx->draw16bitBeRGBBitmap(x, y, data, w, h);
  }

  // Reads the first touch point. CHSC6540 register map: 0x02 = touch count,
  // 0x03/0x04 = X high(4 bits)/low, 0x05/0x06 = Y high/low.
  bool getTouch(uint16_t* x, uint16_t* y) {
    if (!touch_ok) return false;
    uint8_t buf[7] = {0};
    Wire.beginTransmission(TOUCH_ADDR);
    Wire.write((uint8_t)0x00);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((uint8_t)TOUCH_ADDR, (uint8_t)7) != 7) return false;
    for (int i = 0; i < 7; i++) buf[i] = Wire.read();
    if ((buf[2] & 0x0F) == 0) return false;

    uint16_t raw_x = ((buf[3] & 0x0F) << 8) | buf[4];
    uint16_t raw_y = ((buf[5] & 0x0F) << 8) | buf[6];

    // Orientation transform for rotation 5 (swap axes, invert X).
    *x = SCREEN_WIDTH - raw_y;
    *y = raw_x;
    return true;
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

    // CHSC6540 capacitive touch (FT5x06-style register map) at 0x2E.
    pinMode(TOUCH_RST, OUTPUT);
    digitalWrite(TOUCH_RST, LOW);
    delay(20);
    digitalWrite(TOUCH_RST, HIGH);
    delay(300);
    Wire.begin(TOUCH_SDA, TOUCH_SCL, 400000);
    Wire.beginTransmission(TOUCH_ADDR);
    touch_ok = (Wire.endTransmission() == 0);
    Serial.printf("[TOUCH] CHSC6540 at 0x%02X: %s\n", TOUCH_ADDR,
                  touch_ok ? "found" : "NOT FOUND");
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
