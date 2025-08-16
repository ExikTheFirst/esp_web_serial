//
// Created by micha on 20.07.2025.
//

#include "WiFiManager.h"
#include <vector>  // ensure std::vector is available

// Constructor: initialize prefs and server
WifiManager::WifiManager(const WiFiCredentials& preferred_,
                         unsigned long timeout_ms,
                         const char* ap_ssid,
                         const char* ap_pass)
  : prefs(),
    server(80),
    preferred(preferred_),
    maxConnectTime(timeout_ms),
    portalSsid(ap_ssid),
    portalPass(ap_pass) {
  prefs.begin("wifi", false);
}

// Load stored credentials from NVS into vector
void WifiManager::loadSaved() {
  saved.clear();
  uint16_t count = prefs.getUInt("count", 0);
  for (uint16_t i = 0; i < count; ++i) {
    // Build key strings and convert to C-string
    String keySsid  = String("ssid") + i;
    String keyPass  = String("pass") + i;
    String ss       = prefs.getString(keySsid.c_str(), "");
    String pw       = prefs.getString(keyPass.c_str(), "");
    if (ss.length()) {
      saved.push_back({ss, pw});
    }
  }
}

// Save new credential pair into NVS
void WifiManager::saveNew(const String& ssid, const String& pass) {
  uint16_t count = prefs.getUInt("count", 0);
  String keySsid = String("ssid") + count;
  String keyPass = String("pass") + count;
  prefs.putString(keySsid.c_str(), ssid);
  prefs.putString(keyPass.c_str(), pass);
  prefs.putUInt("count", count + 1);
}

// Attempt single connection with timeout
bool WifiManager::attempt(const char* ssid, const char* pass, unsigned long timeout) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  unsigned long start = millis();
  while (millis() - start < timeout) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("Connected to %s, IP: %s\n", ssid, WiFi.localIP().toString().c_str());
      return true;
    }
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nFailed to connect to %s\n", ssid);
  return false;
}

// Try preferred then saved credentials within maxConnectTime
bool WifiManager::connectWifi() {
  unsigned long overallStart = millis();
  // 1) Preferred (limit to 20s or maxConnectTime whichever is smaller)
  unsigned long firstTimeout = (maxConnectTime > 20000UL) ? 20000UL : maxConnectTime;
  if (attempt(preferred.ssid.c_str(), preferred.pass.c_str(), firstTimeout)) {
    return true;
  }
  // 2) Saved
  loadSaved();
  for (auto &c : saved) {
    unsigned long elapsed = millis() - overallStart;
    if (elapsed >= maxConnectTime) break;
    unsigned long remaining = maxConnectTime - elapsed;
    if (attempt(c.ssid.c_str(), c.pass.c_str(), remaining)) {
      return true;
    }
  }
  return false;
}

// Generate HTML form with nearby SSIDs
String WifiManager::portalHTML() {
  if (!MDNS.begin(portalSsid)) {
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  MDNS.addService("http", "tcp", 80);
  int n = WiFi.scanNetworks();
  String opts;
  for (int i = 0; i < n; ++i) {
    opts += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + " (" + WiFi.RSSI(i) + "dBm)</option>\n";
  }
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>WiFi Config</title></head><body>"
    "<h2>Select Network</h2><form action='/save' method='POST'>"
    "<label>SSID:</label><select name='ssid'>" + opts + "</select><br><br>"
    "<label>Password:</label><input type='password' name='pass'><br><br>"
    "<input type='submit' value='Save & Reboot'></form></body></html>";
  return html;
}

// Start AP and AsyncWebServer for config portal
void WifiManager::startPortal() {
  WiFi.mode(WIFI_AP);
  if (strlen(portalPass) > 0) WiFi.softAP(portalSsid, portalPass);
  else                   WiFi.softAP(portalSsid);
  Serial.printf("Config AP '%s' started\n", portalSsid);

  server.on("/", HTTP_GET, [&](AsyncWebServerRequest *req){
    req->send(200, "text/html", portalHTML());
  });

  server.on("/save", HTTP_POST, [&](AsyncWebServerRequest *req){
    if (req->hasParam("ssid", true) && req->hasParam("pass", true)) {
      String ss = req->getParam("ssid", true)->value();
      String pw = req->getParam("pass", true)->value();
      saveNew(ss, pw);
      req->send(200, "text/html", "<h2>Saved! Rebooting...</h2><script>setTimeout(()=>location.reload(),3000)</script>");
      delay(1000);
      ESP.restart();
    } else {
      req->send(400, "text/html", "<h2>Invalid Request</h2>");
    }
  });

  server.begin();
}

// Variant that mounts routes into an existing server (under /wifi)
String WifiManager::portalHTMLAtPath(const char* basePath) {
  // Same content as portalHTML but with form action pointing to basePath/save
  if (!MDNS.begin(portalSsid)) {
    Serial.println("Error setting up MDNS responder!");
  } else {
    Serial.println("mDNS responder started");
    MDNS.addService("http", "tcp", 80);
  }
  int n = WiFi.scanNetworks();
  String opts;
  for (int i = 0; i < n; ++i) {
    opts += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + " (" + WiFi.RSSI(i) + "dBm)</option>\n";
  }
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>WiFi Config</title></head><body>"
    "<h2>Select Network</h2><form action='" + String(basePath) + "/save' method='POST'>"
    "<label>SSID:</label><select name='ssid'>" + opts + "</select><br><br>"
    "<label>Password:</label><input type='password' name='pass'><br><br>"
    "<input type='submit' value='Save & Reboot'></form></body></html>";
  return html;
}

void WifiManager::attachPortalRoutes(AsyncWebServer& s) {
  WiFi.mode(WIFI_AP);
  if (strlen(portalPass) > 0) WiFi.softAP(portalSsid, portalPass);
  else                   WiFi.softAP(portalSsid);
  Serial.printf("Config AP '%s' started (attach)\n", portalSsid);

  s.on("/wifi", HTTP_GET, [&](AsyncWebServerRequest *req){
    req->send(200, "text/html", portalHTMLAtPath("/wifi"));
  });

  s.on("/wifi/save", HTTP_POST, [&](AsyncWebServerRequest *req){
    if (req->hasParam("ssid", true) && req->hasParam("pass", true)) {
      String ss = req->getParam("ssid", true)->value();
      String pw = req->getParam("pass", true)->value();
      saveNew(ss, pw);
      req->send(200, "text/html", "<h2>Saved! Rebooting...</h2><script>setTimeout(()=>location.reload(),3000)</script>");
      delay(1000);
      ESP.restart();
    } else {
      req->send(400, "text/html", "<h2>Invalid Request</h2>");
    }
  });
}
