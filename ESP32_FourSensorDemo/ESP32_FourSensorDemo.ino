/*
 * Four-sensor demonstration for an ESP32-C6-DevKitM-1.
 * See ../README.md and ../docs/wiring.svg before connecting power.
 *
 * I2C (3.3 V): SDA=GPIO23, SCL=GPIO22
 *   TCS3448 0x59, TSL2585 0x39, BH1750 0x23
 * UART1 RX=GPIO20 <- BNO085 SDA/TX, 115200 baud, 8N1
 *   BNO085 P0=3.3 V, P1=GND selects UART-RVC mode.
 *   No ESP32 TX connection is needed. Power-cycle after changing P0/P1.
 *
 * Open Serial Monitor at 115200 baud. Output updates once per second;
 * sensor acquisition runs between updates. All values are real readings:
 * missing/stale sensors are reported, never replaced with demo numbers.
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BNO08x_RVC.h>
#include "LightSensors.h"

#if !defined(CONFIG_IDF_TARGET_ESP32C6)
#error "Select ESP32C6 Dev Module in the Arduino board menu."
#endif

// These are GPIO numbers, not physical header positions.
constexpr int I2C_SDA_PIN = 23;
constexpr int I2C_SCL_PIN = 22;
constexpr int BNO_RX_PIN = 20;
constexpr uint32_t CONSOLE_BAUD = 115200;
constexpr uint32_t BNO_BAUD = 115200;
constexpr uint32_t PRINT_INTERVAL_MS = 1000;
constexpr uint32_t BNO_STALE_MS = 500;

Adafruit_BNO08x_RVC bnoRvc;
BNO08x_RVC_Data latestOrientation = {};
bool bnoHasReading = false;
uint32_t bnoLastReadingMs = 0;
uint32_t lastPrintMs = 0;

void scanI2CBus() {
  Serial.println("I2C scan (expected: 0x23 BH1750, 0x39 TSL2585, 0x59 TCS3448):");
  unsigned found = 0;
  for (uint8_t address = 8; address < 0x78; ++address) {
    Wire.beginTransmission(address);
    const uint8_t error = Wire.endTransmission();
    if (error == 0) {
      Serial.printf("  ACK at 0x%02X\n", address);
      ++found;
    } else if (error != 2) {
      Serial.printf("  Bus error %u at 0x%02X; check SDA/SCL and power.\n",
                    error, address);
      break;
    }
  }
  Serial.printf("  %u I2C device(s) found. BNO085 uses UART and is not in this scan.\n", found);
}

void pollOrientation() {
  // RVC sends a 19-byte frame at 100 Hz. Drain queued frames to retain the
  // newest reading. The iteration limit also bounds work on a noisy RX wire.
  for (unsigned attempts = 0; attempts < 128 && Serial1.available() >= 19;
       ++attempts) {
    BNO08x_RVC_Data sample;
    if (bnoRvc.read(&sample)) {  // library checks framing and checksum
      latestOrientation = sample;
      bnoLastReadingMs = millis();
      bnoHasReading = true;
    }
  }
}

void printOrientation() {
  Serial.print("BNO085 [UART-RVC]: ");
  if (!bnoHasReading) {
    Serial.println("waiting for valid packets; check GPIO20 <- SDA, P0=3V3, P1=GND.");
    return;
  }
  const uint32_t age = millis() - bnoLastReadingMs;
  if (age > BNO_STALE_MS) {
    Serial.printf("STALE (last valid packet %lu ms ago); check UART/power.\n",
                  static_cast<unsigned long>(age));
    return;
  }
  Serial.printf("yaw=%.2f pitch=%.2f roll=%.2f deg | accel x=%.3f y=%.3f z=%.3f m/s^2\n",
                latestOrientation.yaw, latestOrientation.pitch,
                latestOrientation.roll, latestOrientation.x_accel,
                latestOrientation.y_accel, latestOrientation.z_accel);
}

void setup() {
  Serial.begin(CONSOLE_BAUD);
  // Allow native USB to attach, but run even when no monitor is connected.
  const uint32_t waitStart = millis();
  while (!Serial && millis() - waitStart < 3000) {
    delay(10);
  }

  Serial.println("\nESP32-C6 four-sensor demo");
  Serial.println("I2C: SDA GPIO23 / SCL GPIO22; BNO085 UART RX GPIO20; all VIN pins 3V3.");
  Serial.println("TCS3448: raw spectrum; TSL2585: normalized counts; BH1750: lux.");
  Serial.println("BNO085 RVC: relative yaw, pitch/roll (deg), acceleration (m/s^2).");

  if (Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 100000)) {
    Wire.setTimeOut(50);
    scanI2CBus();
    beginLightSensors();
  } else {
    Serial.println("I2C initialization failed; UART demonstration will still run.");
  }

  // ESP32 UART1 is separate from the USB/UART0 console. Set RX buffer before
  // begin(). TX=-1 leaves UART1 transmit unassigned on the ESP32-C6 core.
  Serial1.setRxBufferSize(2048);
  Serial1.begin(BNO_BAUD, SERIAL_8N1, BNO_RX_PIN, -1);
  Serial1.setTimeout(5);
  bnoRvc.begin(&Serial1);
  // begin() only attaches a Stream; it does not probe the sensor. Presence is
  // confirmed by the first successfully decoded packet in pollOrientation().
  Serial.println("BNO085: UART ready; waiting for a valid RVC packet.");
  Serial.println("Cover/uncover the light sensors, change light color, and tilt the BNO085.");
  lastPrintMs = millis();
}

void loop() {
  pollOrientation();
  pollLightSensors();
  pollOrientation();

  const uint32_t now = millis();
  if (now - lastPrintMs >= PRINT_INTERVAL_MS) {
    lastPrintMs = now;
    Serial.printf("\n--- uptime %lu ms ---\n", static_cast<unsigned long>(now));
    printLightSensors();
    printOrientation();
  }
  delay(2);
}
