# Repository Guidelines

## Project Structure & Module Organization
- `platformio.ini`: Platform/board config (`env:nologo_esp32c3_super_mini`).
- `src/`: Application sources (entry: `src/main.cpp`).
- `include/`: Header files for shared declarations.
- `lib/`: Optional project-specific libraries.
- `cdn/`
  - `web-serial.html`, `web-serial.app.js`, `web-serial.style.css`: Web UI served from CDN in production (see below).
- `test/`: PlatformIO unit tests (none yet).
- `.pio/`: Build artifacts (generated).

## Build, Test, and Development Commands
- Build: `pio run` — compiles the current environment.
- Upload: `pio run -t upload` — flashes firmware (set port if needed with `--upload-port`).
- Monitor: `pio device monitor --baud 115200` — opens serial monitor.
- Clean: `pio run -t clean` — removes build artifacts.
- Tests: `pio test` — runs PlatformIO unit tests in `test/`.

## Coding Style & Naming Conventions
- Language: Arduino C++ on ESP32 (Async WebServer + WebSocket + UART).
- Indentation: 2 spaces; no tabs.
- Names: constants/pins `UPPER_SNAKE_CASE` (e.g., `WIFI_SSID`, `RX_PIN`); functions and variables `lowerCamelCase` (e.g., `setupWiFi`, `uartBaud`); classes `CamelCase`.
- Headers live in `include/`; prefer forward declarations in headers and definitions in `src/`.
- Keep inline HTML/CSS/JS in `main.cpp` tidy; avoid long lines > 120 chars when practical.

## Testing Guidelines
- Framework: PlatformIO Unity-based tests.
- Location: one suite per folder under `test/`; name tests after the unit/module under test.
- Run locally with `pio test` (optionally filter by environment with `-e nologo_esp32c3_super_mini`).
- Add minimal smoke tests for serial bridge parsing and baud updates when feasible.

## Commit & Pull Request Guidelines
- History is minimal; use Conventional Commits (e.g., `feat: add websocket bridge`, `fix: flush uart on close`).
- Commits: small, focused, with imperative subject; reference issues (`#123`) in body.
- PRs: describe changes, link issues, list testing steps (build/upload/monitor), and include screenshots or logs (web UI, serial output).

## Security & Configuration Tips
- Do not commit real Wi-Fi credentials. Move them to a private header and include it:
  - `include/secrets.h` → `#define WIFI_SSID "..."` / `#define WIFI_PASS "..."`
  - In `main.cpp`: `#include "secrets.h"`
- Pins/RS-485: adjust `RX_PIN`, `TX_PIN`, and `USE_RS485` in `src/main.cpp` as needed.
