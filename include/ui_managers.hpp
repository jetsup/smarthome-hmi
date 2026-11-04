#pragma once
#include <lvgl.h>

void trigger_logo_sequence();
void trigger_splash_sequence();
void trigger_wifi_settings();
// Called once Wi-Fi is up (any setup method): routes to dashboard or gateway scan.
void trigger_after_wifi_connected();
void trigger_gateway_scan();
void trigger_pairing_flow();
void trigger_main_dashboard_sequence();
