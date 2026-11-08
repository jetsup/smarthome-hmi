#pragma once
#include <Arduino.h>
#include <lvgl.h>

// Lock screen: a full-screen layer over whatever the display is showing, with a clock
// (digital or analog), a background picture and a slide to unlock. Later this is where
// a PIN, and unlocking by a nearby trusted phone running the app, plug in (see authorize()).
namespace lockscreen {
struct Settings {
  bool enabled = true;
  uint16_t lockAfter = 60;   // seconds without touch before the display locks
  uint8_t clockStyle = 0;    // 0 digital, 1 analog
  uint8_t background = 0;    // index into the built-in pictures; 255 = plain colour
  int16_t tzMinutes = 180;   // local time = UTC + this
  bool hour24 = true;
};
const Settings& get();
void set(const Settings& s);  // stores them
void load();                  // at boot
void arm();                   // the dashboard is up: locking is allowed from now on
void tick();                  // from loop()
void lockNow();               // lock (even when auto-lock is off)
bool locked();
extern const uint8_t kNoBackground;
extern const uint8_t kCustomBackground;  // the picture uploaded from the app / website
bool customBackgroundExists();
// The hub tells (at every poll) which picture version it holds for this display: fetch it when it differs.
void onHubBackgroundVersion(int version);
}  // namespace lockscreen
