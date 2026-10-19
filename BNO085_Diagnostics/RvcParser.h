#pragma once

#include <stddef.h>
#include <stdint.h>

// UART-RVC wire format: AA AA, index, six signed little-endian 16-bit
// measurements, three reserved bytes, then the sum of bytes 2..17 modulo 256.
// Angles are in hundredths of a degree. See Adafruit's reference decoder:
// https://github.com/adafruit/Adafruit_BNO08x_RVC/blob/master/Adafruit_BNO08x_RVC.cpp
// This parser also counts incomplete/noisy input and retains overlapping
// windows after a failed candidate, so the next frame can resynchronize.
namespace BnoDiagnostics {

struct Orientation {
  uint8_t index = 0;
  float yaw = 0;
  float pitch = 0;
  float roll = 0;
};

enum class ParseResult { IncompleteOrNoise, BadChecksum, ValidFrame };

class RvcParser {
 public:
  static constexpr size_t kFrameSize = 19;

  ParseResult feed(uint8_t byte) {
    ++bytes_;
    if (filled_ == kFrameSize) {
      for (size_t i = 1; i < kFrameSize; ++i) {
        window_[i - 1] = window_[i];
      }
      --filled_;
    }
    window_[filled_++] = byte;
    if (filled_ < kFrameSize || window_[0] != 0xAA || window_[1] != 0xAA) {
      return ParseResult::IncompleteOrNoise;
    }

    uint8_t sum = 0;
    for (size_t i = 2; i < kFrameSize - 1; ++i) {
      sum = static_cast<uint8_t>(sum + window_[i]);
    }
    if (sum != window_[kFrameSize - 1]) {
      ++badChecksums_;
      // Keep all bytes: a later valid frame may start inside this candidate.
      return ParseResult::BadChecksum;
    }

    latest_.index = window_[2];
    latest_.yaw = signedLittleEndian(window_ + 3) * 0.01f;
    latest_.pitch = signedLittleEndian(window_ + 5) * 0.01f;
    latest_.roll = signedLittleEndian(window_ + 7) * 0.01f;
    ++validFrames_;
    // A valid frame was consumed in full. Do not treat its payload as another
    // header; the very next incoming byte can begin the next frame.
    filled_ = 0;
    return ParseResult::ValidFrame;
  }

  uint64_t bytes() const { return bytes_; }
  uint64_t validFrames() const { return validFrames_; }
  uint64_t badChecksums() const { return badChecksums_; }
  const Orientation &latest() const { return latest_; }

 private:
  static int32_t signedLittleEndian(const uint8_t *bytes) {
    const uint32_t value = static_cast<uint32_t>(bytes[0]) |
                           (static_cast<uint32_t>(bytes[1]) << 8);
    // Avoid an implementation-defined unsigned-to-signed narrowing cast.
    return value >= 0x8000U ? static_cast<int32_t>(value) - 0x10000
                            : static_cast<int32_t>(value);
  }

  uint8_t window_[kFrameSize] = {};
  size_t filled_ = 0;
  uint64_t bytes_ = 0;
  uint64_t validFrames_ = 0;
  uint64_t badChecksums_ = 0;
  Orientation latest_;
};

}  // namespace BnoDiagnostics
