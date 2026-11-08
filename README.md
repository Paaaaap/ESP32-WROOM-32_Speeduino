# ESP32-WROOM-32 Speeduino Bluetooth bridge

Wireless (Bluetooth Classic) connection to a [Speeduino](https://speeduino.com) ECU based on an Arduino Mega 2560, using a classic ESP32 dev board as a transparent serial bridge. TunerStudio on a laptop, or a dashboard app on an Android phone, connects to the ESP32 over Bluetooth SPP, and the ESP32 forwards everything to the Speeduino over UART.

This replaces the usual HC-05 / HC-06 modules, which tend to be unreliable at 115200 baud: small buffers, clones with inconsistent firmware, and frequent dropouts.

![Wiring diagram](docs/wiring.png)

## Hardware

| Part | Notes |
|---|---|
| ESP32-WROOM-32 / 32D dev board | Must be the **classic ESP32**. ESP32-S3, C3, C5, C6 and H2 only have BLE and do **not** support Bluetooth Classic (SPP). Tested with an HW-394 (USB-C, CH340) board. |
| Speeduino (Arduino Mega 2560) | Original Mega or a CH340 clone both work. |
| R1 1 kΩ, R2 2.2 kΩ (or 2 kΩ) | Voltage divider: 5 V TX from the Mega → ~3.4 V for the ESP32 RX. |
| 470 µF electrolytic (≥ 10 V) + 100 nF ceramic | Decoupling close to the ESP32 VIN/GND pins. |

## Wiring

| Speeduino / Mega | ESP32 | Notes |
|---|---|---|
| TX0 (pin 1) | GPIO16 / D16 (RX2) | **Through the divider:** R1 1 kΩ in series, R2 2.2 kΩ from GPIO16 to GND |
| RX0 (pin 0) | GPIO17 / D17 (TX2) | Direct. The Mega reads 3.3 V as logic high. |
| GND | GND | Required, the common ground is mandatory. |
| 5V | VIN | **Not** the Mega VIN pin (raw 12–14 V in a car) and **not** the 3.3V pin (only ~50 mA). |

### Important notes

- **Use the Mega's primary serial (pins 0/1).** TunerStudio only works on Serial0. The secondary serial (Serial3, pins 14/15) uses a different protocol (e.g. RealDash) and will not work with TunerStudio.
- **Use GPIO16/17 on the ESP32, not TX0/RX0.** UART0 on the ESP32 is connected to the on-board USB chip and prints boot messages.
- **A DC-DC step-down converter can't be used as a level shifter.** It only works for power. For the signal, use the resistor divider or a BSS138-based level shifter module.
- **Power:** with Bluetooth only, the ESP32 draws ~100–150 mA (peaks ~250 mA), which the Speeduino 5 V rail can usually handle. Keep the 470 µF + 100 nF capacitors close to the ESP32 to avoid brownout resets and noise on the 5 V sensor reference. If you see TPS/MAP readings get noisier, or if you plan to use WiFi (peaks ~500 mA), power the ESP32 from a separate 12 V → 5 V buck converter with a fuse instead.
- **Do not power the ESP32 from USB-C and the 5V wire at the same time.** Disconnect the 5V wire while flashing the ESP32.

### USB and Bluetooth on the same port

Both the Mega's USB chip and the ESP32 drive RX0. With the ESP32 connected, commands from USB won't reach the Mega reliably, and **flashing the Speeduino firmware over USB will fail**. Two options:

1. **Switch / jumper** in the ESP32 TX2 → Mega RX0 wire. Open it for USB, close it for Bluetooth.
2. **Diode (wired-AND):** put a Schottky diode (BAT85, BAT43, 1N5819; a 1N4148 also works) in that wire, with the **cathode (stripe) towards the ESP32** and the anode towards RX0. The ESP32 can then only pull RX0 low, and the USB chip's 1 kΩ resistor keeps the line high when idle. USB and Bluetooth both work without switching. Only use one connection at a time.

The TX0 → divider → GPIO16 line can stay connected in both cases.

## Flashing the ESP32

1. Arduino IDE → Boards Manager → install **esp32 by Espressif Systems**.
2. Board: **ESP32 Dev Module**.
3. Open `speeduino_bt/speeduino_bt.ino` and upload.
   - Stuck on `Connecting...`: hold the **BOOT** button until the upload starts.
   - `Sketch too big`: Tools → Partition Scheme → **Huge APP (3MB No OTA)**.
4. Open the Serial Monitor at 115200 baud. You should see `Bluetooth running as "Speeduino"`.

On Linux the CH340 shows up as `/dev/ttyUSB0`. Your user needs to be in the `uucp` (Arch) or `dialout` (Debian/Ubuntu) group.

## Connecting

### Linux (TunerStudio)

```bash
bluetoothctl        # scan on, pair <MAC>, trust <MAC>, quit
sudo rfcomm bind 0 <MAC> 1
```

In TunerStudio, use port `/dev/rfcomm0` at **115200** baud. If the port is not listed, type it in manually.

### Android

Pair with the **Speeduino** device in Android settings, then select it in your dashboard / logging app.

## Testing step by step

1. **Bluetooth only:** disconnect the Mega and bridge GPIO16 to GPIO17 on the ESP32. Connect from a phone with *Serial Bluetooth Terminal* and type something. If it echoes back, the ESP32 side works.
2. **Mega link:** reconnect the Mega and temporarily add a debug print after the `Serial2.readBytes(...)` line:
   ```cpp
   if (n) Serial.printf("From Speeduino: %u B\n", n);
   ```
   If bytes arrive when TunerStudio connects, the Mega answers and any problem is on the PC side. If nothing arrives, check wiring, swapped TX/RX, the divider and the common ground.

## Installation in the car

- A **plastic** enclosure is fine. Inside a **metal** box the signal will be badly attenuated: put the ESP32 in its own plastic box outside it, or use an **ESP32-WROOM-32U** with an external antenna. The code is the same.
- Point the PCB antenna (the zig-zag trace end of the module) towards the windshield and keep wires, screws and the Mega board 1–2 cm away from it.
- The area under the windshield (plenum) gets wet: seal the enclosure and route the wires in from below.
- Test with the hood closed from the driver's seat before final installation.

## Performance

The bottleneck is the Speeduino's 115200 baud UART, not the radio. Approximate logging rates (depend on firmware, channel count and TunerStudio):

| Link | Round-trip latency | Approx. log rate |
|---|---|---|
| USB cable | ~1–2 ms | ~25–40 samples/s |
| ESP32 WiFi (TCP) | ~3–10 ms | ~20–35 samples/s |
| ESP32 Bluetooth | ~10–30 ms | ~15–25 samples/s |
| HC-05 / HC-06 | ~20–50 ms, varies | ~10–20 samples/s, frequent dropouts |

15–20 samples/s is enough for VE / AFR / ignition tuning.

## Security

This sketch has no PIN and accepts any device. A device connected via TunerStudio can rewrite the tune, so for permanent installation consider adding a MAC address whitelist (`BT.register_callback()` + `ESP_SPP_SRV_OPEN_EVT`, disconnecting unknown addresses). `BT.setPin()` only works with legacy pairing and is ignored by many phones that use Secure Simple Pairing.
