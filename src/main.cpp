#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include "WifiManager.h"

// ===================== WiFi =====================
const char* WIFI_SSID = "EX_Network";
const char* WIFI_PASS = "q3gd-jwn5-orw4";

// Fallback AP, když se do 8 s nepřipojí k WiFi
const char* AP_SSID  = "ESP32-Serial-Bridge";
const char* AP_PASS  = "12345678";

// ===================== UART =====================
static const int RX_PIN = 20;   // ESP32-C3 U0RXD (připoj na TX zařízení)
static const int TX_PIN = 21;   // ESP32-C3 U0TXD (připoj na RX zařízení)
static uint32_t uartBaud = 115200;

// Pokud chceš RS-485 (DE/RE řízení), odkomentuj:
// #define USE_RS485
#ifdef USE_RS485
static const int RS485_EN_PIN = 21; // DE a /RE svázané dohromady
inline void rs485SetTx(bool en) { digitalWrite(RS485_EN_PIN, en ? HIGH : LOW); }
#endif

HardwareSerial SerialHW(0); // UART0 on ESP32-C3 (pins 20/21)

// ===================== Web =====================
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

String webIndex;

bool fetchCdnFile(const char* name, String& out) {
  HTTPClient http; http.setTimeout(10000);
  String url = String("https://cdn.exik.eu/projects/") + name;
  if (!http.begin(url)) return false;
  int code = http.GET();
  if (code == HTTP_CODE_OK) out = http.getString();
  http.end();
  return code == HTTP_CODE_OK;
}

void fetchWebUi() {
  fetchCdnFile("web-serial.html", webIndex);
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
#ifdef USE_RS485
      rs485SetTx(true);
#endif
      SerialHW.write(data, len);
      SerialHW.flush();
#ifdef USE_RS485
      rs485SetTx(false);
#endif
    }
  } else if (type == WS_EVT_CONNECT) {
    // po připojení pošli krátký banner
    client->text("=== Connected to ESP32 Serial Bridge ===\r\n");
  }
}

// Odesílání UART -> WebSocket
void pumpUartToClients() {
  static char buf[512];
  static size_t idx = 0;
  static uint32_t lastFlush = 0;

  while (SerialHW.available()) {
    if (idx < sizeof(buf)) {
      buf[idx++] = (char)SerialHW.read();
    } else {
      // buffer plný – flush
      buf[idx] = 0;
      ws.textAll(String(buf, idx));
      idx = 0;
    }
  }
  // periodicky flushni
  if (idx && (millis() - lastFlush > 30)) {
    buf[idx] = 0;
    ws.textAll(String(buf, idx));
    idx = 0;
    lastFlush = millis();
  }
  ws.cleanupClients();
}

// WiFi is now managed by WifiManager (lib/wifi-manager)

void setupMDNS() {
  // Start mDNS so the device is reachable at esp-web-serial.local
  if (!MDNS.begin("esp-web-serial")) {
    Serial.println("mDNS start failed");
    return;
  }
  MDNS.addService("http", "tcp", 80);
  Serial.println("mDNS: http://esp-web-serial.local");
}

void setup() {
  Serial.begin(115200);
  delay(200);

#ifdef USE_RS485
  pinMode(RS485_EN_PIN, OUTPUT);
  rs485SetTx(false); // receive mode by default
#endif

  SerialHW.begin(uartBaud, SERIAL_8N1, RX_PIN, TX_PIN);

  // Connect WiFi using WifiManager; if failed, attach config portal to our server under /wifi
  WiFiCredentials pref{WIFI_SSID, WIFI_PASS};
  WifiManager wm(pref, 30000 /*ms*/, AP_SSID, AP_PASS);

  bool connected = wm.connectWifi();
  if (connected) {
    setupMDNS();
  } else {
    wm.attachPortalRoutes(server);
    Serial.printf("AP SSID: %s  PASS: %s  IP: %s\n", AP_SSID, AP_PASS, WiFi.softAPIP().toString().c_str());
  }

  fetchWebUi();

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
    if (webIndex.length()) req->send(200, "text/html", webIndex);
    else req->send(500, "text/plain", "index.html missing");
  });

  // Info route: show WiFi portal link when not connected
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *req){
    String s = (WiFi.status() == WL_CONNECTED) ? "STA connected" : String("AP portal at http://") + WiFi.softAPIP().toString() + "/wifi";
    req->send(200, "text/plain", s);
  });

  server.on("/setbaud", HTTP_GET, [](AsyncWebServerRequest *req){
    if (req->hasParam("b")) {
      uartBaud = req->getParam("b")->value().toInt();
      if (uartBaud < 1200) uartBaud = 1200;
      SerialHW.updateBaudRate(uartBaud);
      Serial.printf("Baud set to %u\n", uartBaud);
    }
    req->send(200, "text/plain", "OK");
  });

  server.begin();
  Serial.println("HTTP/WebSocket server started");
}

void loop() {
  pumpUartToClients();
}
