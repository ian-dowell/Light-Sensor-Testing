/*
 * BNO085 UART-RVC connection diagnostic for the project's ESP32-C6 board.
 * No external Arduino libraries are required. This sketch only receives on
 * GPIO20; it does not initialize I2C or connect an ESP32 transmit pin.
 *
 * BNO085 VIN -> 3V3, GND -> GND, SDA/TX -> GPIO20, P0 -> 3V3, P1 LOW.
 * P1's default pulldown holds LOW; leave its solder jumper open.
 * Power-cycle after mode changes. See ../docs/BNO085_TROUBLESHOOTING.md
 * for wiring and interpreting results.
 *
 * Select ESP32C6 Dev Module, 4 MB flash, and Serial Monitor at 115200 baud.
 * USB CDC On Boot: Enabled for native USB, Disabled for USB-to-UART bridge.
 */

#include <Arduino.h>
#include "RvcParser.h"

#if !defined(CONFIG_IDF_TARGET_ESP32C6)
#error "Select ESP32C6 Dev Module in the Arduino board menu."
#endif

constexpr int BNO_RX_PIN = 20;  // GPIO label, not physical header position.
constexpr uint32_t BNO_BAUD = 115200;
constexpr uint32_t REPORT_INTERVAL_MS = 1000;
constexpr uint32_t STALE_AFTER_MS = 500;
constexpr unsigned MAX_BYTES_PER_LOOP = 512;
constexpr size_t RAW_SAMPLE_SIZE = 32;

BnoDiagnostics::RvcParser parser;
uint8_t rawSample[RAW_SAMPLE_SIZE] = {};
size_t rawSampleLength = 0;
uint32_t intervalBytes = 0;
uint32_t intervalValid = 0;
uint32_t intervalBadChecksums = 0;
uint32_t lastReportMs = 0;
uint32_t lastByteMs = 0;
uint32_t lastValidMs = 0;

void pollBno() {
  // Every received byte counts, even a single byte or garbage without AA AA.
  // A per-loop limit keeps noisy RX input from starving status reporting.
  for (unsigned i = 0; i < MAX_BYTES_PER_LOOP && Serial1.available() > 0; ++i) {
    const int received = Serial1.read();
    if (received < 0) {
      break;
    }
    const uint8_t byte = static_cast<uint8_t>(received);
    ++intervalBytes;
    lastByteMs = millis();
    if (rawSampleLength < RAW_SAMPLE_SIZE) {
      rawSample[rawSampleLength++] = byte;
    }
    const BnoDiagnostics::ParseResult result = parser.feed(byte);
    if (result == BnoDiagnostics::ParseResult::ValidFrame) {
      ++intervalValid;
      lastValidMs = lastByteMs;
    } else if (result == BnoDiagnostics::ParseResult::BadChecksum) {
      ++intervalBadChecksums;
    }
  }
}

void reportBno(uint32_t now) {
  Serial.printf("\n[%lu ms; window %lu ms] RX=%lu bytes | valid=%lu | bad checksum=%lu\n",
                static_cast<unsigned long>(now),
                static_cast<unsigned long>(now - lastReportMs),
                static_cast<unsigned long>(intervalBytes),
                static_cast<unsigned long>(intervalValid),
                static_cast<unsigned long>(intervalBadChecksums));
  Serial.printf("Totals: RX=%llu bytes | valid=%llu | bad checksum=%llu | queued=%d\n",
                static_cast<unsigned long long>(parser.bytes()),
                static_cast<unsigned long long>(parser.validFrames()),
                static_cast<unsigned long long>(parser.badChecksums()),
                Serial1.available());

  if (parser.bytes() == 0) {
    Serial.println("NO RX DATA observed: check 3V3/GND, SDA/TX -> GPIO20, P0 high/P1 low; power-cycle.");
  } else if (parser.validFrames() == 0) {
    Serial.printf("RX SEEN, NO VALID RVC yet; last byte %lu ms ago. Check mode, pin and wiring.\n",
                  static_cast<unsigned long>(now - lastByteMs));
  } else {
    const uint32_t age = now - lastValidMs;
    const BnoDiagnostics::Orientation &reading = parser.latest();
    Serial.printf("%s | last valid %lu ms ago | last byte %lu ms ago | index=%u\n",
                  age > STALE_AFTER_MS ? "STALE RVC" : "VALID RVC",
                  static_cast<unsigned long>(age),
                  static_cast<unsigned long>(now - lastByteMs), reading.index);
    Serial.printf("%syaw=%.2f pitch=%.2f roll=%.2f deg\n",
                  age > STALE_AFTER_MS ? "Last good reading: " : "",
                  reading.yaw, reading.pitch, reading.roll);
  }

  Serial.printf("Raw first %u byte(s) of this window:",
                static_cast<unsigned>(rawSampleLength));
  if (rawSampleLength == 0) {
    Serial.print(" none");
  }
  for (size_t i = 0; i < rawSampleLength; ++i) {
    Serial.printf(" %02X", rawSample[i]);
  }
  Serial.println();

  intervalBytes = 0;
  intervalValid = 0;
  intervalBadChecksums = 0;
  rawSampleLength = 0;
  lastReportMs = now;
}

void setup() {
  Serial.begin(115200);
  const uint32_t waitStart = millis();
  while (!Serial && millis() - waitStart < 3000) {
    delay(10);
  }
  Serial.println("\nBNO085 UART-RVC diagnostic: GPIO20 RX, 115200 baud, 8N1.");
  Serial.println("VIN=3V3, shared GND, SDA/TX=GPIO20, P0=3V3, P1 LOW (default pulldown; jumper open).");
  Serial.println("Power-cycle the BNO085 after changing its mode straps.");
  Serial.println("Healthy RVC is normally about 100 packets / 1900 bytes per second.");
  Serial.println("Bad checksum counts full AA AA candidates only; all received bytes count as RX.");
  Serial.println("Raw bytes may begin mid-packet. Opening UART alone does not detect a sensor.");

  const size_t bufferSize = Serial1.setRxBufferSize(2048);
  if (bufferSize < 2048) {
    Serial.printf("Warning: requested 2048-byte RX buffer, got %u.\n",
                  static_cast<unsigned>(bufferSize));
  }
  Serial1.begin(BNO_BAUD, SERIAL_8N1, BNO_RX_PIN, -1);
  if (!Serial1) {
    Serial.println("UART1 initialization failed; check board/core settings before interpreting RX counts.");
  }
  lastReportMs = millis();
}

void loop() {
  pollBno();
  const uint32_t now = millis();
  if (now - lastReportMs >= REPORT_INTERVAL_MS) {
    reportBno(now);
  }
  delay(1);
}
