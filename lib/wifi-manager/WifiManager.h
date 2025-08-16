#pragma once

#include <WiFi.h>
#include <Preferences.h>
// #include <AsyncTCP.h>
// #include <RPAsyncTCP.h>          // Use RPAsyncTCP instead of AsyncTCP
// Replace AsyncTCP with a shim to use RPAsyncTCP
#include <AsyncTCP.h>            // AsyncTCP.h should be a shim to RPAsyncTCP
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <vector>

/**
 * WiFiManager
 * ------------
 * Encapsulates connection logic for an ESP32: tries a preferred network,
 * then saved credentials, and falls back to a configuration portal if needed.
 *
 * Usage:
 *   // Define preferred network and timeout
 *   WiFiCredentials preferred = {"MySSID", "MyPassword"};
 *   WifiManager cfg(preferred, 120000); // wait up to 120s
 *   if (!cfg.connectWifi()) {
 *     cfg.startPortal();  // start AP + config portal
 *   }
 */

struct WiFiCredentials {
    String ssid;
    String pass;
};

class WifiManager {
public:
    /**
     * @param preferred   Preferred WiFi credentials (tried first)
     * @param timeout_ms  Total STA connect timeout in milliseconds
     * @param ap_ssid     SSID for fallback AP (default "ESP32_Config")
     * @param ap_pass     Password for fallback AP ("" for open)
     */
    WifiManager(const WiFiCredentials& preferred,
                unsigned long timeout_ms,
                const char* ap_ssid = "ESP32_Config",
                const char* ap_pass = "");

    /**
     * Attempt to connect to WiFi.
     * Returns true if connected within timeout.
     * If false, call startPortal() to begin config AP.
     */
    bool connectWifi();

    /**
     * Start fallback Access Point and web portal for configuring WiFi.
     */
    void startPortal();

    /** Attach config portal routes to an existing AsyncWebServer.
     *  Routes will be available under /wifi and /wifi/save.
     *  Also starts AP and mDNS for the portal.
     */
    void attachPortalRoutes(AsyncWebServer& s);

private:
    Preferences       prefs;
    AsyncWebServer    server;
    WiFiCredentials   preferred;
    unsigned long     maxConnectTime;
    const char*       portalSsid;
    const char*       portalPass;
    struct Cred { String ssid, pass; };
    std::vector<Cred> saved;

    void loadSaved();
    void saveNew(const String& ssid, const String& pass);
    bool attempt(const char* ssid, const char* pass, unsigned long timeout);
    String portalHTML();
    String portalHTMLAtPath(const char* basePath);
};
