# WIRING – Zapojení a piny ESP

Tento dokument udržuje přehled o zapojení, použitých pinech a součástkách pro tento projekt (ESP32 Web ↔ UART bridge).

## Výchozí piny v projektu
Hodnoty v `src/main.cpp` (lze změnit):

- `RX_PIN = 16` – UART RX (ESP přijímá z externího zařízení)
- `TX_PIN = 17` – UART TX (ESP vysílá do zařízení)
- `RS485_EN_PIN = 21` – volitelné, povolí TX režim při RS‑485 (`USE_RS485`)

Poznámka: Piny lze na ESP32 remapovat. Pokud používáte jinou desku/pinout, upravte výše uvedené konstanty.

## Doporučení pro ESP32‑C3 Super Mini
Na této desce bývá vhodné použít:

- `RX_PIN = 20`, `TX_PIN = 21` (výchozí U0RXD/U0TXD)
- `RS485_EN_PIN = 10` (libovolný volný GPIO)

Před zapojením ověřte skutečný pinout vaší varianty desky.

## Zapojení 1: TTL UART zařízení (3.3 V)
- ESP `TX_PIN` → zařízení `RX`
- ESP `RX_PIN` ← zařízení `TX`
- GND ↔ GND (společná zem)
- Napájení zařízení dle specifikace; ESP GPIO jsou 3.3 V (ne 5 V tolerantní).

## Zapojení 2: RS‑485 (poloduplex)
Použijte 3.3 V RS‑485 transceiver (např. MAX3485, SN65HVD3082E). Nedoporučuje se 5 V MAX485 přímo na ESP32 (výstup RO = 5 V).

- ESP `TX_PIN` → transceiver `DI`
- ESP `RX_PIN` ← transceiver `RO`
- ESP `RS485_EN_PIN` → transceiver `DE` a `/RE` (svázat spolu)
- `A`/`B` → kroucený pár sběrnice; zakončení 120 Ω na koncích linky, volitelně bias rezistory dle topologie
- GND ↔ GND (společná zem)

V kódu je řízení směru zajištěno makrem `USE_RS485` a funkcí `rs485SetTx()`.

## Konfigurace v kódu
Změna pinů/přepnutí RS‑485 v `src/main.cpp`:

```cpp
// Piny
static const int RX_PIN = /* váš RX */;
static const int TX_PIN = /* váš TX */;
// #define USE_RS485
#ifdef USE_RS485
static const int RS485_EN_PIN = /* váš EN pin */;
#endif
```

## Doporučení a bezpečnost
- Nepoužívejte piny vyhrazené pro boot/flash/USB, pokud si nejste jisti (viz pinout desky).
- Vždy sdílejte GND mezi ESP a periferiemi.
- Délka kabelu a šum: pro delší vedení použijte RS‑485, stíněný kroucený pár a správné zakončení.
- Před nahráním FW odpojte periférie, které mohou blokovat boot (např. tahání RX do nečekaného stavu).
