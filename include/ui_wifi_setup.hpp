#pragma once
#include <lvgl.h>

// Wi-Fi onboarding chooser: phone browser (hotspot + captive portal),
// SmartHome app (QR + Bluetooth) or typing on the HMI.
void build_wifi_setup_screen(bool fromSettings = false);
void cleanup_wifi_setup_screen();
