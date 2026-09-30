/*
 * wifi_connect.ino
 * Wi-Fi station connect test. On this board Wi-Fi is provided by the ESP32-C6
 * co-processor over SDIO (ESP-Hosted), so the SDIO pins must be given to the
 * core with WiFi.setPins() BEFORE WiFi.begin().
 *
 * Board  : Guition JC4880P443C_I_W (ESP32-P4 + ESP32-C6)
 * Setup  : see the top-level README.md
 * Pins   : from ultramcu/guition-jc4880p4-bsp (board_p4_pins.h, MIT)
 *          https://github.com/ultramcu/guition-jc4880p4-bsp
 *
 * If WiFi.setPins() does not compile, update the ESP32 board package.
 * If it connects and then drops repeatedly, the C6 ESP-Hosted slave firmware
 * version may not match the core (see the BSP README above for the C6 fix).
 */

#include <WiFi.h>

#define WIFI_SDIO_CLK  18
#define WIFI_SDIO_CMD  19
#define WIFI_SDIO_D0   14
#define WIFI_SDIO_D1   15
#define WIFI_SDIO_D2   16
#define WIFI_SDIO_D3   17
#define WIFI_C6_RESET  54

const char *ssid     = "YOUR_WIFI_SSID";
const char *password = "YOUR_WIFI_PASSWORD";

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) delay(10);
  delay(500);

  bool ok = WiFi.setPins(WIFI_SDIO_CLK, WIFI_SDIO_CMD,
                         WIFI_SDIO_D0, WIFI_SDIO_D1, WIFI_SDIO_D2, WIFI_SDIO_D3,
                         WIFI_C6_RESET);
  Serial.printf("WiFi.setPins: %s\n", ok ? "OK" : "FAILED");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting");
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    if (millis() - start > 20000) {
      Serial.println("\nTimeout");
      return;
    }
  }
  Serial.printf("\nConnected! IP: %s\n", WiFi.localIP().toString().c_str());
}

void loop() {
  delay(1000);
}
