#include "wifi_provisioner.hpp"

#include <ArduinoJson.h>
#include <DNSServer.h>
#include <NimBLEDevice.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_random.h>

#include "network_manager.hpp"
#include "preferences_manager.hpp"

namespace {
constexpr uint32_t kConnectTimeoutMs = 20000;
constexpr uint32_t kHandoverDelayMs = 2500;  // let the phone read the result

// Keep these in sync with smarthome-app/lib/services/hmi_provisioning_service.dart
constexpr const char* kSvcUuid = "4e5c0001-8d1f-4b6a-9c53-0a5b7a1e0001";
constexpr const char* kCredUuid = "4e5c0002-8d1f-4b6a-9c53-0a5b7a1e0001";
constexpr const char* kStatUuid = "4e5c0003-8d1f-4b6a-9c53-0a5b7a1e0001";

WebServer* g_server = nullptr;
DNSServer g_dns;
NimBLECharacteristic* g_statChar = nullptr;

String randomDigits(int n) {
  String s;
  for (int i = 0; i < n; i++) s += char('0' + (esp_random() % 10));
  return s;
}

String macTag() {
  char buf[8];
  snprintf(buf, sizeof(buf), "%04X", (uint16_t)(ESP.getEfuseMac() >> 32));
  return String(buf);
}

String wifiQrEscape(const String& in) {
  String out;
  for (char c : in) {
    if (c == '\\' || c == ';' || c == ',' || c == ':' || c == '"') out += '\\';
    out += c;
  }
  return out;
}

const char kPortalHtml[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>SmartHome HMI Wi-Fi</title>
<style>
:root{color-scheme:light dark;--bg:#f6f8fa;--card:#fff;--card2:#f3f4f6;--bd:#d0d7de;--tx:#1f2328;--mu:#59636e;--ac:#0969da;--bad:#cf222e;--ok:#1a7f37}
@media (prefers-color-scheme:dark){:root{--bg:#0d1117;--card:#161b22;--card2:#1c2128;--bd:#30363d;--tx:#c9d1d9;--mu:#8b949e;--ac:#58a6ff;--bad:#f85149;--ok:#3fb950}}

*{box-sizing:border-box}
body{margin:0;font-family:-apple-system,Segoe UI,Roboto,sans-serif;background:var(--bg);color:var(--tx);padding:1.2rem}
.c{max-width:420px;margin:auto}
h1{font-size:1.25rem;margin:.2rem 0 1rem;color:#fff}
p{color:var(--mu);font-size:.9rem;line-height:1.4}
.net{display:flex;justify-content:space-between;width:100%;text-align:left;background:var(--card);color:var(--tx);border:1px solid var(--bd);border-radius:8px;padding:.8rem;margin:.4rem 0;font-size:1rem}
.net.sel{border-color:var(--ac)}
.net small{color:var(--mu)}
input{width:100%;padding:.8rem;border-radius:8px;border:1px solid var(--bd);background:var(--bg);color:#fff;font-size:1rem;margin:.4rem 0}
button.go{width:100%;padding:.9rem;border:0;border-radius:8px;background:var(--ac);color:#fff;font-size:1rem;margin-top:.6rem}
button.go:disabled{opacity:.5}
.msg{margin-top:1rem;padding:.8rem;border-radius:8px;background:var(--card)}
.ok{color:var(--ok)}.bad{color:var(--bad)}
.link{background:none;border:0;color:var(--ac);font-size:.9rem;padding:0;margin-top:.5rem}
</style></head><body><div class="c">
<h1>Connect HMI to Wi-Fi</h1>
<p>Pick your home network. The HMI will test it before saving.</p>
<div id="list"><p>Scanning…</p></div>
<button class="link" onclick="scan(true)">Rescan</button>
<input id="ssid" placeholder="Network name (SSID)" autocapitalize="off" autocorrect="off">
<input id="pass" type="password" placeholder="Password (blank for open network)">
<button class="go" id="go" onclick="connect()">Connect</button>
<div class="msg" id="msg" hidden></div>
</div><script>
const $=i=>document.getElementById(i);
function show(t,c){const m=$('msg');m.hidden=false;m.className='msg '+(c||'');m.textContent=t}
async function scan(force){
  try{
    const r=await fetch('/scan'+(force?'?rescan=1':''));const d=await r.json();
    if(d.scanning&&!d.nets.length){setTimeout(scan,1500)}
    const l=$('list');l.innerHTML='';
    d.nets.forEach(n=>{
      const b=document.createElement('button');b.className='net';
      const a=document.createElement('span');a.textContent=n.ssid;
      const s=document.createElement('small');s.textContent=(n.open?'open · ':'')+n.rssi+' dBm';
      b.append(a,s);b.onclick=()=>{$('ssid').value=n.ssid;[...l.children].forEach(x=>x.classList.remove('sel'));b.classList.add('sel');$('pass').focus()};
      l.appendChild(b)});
    if(!d.nets.length&&!d.scanning)l.innerHTML='<p>No networks found.</p>';
  }catch(e){setTimeout(scan,2000)}
}
async function connect(){
  const ssid=$('ssid').value.trim();if(!ssid){show('Choose or type a network name','bad');return}
  $('go').disabled=true;show('Connecting… the HMI is testing these details.');
  const body=new URLSearchParams({ssid:ssid,pass:$('pass').value});
  try{await fetch('/connect',{method:'POST',body})}catch(e){}
  poll(0);
}
async function poll(n){
  try{
    const r=await fetch('/status');const d=await r.json();
    if(d.state==='connected'){show('Connected! You can close this page — the HMI will continue on its own.','ok');return}
    if(d.state==='failed'){show('Could not connect: '+d.error+'. Check the details and try again.','bad');$('go').disabled=false;return}
  }catch(e){ if(n>40){show('Lost contact with the HMI. If its screen shows it is connected you are done, otherwise rejoin its hotspot and retry.','bad');$('go').disabled=false;return} }
  setTimeout(()=>poll(n+1),1000);
}
scan();
</script></body></html>)HTML";

void sendJson(int code, JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  g_server->sendHeader("Cache-Control", "no-store");
  g_server->send(code, "application/json", out);
}
}  // namespace

// ── BLE ─────────────────────────────────────────────────────────────────────
class ProvBleCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c) override {
    auto& p = WifiProvisioner::instance();
    JsonDocument doc;
    std::string raw = c->getValue();
    if (deserializeJson(doc, raw) != DeserializationError::Ok) {
      p.m_error = "bad request";
      p.m_state = WifiProvisioner::State::Failed;
      p.m_bleNotifyPending = true;
      return;
    }
    String pop = doc["k"] | "";
    if (pop != p.m_blePop) {
      Serial.println("[PROV] BLE write rejected: wrong code");
      p.m_error = "wrong code";
      p.m_state = WifiProvisioner::State::Failed;
      p.m_bleNotifyPending = true;
      return;
    }
    p.m_remoteSsid = String(doc["s"] | "");
    p.m_remotePass = String(doc["p"] | "");
    p.m_remotePending = true;
  }
};

class ProvBleServerCallbacks : public NimBLEServerCallbacks {
  void onDisconnect(NimBLEServer*) override {
    // Keep advertising so the app can reconnect.
    if (WifiProvisioner::instance().mode() == WifiProvisioner::Mode::Ble)
      NimBLEDevice::getAdvertising()->start();
  }
};

WifiProvisioner& WifiProvisioner::instance() {
  static WifiProvisioner inst;
  return inst;
}

String WifiProvisioner::portalWifiQr() const {
  return "WIFI:T:WPA;S:" + wifiQrEscape(m_apSsid) + ";P:" + wifiQrEscape(m_apPass) + ";;";
}

String WifiProvisioner::bleQr() const {
  return "SHPROV1;n=" + m_bleName + ";k=" + m_blePop;
}

String WifiProvisioner::statusText() const {
  switch (m_state) {
    case State::Idle: return "idle";
    case State::Connecting: return "connecting";
    case State::Connected: return "ok:" + WiFi.localIP().toString();
    case State::Failed: return "fail:" + m_error;
  }
  return "idle";
}

void WifiProvisioner::notifyBle() {
  if (g_statChar) {
    g_statChar->setValue(std::string(statusText().c_str()));
    g_statChar->notify();
  }
}

void WifiProvisioner::startBle() {
  stop();
  NetworkManager::instance().stopAuto();
  m_mode = Mode::Ble;
  m_state = State::Idle;
  m_error = "";
  m_bleName = "SmartHome-HMI-" + macTag();
  m_blePop = randomDigits(6);

  WiFi.mode(WIFI_STA);
  NimBLEDevice::init(m_bleName.c_str());
  NimBLEServer* srv = NimBLEDevice::createServer();
  srv->setCallbacks(new ProvBleServerCallbacks());
  NimBLEService* svc = srv->createService(kSvcUuid);
  NimBLECharacteristic* cred =
      svc->createCharacteristic(kCredUuid, NIMBLE_PROPERTY::WRITE);
  cred->setCallbacks(new ProvBleCallbacks());
  g_statChar = svc->createCharacteristic(
      kStatUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  g_statChar->setValue(std::string("idle"));
  svc->start();
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(kSvcUuid);
  adv->setScanResponse(true);
  adv->start();
  Serial.printf("[PROV] BLE advertising as %s (code %s)\n", m_bleName.c_str(),
                m_blePop.c_str());
}

// ── Portal ──────────────────────────────────────────────────────────────────
void WifiProvisioner::startPortal() {
  stop();
  NetworkManager::instance().stopAuto();
  m_mode = Mode::Portal;
  m_state = State::Idle;
  m_error = "";
  m_apSsid = "SmartHome-HMI-" + macTag();
  m_apPass = randomDigits(8);

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(m_apSsid.c_str(), m_apPass.c_str());
  delay(100);
  g_dns.start(53, "*", WiFi.softAPIP());
  beginPortalServer();
  WiFi.scanNetworks(true);
  Serial.printf("[PROV] Portal up: %s / %s at %s\n", m_apSsid.c_str(),
                m_apPass.c_str(), WiFi.softAPIP().toString().c_str());
}

void WifiProvisioner::beginPortalServer() {
  delete g_server;
  g_server = new WebServer(80);
  g_server->on("/", HTTP_GET, [] { g_server->send_P(200, "text/html", kPortalHtml); });
  g_server->on("/scan", HTTP_GET, [] { WifiProvisioner::instance().handleScanRequest(); });
  g_server->on("/connect", HTTP_POST, [] {
    String ssid = g_server->arg("ssid");
    String pass = g_server->arg("pass");
    WifiProvisioner::instance().requestFromRemote(ssid, pass);
    JsonDocument d;
    d["ok"] = true;
    sendJson(200, d);
  });
  g_server->on("/status", HTTP_GET, [] {
    auto& p = WifiProvisioner::instance();
    JsonDocument d;
    switch (p.m_state) {
      case State::Idle: d["state"] = "idle"; break;
      case State::Connecting: d["state"] = "connecting"; break;
      case State::Connected: d["state"] = "connected"; break;
      case State::Failed: d["state"] = "failed"; break;
    }
    d["error"] = p.m_error;
    sendJson(200, d);
  });
  // Captive-portal probes (Android, iOS, Windows) and anything else.
  g_server->onNotFound([] {
    g_server->sendHeader("Location", "http://192.168.4.1/", true);
    g_server->send(302, "text/plain", "");
  });
  g_server->begin();
}

void WifiProvisioner::handleScanRequest() {
  if (g_server->hasArg("rescan") && WiFi.scanComplete() != WIFI_SCAN_RUNNING) {
    WiFi.scanDelete();
    WiFi.scanNetworks(true);
  }
  int n = WiFi.scanComplete();
  JsonDocument d;
  d["scanning"] = (n == WIFI_SCAN_RUNNING);
  JsonArray arr = d["nets"].to<JsonArray>();
  if (n == WIFI_SCAN_FAILED) {
    WiFi.scanNetworks(true);
  } else if (n > 0) {
    // Strongest first, skipping duplicates and the hidden/our own hotspot.
    bool used[64] = {false};
    if (n > 64) n = 64;
    for (int pass = 0; pass < n; pass++) {
      int best = -1;
      for (int i = 0; i < n; i++) {
        if (used[i]) continue;
        if (best < 0 || WiFi.RSSI(i) > WiFi.RSSI(best)) best = i;
      }
      if (best < 0) break;
      used[best] = true;
      String s = WiFi.SSID(best);
      if (s.isEmpty() || s == m_apSsid) continue;
      bool dup = false;
      for (JsonObject o : arr)
        if (s == o["ssid"].as<const char*>()) dup = true;
      if (dup) continue;
      JsonObject o = arr.add<JsonObject>();
      o["ssid"] = s;
      o["rssi"] = WiFi.RSSI(best);
      o["open"] = WiFi.encryptionType(best) == WIFI_AUTH_OPEN;
    }
  }
  sendJson(200, d);
}

void WifiProvisioner::stop() {
  if (m_mode == Mode::Portal) {
    if (g_server) {
      g_server->stop();
      delete g_server;
      g_server = nullptr;
    }
    g_dns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.scanDelete();
    if (WiFi.status() == WL_CONNECTED) {
      WiFi.mode(WIFI_STA);
    }
  } else if (m_mode == Mode::Ble) {
    g_statChar = nullptr;
    NimBLEDevice::deinit(true);
  }
  m_mode = Mode::Off;
}

// ── Connection state machine ────────────────────────────────────────────────
void WifiProvisioner::requestFromRemote(const String& ssid, const String& pass) {
  m_remoteSsid = ssid;
  m_remotePass = pass;
  m_remotePending = true;
}

void WifiProvisioner::tryConnect(const String& ssid, const String& pass) {
  NetworkManager::instance().stopAuto();
  m_trySsid = ssid;
  m_tryPass = pass;
  m_error = "";
  m_state = State::Connecting;
  m_tryStart = millis();
  m_bleNotifyPending = true;
  if (WiFi.getMode() == WIFI_OFF) WiFi.mode(WIFI_STA);
  WiFi.disconnect(false);
  delay(50);
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.printf("[PROV] Trying '%s'\n", ssid.c_str());
}

void WifiProvisioner::finishConnect(bool ok, const char* why) {
  m_bleNotifyPending = true;
  if (ok) {
    m_state = State::Connected;
    // The network the user just chose becomes the top priority.
    PreferencesManager::instance().saveWifiCredentials(m_trySsid, m_tryPass, true);
    NetworkManager::instance().adopt(m_trySsid);
    m_doneAt = millis();
    m_connectedFlag = true;
    Serial.printf("[PROV] Connected to '%s' (%s) — credentials saved\n",
                  m_trySsid.c_str(), WiFi.localIP().toString().c_str());
  } else {
    m_state = State::Failed;
    m_error = why;
    WiFi.disconnect(false);
    Serial.printf("[PROV] Failed: %s\n", why);
  }
}

bool WifiProvisioner::takeConnected() {
  if (!m_connectedFlag) return false;
  if (m_mode != Mode::Off && millis() - m_doneAt < kHandoverDelayMs) return false;
  m_connectedFlag = false;
  stop();
  return true;
}

void WifiProvisioner::update() {
  if (m_mode == Mode::Portal) {
    g_dns.processNextRequest();
    if (g_server) g_server->handleClient();
  }

  if (m_remotePending) {
    m_remotePending = false;
    if (m_state != State::Connecting) tryConnect(m_remoteSsid, m_remotePass);
  }

  if (m_state == State::Connecting) {
    wl_status_t st = WiFi.status();
    if (st == WL_CONNECTED) {
      finishConnect(true, "");
    } else if (st == WL_NO_SSID_AVAIL && millis() - m_tryStart > 6000) {
      finishConnect(false, "network not found");
    } else if (st == WL_CONNECT_FAILED) {
      finishConnect(false, "wrong password");
    } else if (millis() - m_tryStart > kConnectTimeoutMs) {
      finishConnect(false, "timed out - check the password");
    }
  }

  if (m_bleNotifyPending) {
    m_bleNotifyPending = false;
    if (m_mode == Mode::Ble) notifyBle();
  }
}
