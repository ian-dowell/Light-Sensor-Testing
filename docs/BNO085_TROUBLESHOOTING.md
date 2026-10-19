# BNO085 connection troubleshooting

## Finding from this setup

The three light sensors work. The BNO085 has a separate QT breakout cable with red connected to 3.3 V, black connected to ground, and blue connected to the seventh header pin on the I2C side of the ESP32. **P0 and P1 have no connections.**

That leaves the listed Adafruit BNO085 breakout in its default **I2C mode**. The project's sketch instead listens for **UART-RVC** on GPIO20 at 115200 baud. Moving the blue wire to a UART input does not change the BNO085's mode. This is a confirmed configuration mismatch; successful readings after correcting it still require a hardware check.

The source code review found no evident UART setup or polling defect that explains persistent absence. Red/black wiring as described matches the intended power connections. The precise ESP32 pin label and physical connections remain unverified.

## Correct the mode first

1. Disconnect USB and any other power from the circuit.
2. Add a separate jumper wire from the **BNO085 pin labeled P0** to the **ESP32 3V3 supply**, sharing that supply with the red cable wire. P0 is on the sensor board, not ESP32 GPIO0. It is not present in the four-wire QT cable. Use a secure electrical connection; a loose wire touching an unsoldered hole is unreliable.
3. Leave **P1 unconnected and its solder jumper open**. The Adafruit #4754 breakout already pulls P1 low. An explicit P1-to-GND wire is optional; do not ground it with its high-select solder jumper bridged.
4. Verify the blue wire connects to the ESP32 pin **labeled 20**. Leave the yellow cable wire disconnected and the BNO085's other QT socket empty.
5. Reconnect power to restart both the BNO085 and ESP32. Pressing only the ESP32 Reset button does not restart the sensor or change its sampled mode.
6. Open Serial Monitor at **115200 baud** with the current four-sensor sketch. Expect `BNO085 [UART-RVC]: yaw=... pitch=... roll=...` and readings that change as you tilt it. No firmware change is needed for this mode correction.

Adafruit documents the [P0-high/P1-low mode selection and default pull-downs](https://learn.adafruit.com/adafruit-9-dof-orientation-imu-fusion-breakout-bno085/pinouts) and the [P0-to-3V wiring](https://learn.adafruit.com/adafruit-9-dof-orientation-imu-fusion-breakout-bno085/uart-rvc-for-arduino). Bridging the **P0** solder jumper is an alternative to adding the P0 wire; do not bridge P1.

## Wiring with your separate QT breakout cable

| Connection at BNO085 | Connect to ESP32 / action |
| --- | --- |
| Red cable wire: VIN | 3V3; your reported connection is correct |
| Black cable wire: GND | GND; your reported connection is correct |
| Blue cable wire: SDA, which becomes UART TX in RVC mode | GPIO20 |
| Yellow cable wire: SCL / UART RX | Leave disconnected |
| P0 header pin, using an additional jumper wire | 3V3 |
| P1 header pin | Leave open with its solder jumper open; onboard pull-down keeps it low |

These are the [standard QT cable colors](https://learn.adafruit.com/introducing-adafruit-stemma-qt/technical-specs). The QT connector exposes the same VIN/GND/SDA/SCL connections as the breakout headers, so a separate breakout cable is suitable. Keep it separate from the light-sensor data bus because SDA becomes a UART output in RVC mode.

For the **Espressif ESP32-C6-DevKitM-1** assumed by this project, J3 starts with these labels in its documented numbering order:

| J3 physical pin | Printed label |
| --- | --- |
| 1 | G |
| 2 | TX |
| 3 | RX |
| 4 | 23 — light-sensor SDA |
| 5 | 22 — light-sensor SCL |
| 6 | 21 |
| 7 | **20 — BNO085 blue wire** |

Counting from the other end gives a different pin. Use the printed **20** label. The pin printed **RX** is GPIO17 and is used by the UART0 console, whereas this sketch explicitly selects GPIO20 for UART1. Other ESP32-C6 boards may have different header positions. [Official DevKitM-1 pinout](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c6/esp32-c6-devkitm-1/user_guide.html#header-block)

## If it still waits after the mode change

- Confirm 3.3 V between BNO085 VIN and GND, and a high level on P0 relative to the same ground. Check cable seating, header solder joints, and breadboard rail continuity. P1 should be low.
- Disconnect power before moving wires. Ensure the blue wire ends at GPIO20 and yellow is unused. Check any previous P0/P1 solder bridges.
- The startup I2C scan should still find only the three light sensors. The BNO085 is detected by receiving UART packets, so its absence from that scan is expected.
- If needed, use the separate `BNO085_Diagnostics/BNO085_Diagnostics.ino` sketch described below. Preserve the current Arduino board, port, and USB CDC setting that already give you Serial Monitor output.

## Optional diagnostic sketch

Open `BNO085_Diagnostics/BNO085_Diagnostics.ino` in Arduino IDE and upload it to the ESP32. Uploading replaces the running four-sensor program until you upload the original again. Keep the diagnostic's helper header in the same folder. It needs the ESP32 board core but no sensor libraries. Leave the sensor wiring as above; the light sensors can remain attached.

The diagnostic listens only to GPIO20 at 115200 baud, reports incoming byte counts and checksum-valid RVC packets, and shows a short hexadecimal sample. It does not configure P0/P1 or communicate with the light sensors.

| Result after several seconds | Interpretation / next check |
| --- | --- |
| No incoming bytes | No UART data is reaching GPIO20. Recheck P0 selection, power-cycle, power, ground, cable, and pin label. This does not by itself prove a faulty sensor. |
| Incoming bytes but no valid RVC packets | Check mode selection, common ground, signal integrity, and wiring. Bytes alone do not establish a working RVC connection. |
| Repeated valid packets and changing angles | The UART link is working. Upload the original four-sensor sketch again and check its BNO085 output. |
| Previously valid packets become stale | Check intermittent power or data connections. A displayed historical sample is not a fresh reading. |

RVC normally sends about 100 packets (1900 bytes) per second. Occasional startup corruption can occur when listening begins mid-packet; persistent failures or loss of valid packets are the useful evidence. The packet format is documented in the [BNO08x datasheet linked by Adafruit](https://learn.adafruit.com/adafruit-9-dof-orientation-imu-fusion-breakout-bno085/downloads) and implemented in the [official Adafruit RVC library](https://github.com/adafruit/Adafruit_BNO08x_RVC/blob/master/Adafruit_BNO08x_RVC.cpp).

If readings still fail, share the exact ESP32 board model, the label beside the blue wire, and several consecutive diagnostic output blocks after the mode change and full power cycle.

## Validation boundary

No ESP32 USB serial device was available on this Mac during this troubleshooting pass. No firmware was uploaded and no sensor readings or electrical measurements were taken. See `docs/bno085_validation.txt` for the separate diagnostic's software checks.
