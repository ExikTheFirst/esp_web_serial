Jasně. Níže máš hotový „UART-to-Web“ pro ESP32: čte cizí sériovku (TTL 3.3 V) na **Serial2** a v reálném čase ji posílá do webového rozhraní přes WebSocket. Umí i posílat zpět (z webu → do UARTu) a měnit baudrate za běhu.

---

# 1) Knihovny a prostředí

* **ESP32 core pro Arduino** (běžně už máš).
* **AsyncTCP** a **ESPAsyncWebServer** (nainstaluj v Library Manageru).

    * Autor: *me-no-dev* (případně fork „ESPAsyncWebServer“ pro ESP32).

---

# 2) Zapojení (TTL 3.3 V)

Základ (pro čtení i zápis):

* **GND ↔ GND** (společná zem je nutnost)
* **ESP32 RX (GPIO16)** ↔ **TX zařízení**
* **ESP32 TX (GPIO17)** ↔ **RX zařízení** *(pokud chceš i posílat; pro čisté čtení TX klidně nechej volný)*

Poznámky:

* ESP32 používá pro programování **UART0 (GPIO1/3)** – ty **nepoužívej** pro externí linku.
* **Logické úrovně**: ESP32 = **3.3 V TTL**.

    * Pokud druhá strana je **5 V TTL** (např. Arduino UNO), **NEPŘIPOJUJ** 5 V na RX ESP32. Použij převod:

        * Jednoduchý dělič pro **UNO TX → ESP32 RX**: *1 kΩ (nahoře) + 2 kΩ (dole)* → \~3.3 V.
        * **ESP32 TX → UNO RX** většinou OK (3.3 V se bere jako HIGH).
* **RS-232 (DB9)** není TTL – potřebuješ **MAX3232** modul (3.3 V varianta).
* **RS-485** potřebuje transceiver (např. **MAX3485/75176**), zakončení **120 Ω** na koncích a ideálně pull-up/pull-down bias na A/B.

---

# 3) Kód (Arduino pro ESP32)

* Web UI běží na portu 80, real-time přes WebSocket (`/ws`).
* Přepínání baudrate: GET `/setbaud?b=115200`.
* Výběr běžných rychlostí je na stránce.
* Volitelná podpora RS-485 (DE/RE řízení) je zakomentovaná – stačí odkomentovat `USE_RS485`.

> Default piny: **RX2 = GPIO16**, **TX2 = GPIO17** (změň v `RX_PIN/TX_PIN` dle potřeby).

```cpp
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// ===================== WiFi =====================
const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PASS = "YOUR_PASSWORD";

// Fallback AP, když se do 8 s nepřipojí k WiFi
const char* AP_SSID  = "ESP32-Serial-Bridge";
const char* AP_PASS  = "12345678";

// ===================== UART =====================
static const int RX_PIN = 16;   // ESP32 RX2 (připoj na TX zařízení)
static const int TX_PIN = 17;   // ESP32 TX2 (připoj na RX zařízení)
static uint32_t uartBaud = 115200;

// Pokud chceš RS-485 (DE/RE řízení), odkomentuj:
// #define USE_RS485
#ifdef USE_RS485
static const int RS485_EN_PIN = 21; // DE a /RE svázané dohromady
inline void rs485SetTx(bool en) { digitalWrite(RS485_EN_PIN, en ? HIGH : LOW); }
#endif

HardwareSerial SerialHW(2); // UART2

// ===================== Web =====================
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

static const char index_html[] PROGMEM = R"html(
<!DOCTYPE html>
<html lang="cs">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>ESP32 Serial Bridge</title>
<style>
  body{font-family:system-ui,Segoe UI,Roboto,Arial,sans-serif;margin:0;padding:16px;background:#0b0b0b;color:#eaeaea}
  .row{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:8px}
  input,select,button{font:inherit;padding:8px;border-radius:10px;border:1px solid #333;background:#151515;color:#eee}
  button{cursor:pointer}
  #log{height:60vh;overflow:auto;background:#0f0f0f;border:1px solid #222;padding:8px;white-space:pre-wrap;font-family:ui-monospace,Consolas,monospace}
  .pill{padding:4px 10px;border-radius:999px;border:1px solid #333;background:#111}
  .ok{color:#5ad67d}.bad{color:#ff7070}
</style>
</head>
<body>
<h2>ESP32 Serial Bridge</h2>

<div class="row">
  <span>WS: <span id="wsState" class="pill bad">odpojeno</span></span>
  <span>Baud:</span>
  <select id="baud">
    <option>9600</option><option>19200</option><option>38400</option>
    <option>57600</option><option selected>115200</option><option>230400</option>
    <option>460800</option><option>921600</option>
  </select>
  <button id="applyBaud">Nastavit</button>
  <button id="clear">Vyčistit</button>
</div>

<div id="log"></div>

<div class="row" style="margin-top:8px;">
  <input id="tx" placeholder="text k odeslání…" style="flex:1;min-width:200px"/>
  <label class="pill"><input id="crlf" type="checkbox" checked> přidat CRLF</label>
  <button id="send">Odeslat</button>
</div>

<script>
  const log = document.getElementById('log');
  const wsState = document.getElementById('wsState');
  function append(s){
    log.textContent += s;
    if (log.textContent.length > 300000) log.textContent = log.textContent.slice(-150000);
    log.scrollTop = log.scrollHeight;
  }

  function connectWS(){
    const scheme = location.protocol === 'https:' ? 'wss://' : 'ws://';
    const ws = new WebSocket(scheme + location.host + '/ws');
    ws.onopen = () => { wsState.textContent='připojeno'; wsState.classList.remove('bad'); wsState.classList.add('ok'); };
    ws.onclose = () => { wsState.textContent='odpojeno'; wsState.classList.add('bad'); wsState.classList.remove('ok'); setTimeout(connectWS, 1000); };
    ws.onerror = () => { ws.close(); };
    ws.onmessage = (ev) => { append(ev.data); };
    window.__ws = ws;
  }
  connectWS();

  document.getElementById('send').onclick = () => {
    const ws = window.__ws;
    if (!ws || ws.readyState !== 1) return;
    let msg = document.getElementById('tx').value;
    if (document.getElementById('crlf').checked) msg += "\\r\\n";
    ws.send(msg);
  };
  document.getElementById('clear').onclick = () => { log.textContent = ''; };

  document.getElementById('applyBaud').onclick = async () => {
    const b = document.getElementById('baud').value;
    await fetch('/setbaud?b=' + encodeURIComponent(b));
  };
</script>
</body>
</html>
)html";

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

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("WiFi: connecting to %s ...\n", WIFI_SSID);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000) {
    delay(200);
    Serial.print('.');
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi OK, IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nWiFi STA failed → starting AP");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.printf("AP SSID: %s  PASS: %s  IP: %s\n", AP_SSID, AP_PASS, WiFi.softAPIP().toString().c_str());
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

#ifdef USE_RS485
  pinMode(RS485_EN_PIN, OUTPUT);
  rs485SetTx(false); // receive mode by default
#endif

  SerialHW.begin(uartBaud, SERIAL_8N1, RX_PIN, TX_PIN);

  setupWiFi();

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send_P(200, "text/html; charset=utf-8", index_html);
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
```

---

# 4) Jak se napojit na **další typy sérií**

### a) 3.3 V / 5 V **TTL (UART)**

* **GND ↔ GND** (vždy)
* **TX zařízení → RX ESP32**, **RX zařízení ← TX ESP32**
* **5 V TTL**: dej **dělič** na cestu **TX→RX ESP32** (např. 1 kΩ nahoře + 2 kΩ dole).
* Zkontroluj **baud** a formát (8N1 je default).

### b) **RS-232 (±12 V, DB9)**

* Potřebuješ **MAX3232** (3.3 V verzi).
* **ESP32 TX → T1IN**, **ESP32 RX ← R1OUT**, **GND ↔ GND**, **VCC 3.3 V**.
* Na DB9 platí DTE/DCE pravidla – když nic neteče, použij **null-modem** kabel (prohození TX/RX).

### c) **RS-485 (diferenciální, A/B)**

* Přidej **MAX3485/MAX485**:

    * **RO → RX ESP32**, **DI ← TX ESP32**, **DE & /RE** svázat na 1 pin ESP32 (např. GPIO21).
    * Na koncích sběrnice **120 Ω** mezi **A–B**; přidej bias odpory (např. 680 Ω A→VCC, 680 Ω B→GND) – pokud je nemá jiné zařízení.
* V kódu zapni `USE_RS485` a při vysílání se DE/RE přepíná (viz `rs485SetTx()`).

### d) **Konzolové piny na routerech/MCU**

* Často **3.3 V TTL** na 115200 8N1.
* Obvykle stačí **GND** + **RX/TX**; někdy **jen čtení** (TX zařízení → RX ESP32).
* Pozor na **napájení** – z cizího zařízení 3.3 V **neodebírej** proud pro ESP32.

---

# 5) Více sériovek najednou

ESP32 má 3 HW UARTy. V příkladu běží:

* `Serial` = debug přes USB (UART0)
* `Serial2` = tvá linka

Můžeš přidat i `Serial1` na libovolné piny:

```cpp
HardwareSerial SerialA(1);
SerialA.begin(115200, SERIAL_8N1, /*RX=*/26, /*TX=*/27);
```

Pak doplň druhé čtení a posílej do WS s prefixem (např. `[A] ...`, `[B] ...`) nebo udělej dvě WS cesty `/ws1`, `/ws2`.

---

# 6) Použití

1. Nahraj kód (zadej SSID/heslo nebo použij AP fallback).
2. Připoj se na IP vypsanou v sériové konzoli (nebo na AP `ESP32-Serial-Bridge` → 192.168.4.1).
3. Otevři stránku, sleduj log, případně měň **baud** a posílej příkazy.

Když budeš chtít, můžu ti to rozšířit:

* víc UARTů ve stejné stránce,
* „hex view“ režim,
* logování do SPIFFS/SD,
* jednoduchý filtr/grep v prohlížeči.
