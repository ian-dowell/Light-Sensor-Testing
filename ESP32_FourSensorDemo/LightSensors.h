#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_TCS3448.h>
#include <Adafruit_TSL2585.h>
#include <BH1750.h>
#include <math.h>

// Included only by ESP32_FourSensorDemo.ino. The sketch configures Wire first.
// Libraries: Adafruit TCS3448 1.0.0, Adafruit AS7343 1.1.0,
// Adafruit TSL2585 1.0.0, and BH1750 by Christopher Laws 1.3.0.
namespace LightSensorDemo {

constexpr uint32_t kPollIntervalMs = 20;
constexpr uint32_t kRetryIntervalMs = 1000;
constexpr uint32_t kStaleAfterMs = 3000;
constexpr uint8_t kBh1750Address = 0x23;

struct SensorState {
  bool initialized = false;
  bool hasReading = false;
  bool readFailed = false;
  uint32_t lastGoodMs = 0;
  uint32_t lastPollMs = 0;
};

Adafruit_TCS3448 spectrum;
Adafruit_TSL2585 uvAmbient;
BH1750 illuminance;
SensorState spectrumState;
SensorState uvState;
SensorState luxState;
tcs3448_data_t spectrumData = {};
tsl2585_data_t uvData = {};
float lux = 0.0f;

// Always subtract unsigned timestamps so millis() rollover is harmless.
bool shouldPoll(SensorState &state, uint32_t now) {
  if (!state.initialized) {
    return false;
  }
  const uint32_t interval =
      state.readFailed ? kRetryIntervalMs : kPollIntervalMs;
  if (now - state.lastPollMs < interval) {
    return false;
  }
  state.lastPollMs = now;
  return true;
}

void recordReading(SensorState &state) {
  state.hasReading = true;
  state.readFailed = false;
  state.lastGoodMs = millis();
}

void printFreshness(const SensorState &state, uint32_t now) {
  Serial.print(F(" [age "));
  Serial.print(now - state.lastGoodMs);
  Serial.print(F(" ms"));
  if (now - state.lastGoodMs > kStaleAfterMs) {
    Serial.print(F("; STALE"));
  }
  if (state.readFailed) {
    Serial.print(F("; read failed, showing last good reading"));
  }
  Serial.println(']');
}

bool printUnavailable(const SensorState &state) {
  if (!state.initialized) {
    Serial.println(F("unavailable (not detected or setup failed; check wiring and reset)"));
    return true;
  }
  if (!state.hasReading) {
    if (state.readFailed) {
      Serial.println(F("read failed; retrying"));
    } else if (millis() - state.lastGoodMs > kStaleAfterMs) {
      Serial.println(F("no measurement received within 3 s; check sensor and wiring"));
    } else {
      Serial.println(F("waiting for first measurement"));
    }
    return true;
  }
  return false;
}

void printSpectralChannel(const __FlashStringHelper *label,
                          tcs3448_channel_t channel) {
  Serial.print(label);
  Serial.print('=');
  Serial.print(spectrumData.channels[channel]);
}

void printNormalizedCount(const __FlashStringHelper *label, float value,
                          bool saturated) {
  Serial.print(label);
  Serial.print('=');
  Serial.print(value, 3);
  if (saturated) {
    Serial.print(F(" (SATURATED)"));
  }
}

} // namespace LightSensorDemo

void beginLightSensors() {
  using namespace LightSensorDemo;

  // Each sensor is initialized independently, so a missing board does not stop
  // the remaining sensors. Modest fixed gain suits a typical indoor demo.
  spectrumState.initialized =
      spectrum.begin(&Wire) && spectrum.setGain(TCS3448_GAIN_16X) &&
      spectrum.setATIME(29) && spectrum.setASTEP(599) &&
      spectrum.setSMUXMode(TCS3448_SMUX_18CH) &&
      spectrum.enableLED(false) && spectrum.startMeasurement();
  spectrumState.lastGoodMs = millis();
  Serial.print(F("TCS3448 @ 0x59: "));
  Serial.println(spectrumState.initialized
                     ? F("ready; 16x gain, 50.04 ms/cycle, illumination LED off")
                     : F("not detected or configuration failed"));

  // begin() starts continuous measurements with 50 ms integration and AGC.
  uvState.initialized = uvAmbient.begin(TSL2585_DEFAULT_ADDR, &Wire);
  uvState.lastGoodMs = millis();
  Serial.print(F("TSL2585 @ 0x39: "));
  Serial.println(uvState.initialized
                     ? F("ready; 50 ms integration, automatic gain")
                     : F("not detected or configuration failed"));

  luxState.initialized = illuminance.begin(
      BH1750::CONTINUOUS_HIGH_RES_MODE, kBh1750Address, &Wire);
  luxState.lastGoodMs = millis();
  Serial.print(F("BH1750 @ 0x23: "));
  Serial.println(luxState.initialized
                     ? F("ready; continuous high resolution, MTreg 69")
                     : F("not detected or configuration failed"));

  // BusIO calls Wire.begin() during initialization. Keep the shared bus at
  // the speed selected for this demo after every library has initialized.
  Wire.setClock(100000);
}

void pollLightSensors() {
  using namespace LightSensorDemo;
  const uint32_t now = millis();

  if (shouldPoll(spectrumState, now)) {
    bool ready = false;
    if (!spectrum.getDataReady(&ready)) {
      spectrumState.readFailed = true;
    } else if (ready) {
      // Read status + all 18 register results in one coherent transaction.
      // Unlike readAllChannels(), readData() does not wait for a conversion.
      tcs3448_data_t next = {};
      if (spectrum.readData(&next)) {
        spectrumData = next;
        recordReading(spectrumState);
      } else {
        spectrumState.readFailed = true;
      }
    }
  }

  if (shouldPoll(uvState, now) && uvAmbient.dataReady()) {
    tsl2585_data_t next = {};
    if (uvAmbient.readData(&next)) {
      uvData = next;
      recordReading(uvState);
    } else {
      uvState.readFailed = true;
    }
  }
  // TSL2585 dataReady() returns false for both "not yet ready" and an I2C
  // failure. The freshness age exposes failures that happen at that step.

  if (shouldPoll(luxState, now) && illuminance.measurementReady(true)) {
    // true uses the maximum 180 ms conversion time, without blocking.
    const float next = illuminance.readLightLevel();
    if (isfinite(next) && next >= 0.0f) {
      lux = next;
      recordReading(luxState);
    } else {
      luxState.readFailed = true;
    }
  }
}

// The sketch calls this once a second. Reporting never triggers a conversion.
void printLightSensors() {
  using namespace LightSensorDemo;
  const uint32_t now = millis();

  Serial.print(F("BH1750 illuminance: "));
  if (!printUnavailable(luxState)) {
    Serial.print(lux, 2);
    Serial.print(F(" lux"));
    // With the fixed high-resolution mode and MTreg=69, 65535 raw counts
    // converts to about 54612.5 lux. A clipped value is not a valid maximum.
    if (lux >= 54612.4f) {
      Serial.print(F(" (SATURATED; increase range before using this reading)"));
    }
    printFreshness(luxState, now);
  }

  Serial.print(F("TSL2585 (1x-equivalent counts at 50 ms): "));
  if (!printUnavailable(uvState)) {
    printNormalizedCount(F("photopic"), uvData.photopic_1x,
                         uvData.photopic_saturated);
    Serial.print(F("  "));
    printNormalizedCount(F("IR"), uvData.infrared_1x,
                         uvData.infrared_saturated);
    Serial.print(F("  "));
    printNormalizedCount(F("UVA"), uvData.uva_1x, uvData.uva_saturated);
    printFreshness(uvState, now);
  }

  Serial.print(F("TCS3448 spectrum (raw counts, 16x gain): "));
  if (!printUnavailable(spectrumState)) {
    if (spectrumData.saturated) {
      Serial.print(F("SATURATED; reduce gain or light intensity"));
    }
    printFreshness(spectrumState, now);
    // Typical peak wavelengths from the TCS3448 datasheet, Table 5 (p. 12),
    // DS001121 v2-00, 2026-06-03. These differ from AS7343 channel centers.
    // https://look.ams-osram.com/m/1c24b057e65ee61e/original/TCS3448-14-Channel-multi-spectral-sensor.pdf
    Serial.print(F("  "));
    printSpectralChannel(F("F1~407nm"), TCS3448_CHANNEL_F1);
    Serial.print(F("  "));
    printSpectralChannel(F("F2~424nm"), TCS3448_CHANNEL_F2);
    Serial.print(F("  "));
    printSpectralChannel(F("FZ~450nm"), TCS3448_CHANNEL_FZ);
    Serial.print(F("  "));
    printSpectralChannel(F("F3~473nm"), TCS3448_CHANNEL_F3);
    Serial.print(F("  "));
    printSpectralChannel(F("F4~516nm"), TCS3448_CHANNEL_F4);
    Serial.print(F("  "));
    printSpectralChannel(F("F5~546nm"), TCS3448_CHANNEL_F5);
    Serial.println();
    Serial.print(F("  "));
    printSpectralChannel(F("FY~560nm"), TCS3448_CHANNEL_FY);
    Serial.print(F("  "));
    printSpectralChannel(F("FXL~596nm"), TCS3448_CHANNEL_FXL);
    Serial.print(F("  "));
    printSpectralChannel(F("F6~636nm"), TCS3448_CHANNEL_F6);
    Serial.print(F("  "));
    printSpectralChannel(F("F7~687nm"), TCS3448_CHANNEL_F7);
    Serial.print(F("  "));
    printSpectralChannel(F("F8~748nm"), TCS3448_CHANNEL_F8);
    Serial.print(F("  "));
    printSpectralChannel(F("NIR~855nm"), TCS3448_CHANNEL_NIR);
    Serial.println();
    Serial.print(F("  "));
    printSpectralChannel(F("Clear"), TCS3448_CHANNEL_VIS_TL_0);
    Serial.println(F(" (first SMUX cycle; flicker detection is not enabled)"));
  }
}
