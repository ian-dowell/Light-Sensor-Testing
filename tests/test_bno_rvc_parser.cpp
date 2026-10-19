#include "../BNO085_Diagnostics/RvcParser.h"

#include <array>
#include <assert.h>
#include <cmath>
#include <cstdio>
#include <vector>

using BnoDiagnostics::ParseResult;
using BnoDiagnostics::RvcParser;
using Frame = std::array<uint8_t, RvcParser::kFrameSize>;

Frame frame(uint8_t index, int16_t yaw, int16_t pitch, int16_t roll) {
  Frame result = {{0xAA, 0xAA, index}};
  const int16_t values[] = {yaw, pitch, roll, -1000, 1000, 0};
  for (size_t i = 0; i < 6; ++i) {
    const uint16_t bits = static_cast<uint16_t>(values[i]);
    result[3 + 2 * i] = static_cast<uint8_t>(bits);
    result[4 + 2 * i] = static_cast<uint8_t>(bits >> 8);
  }
  for (size_t i = 2; i < result.size() - 1; ++i) {
    result.back() = static_cast<uint8_t>(result.back() + result[i]);
  }
  return result;
}

template <typename Bytes>
void feed(RvcParser &parser, const Bytes &bytes) {
  for (uint8_t byte : bytes) {
    parser.feed(byte);
  }
}

void near(float actual, float expected) {
  assert(std::fabs(actual - expected) < 0.001f);
}

void testSignedDecodeAndChunkedInput() {
  RvcParser parser;
  const Frame data = frame(7, -12345, 1234, -32768);
  // Arrival in short chunks must retain state without an Arduino Stream.
  for (size_t i = 0; i < 3; ++i) {
    assert(parser.feed(data[i]) == ParseResult::IncompleteOrNoise);
  }
  assert(parser.bytes() == 3 && parser.validFrames() == 0);
  for (size_t i = 3; i < 18; ++i) {
    assert(parser.feed(data[i]) == ParseResult::IncompleteOrNoise);
  }
  assert(parser.feed(data[18]) == ParseResult::ValidFrame);
  assert(parser.bytes() == 19 && parser.validFrames() == 1);
  assert(parser.badChecksums() == 0 && parser.latest().index == 7);
  near(parser.latest().yaw, -123.45f);
  near(parser.latest().pitch, 12.34f);
  near(parser.latest().roll, -327.68f);
}

void testGarbageAndBackToBackFrames() {
  RvcParser parser;
  feed(parser, std::vector<uint8_t>{0, 0x12, 0xAA, 0x55, 0xAA});
  assert(parser.bytes() == 5 && parser.validFrames() == 0);
  feed(parser, frame(8, 0, 0, 0));
  feed(parser, frame(9, 32767, -1, 18000));
  assert(parser.bytes() == 43 && parser.validFrames() == 2);
  assert(parser.latest().index == 9);
  near(parser.latest().yaw, 327.67f);
  near(parser.latest().pitch, -0.01f);
  near(parser.latest().roll, 180.0f);
  // Leading garbage ends in AA and creates one overlapping bad candidate.
  assert(parser.badChecksums() == 1);
}

void testChecksumFailurePreservesLastGood() {
  RvcParser parser;
  feed(parser, frame(10, 100, 200, 300));
  Frame corrupt = frame(11, 400, 500, 600);
  corrupt[7] ^= 0x40;
  for (size_t i = 0; i < corrupt.size() - 1; ++i) {
    assert(parser.feed(corrupt[i]) == ParseResult::IncompleteOrNoise);
  }
  assert(parser.feed(corrupt.back()) == ParseResult::BadChecksum);
  assert(parser.validFrames() == 1 && parser.badChecksums() == 1);
  assert(parser.latest().index == 10);
  near(parser.latest().roll, 3.0f);
  feed(parser, frame(12, 700, 800, 900));
  assert(parser.validFrames() == 2 && parser.latest().index == 12);
}

void testTruncatedFrameResynchronizesInsideFailedCandidate() {
  RvcParser parser;
  const Frame truncated = frame(20, 100, 200, 300);
  for (size_t i = 0; i < 10; ++i) {
    parser.feed(truncated[i]);
  }
  assert(parser.bytes() == 10 && parser.validFrames() == 0);
  // The next header starts inside the first candidate's 19-byte window.
  feed(parser, frame(21, -100, -200, -300));
  assert(parser.bytes() == 29 && parser.validFrames() == 1);
  assert(parser.badChecksums() == 1 && parser.latest().index == 21);
  near(parser.latest().pitch, -2.0f);
}

void testLongNoiseAndHeaderInPayload() {
  RvcParser parser;
  feed(parser, std::vector<uint8_t>(4096, 0x55));
  assert(parser.bytes() == 4096 && parser.validFrames() == 0);
  assert(parser.badChecksums() == 0);
  // A valid measurement can itself contain AA AA. Consuming the valid frame
  // must prevent its payload from being misreported as a failed candidate.
  feed(parser, frame(30, -21846, 0, 0));
  feed(parser, frame(31, 1, 2, 3));
  assert(parser.validFrames() == 2 && parser.badChecksums() == 0);
  assert(parser.latest().index == 31);
}

int main() {
  testSignedDecodeAndChunkedInput();
  testGarbageAndBackToBackFrames();
  testChecksumFailurePreservesLastGood();
  testTruncatedFrameResynchronizesInsideFailedCandidate();
  testLongNoiseAndHeaderInPayload();
  std::puts("PASS: 5 RVC parser tests (signed/chunked, garbage/back-to-back, checksum, truncated/resync, long noise/payload headers).");
}
