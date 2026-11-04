#pragma once

#define HMI_WIFI_SSID "YOUR_SSID"
#define HMI_WIFI_PASSWORD "YOUR_PASSWORD"

// Override per deployment with -D build flags in platformio.ini
#ifndef HMI_HUB_HOST
#define HMI_HUB_HOST "192.168.100.100"
#endif
#ifndef HMI_HUB_PORT
#define HMI_HUB_PORT 9000
#endif

#define HMI_POLL_INTERVAL_MS 5000
