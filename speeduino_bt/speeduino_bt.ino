// Speeduino <-> Bluetooth bridge
// ESP32-WROOM-32 (classic ESP32, Bluetooth Classic SPP)
//
// Wiring (see README.md):
//   GPIO16 (RX2) <- Mega TX0 (pin 1) through a 1k / 2.2k voltage divider
//   GPIO17 (TX2) -> Mega RX0 (pin 0), direct
//   GND          <-> Mega GND
//   VIN          <- Mega 5V (with 470 uF + 100 nF close to the ESP32)
//
// Arduino IDE board: "ESP32 Dev Module"
// If you get "Sketch too big": Tools -> Partition Scheme -> "Huge APP (3MB No OTA)"

#include "BluetoothSerial.h"

#define SPEEDUINO_BAUD 115200
#define RX_PIN 16
#define TX_PIN 17
#define BT_NAME "Speeduino"

BluetoothSerial BT;
uint8_t buf[512];

void setup() {
  Serial.begin(115200);                       // debug output over USB
  Serial2.setRxBufferSize(2048);              // must be called before begin()
  Serial2.begin(SPEEDUINO_BAUD, SERIAL_8N1, RX_PIN, TX_PIN);

  if (!BT.begin(BT_NAME)) {
    Serial.println("Error: Bluetooth failed to start");
  } else {
    Serial.println("Bluetooth running as \"" BT_NAME "\"");
  }
}

void loop() {
  // Speeduino -> Bluetooth (forwarded in blocks, not byte by byte)
  size_t n = Serial2.available();
  if (n) {
    n = Serial2.readBytes(buf, min(n, sizeof(buf)));
    BT.write(buf, n);
  }

  // Bluetooth -> Speeduino
  n = BT.available();
  if (n) {
    n = BT.readBytes(buf, min(n, sizeof(buf)));
    Serial2.write(buf, n);
  }
}
