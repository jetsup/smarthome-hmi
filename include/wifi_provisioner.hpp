#pragma once
#include <Arduino.h>

// Wi-Fi onboarding for a board with no (working) credentials.
//
// Three ways in, all funnelled through the same "try, and only save on
// success" state machine so a typo never overwrites working credentials:
//   * Manual   – typed on the HMI touch screen
//   * Portal   – HMI hosts a hotspot + captive web page (phone browser)
//   * App/BLE  – the SmartHome app scans networks on the phone and sends the
//                chosen SSID/password over Bluetooth LE, so the phone never
//                has to leave its own network
//
// update() must be called from loop(); every network/BLE callback only posts
// work to it, so LVGL is never touched from another task.
class WifiProvisioner {
 public:
  enum class Mode { Off, Portal, Ble };
  enum class State { Idle, Connecting, Connected, Failed };

  static WifiProvisioner& instance();

  void update();

  // Start/stop the onboarding transports.
  void startPortal();
  void startBle();
  void stop();
  Mode mode() const { return m_mode; }

  // Try credentials. Saved to NVS only when the connection succeeds.
  void tryConnect(const String& ssid, const String& pass);
  State state() const { return m_state; }
  const String& lastError() const { return m_error; }
  const String& lastSsid() const { return m_trySsid; }
  // True once after a successful connection; the UI uses it to move on.
  bool takeConnected();

  // Strings for the on-screen QR codes / labels.
  String portalSsid() const { return m_apSsid; }
  String portalPass() const { return m_apPass; }
  String portalWifiQr() const;  // "WIFI:T:WPA;S:..;P:..;;"
  String bleName() const { return m_bleName; }
  String bleQr() const;  // "SHPROV1;n=<bleName>;k=<pop>"

  // Used by the web/BLE handlers.
  void requestFromRemote(const String& ssid, const String& pass);
  String statusText() const;

 private:
  WifiProvisioner() = default;

  void beginPortalServer();
  void handleScanRequest();
  void finishConnect(bool ok, const char* why);
  void notifyBle();

  Mode m_mode = Mode::Off;
  State m_state = State::Idle;
  String m_error;
  String m_trySsid, m_tryPass;
  uint32_t m_tryStart = 0;
  bool m_connectedFlag = false;
  uint32_t m_doneAt = 0;

  String m_apSsid, m_apPass, m_blePop, m_bleName;

  // Work posted by other tasks (BLE) to be picked up in update().
  volatile bool m_remotePending = false;
  String m_remoteSsid, m_remotePass;
  volatile bool m_bleNotifyPending = false;

  friend class ProvBleCallbacks;
};
