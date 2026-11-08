// Speeduino <-> Bluetooth + WiFi bridge
// ESP32-WROOM-32 (classic ESP32)
//
// Bluetooth Classic SPP:  device name BT_NAME
// WiFi access point:      SSID WIFI_SSID, password WIFI_PASS
// TCP server:             192.168.4.1 : TCP_PORT  (one client at a time)
//
// Wiring (see README.md):
//   GPIO16 (RX2) <- Mega TX0 (pin 1) through a 1k / 2.2k voltage divider
//   GPIO17 (TX2) -> Mega RX0 (pin 0), direct
//   GND          <-> Mega GND
//   VIN          <- separate 12 V -> 5 V buck converter (WiFi peaks ~500 mA,
//                   too much for the Speeduino 5 V rail)
//
// Arduino IDE board: "ESP32 Dev Module"
// Partition Scheme:  "Huge APP (3MB No OTA)" (BT + WiFi does not fit the default)
//
// Use only ONE connection (Bluetooth or WiFi) at a time.

#include <WiFi.h>
#include "BluetoothSerial.h"

#define SPEEDUINO_BAUD 115200
#define RX_PIN 16
#define TX_PIN 17

#define BT_NAME   "Speeduino"
#define WIFI_SSID "Speeduino"
#define WIFI_PASS "speeduino123"   // WPA2, at least 8 characters - change it!
#define TCP_PORT  2000

BluetoothSerial BT;
WiFiServer server(TCP_PORT);
WiFiClient client;
uint8_t buf[512];

void setup() {
  Serial.begin(115200);                       // debug output over USB
  Serial2.setRxBufferSize(2048);              // must be called before begin()
  Serial2.begin(SPEEDUINO_BAUD, SERIAL_8N1, RX_PIN, TX_PIN);

  // WiFi access point + TCP server
  // Note: do not call WiFi.setSleep(false) - modem sleep is required
  // when WiFi and Bluetooth run at the same time.
  WiFi.mode(WIFI_AP);
  if (WiFi.softAP(WIFI_SSID, WIFI_PASS)) {
    server.begin();
    server.setNoDelay(true);
    Serial.print("WiFi AP \"" WIFI_SSID "\", TCP ");
    Serial.print(WiFi.softAPIP());
    Serial.println(":" + String(TCP_PORT));
  } else {
    Serial.println("Error: WiFi AP failed to start");
  }

  // Bluetooth
  if (BT.begin(BT_NAME)) {
    Serial.println("Bluetooth running as \"" BT_NAME "\"");
  } else {
    Serial.println("Error: Bluetooth failed to start");
  }
}

void loop() {
  // New TCP client replaces the old one
  if (server.hasClient()) {
    if (client && client.connected()) client.stop();
    client = server.available();
    client.setNoDelay(true);
    Serial.print("TCP client connected: ");
    Serial.println(client.remoteIP());
  }
  bool tcpOn = client && client.connected();

  // Speeduino -> active clients (forwarded in blocks, not byte by byte)
  size_t n = Serial2.available();
  if (n) {
    n = Serial2.readBytes(buf, min(n, sizeof(buf)));
    if (tcpOn) client.write(buf, n);
    if (BT.hasClient()) BT.write(buf, n);
  }

  // Bluetooth -> Speeduino
  n = BT.available();
  if (n) {
    n = BT.readBytes(buf, min(n, sizeof(buf)));
    Serial2.write(buf, n);
  }

  // WiFi (TCP) -> Speeduino
  if (tcpOn) {
    n = client.available();
    if (n) {
      int r = client.read(buf, min(n, sizeof(buf)));
      if (r > 0) Serial2.write(buf, r);
    }
  }
}
