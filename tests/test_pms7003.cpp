#include <vector>
#include "test.h"
#include "pms7003.h"

// Builds a valid 32-byte PMS7003 frame with the given atmospheric PM values.
static std::vector<uint8_t> makeFrame(uint16_t pm1Atm, uint16_t pm25Atm, uint16_t pm10Atm) {
  std::vector<uint8_t> f(32, 0);
  f[0] = 0x42; f[1] = 0x4D; f[2] = 0x00; f[3] = 28;
  f[4] = 0; f[5] = 1; f[6] = 0; f[7] = 2; f[8] = 0; f[9] = 3;   // CF=1 values 1,2,3
  f[10] = pm1Atm >> 8; f[11] = pm1Atm & 0xFF;
  f[12] = pm25Atm >> 8; f[13] = pm25Atm & 0xFF;
  f[14] = pm10Atm >> 8; f[15] = pm10Atm & 0xFF;
  uint16_t sum = 0;
  for (int i = 0; i < 30; ++i) sum += f[i];
  f[30] = sum >> 8; f[31] = sum & 0xFF;
  return f;
}

static int feedAll(Pms7003Parser& p, const std::vector<uint8_t>& bytes, PmsFrame& out) {
  int frames = 0;
  for (uint8_t b : bytes) if (p.feed(b, out)) ++frames;
  return frames;
}

TEST(pms_parses_valid_frame) {
  Pms7003Parser p; PmsFrame f{};
  CHECK_EQ(feedAll(p, makeFrame(5, 8, 11), f), 1);
  CHECK_EQ(f.pm1Atm, 5); CHECK_EQ(f.pm25Atm, 8); CHECK_EQ(f.pm10Atm, 11);
  CHECK_EQ(f.pm1Std, 1); CHECK_EQ(f.pm25Std, 2); CHECK_EQ(f.pm10Std, 3);
  CHECK_EQ(p.framesOk(), 1);
}

TEST(pms_resyncs_after_noise_and_partial_frame) {
  Pms7003Parser p; PmsFrame f{};
  std::vector<uint8_t> stream = {0x00, 0xFF, 0x4D, 0x42, 0x11};        // noise, lone 0x4D, false start
  std::vector<uint8_t> partial = makeFrame(1, 1, 1);
  partial.resize(20);                                                   // truncated frame
  stream.insert(stream.end(), partial.begin(), partial.end());
  std::vector<uint8_t> good = makeFrame(7, 9, 12);
  stream.insert(stream.end(), good.begin(), good.end());
  CHECK_EQ(feedAll(p, stream, f), 1);
  CHECK_EQ(f.pm25Atm, 9);
}

TEST(pms_double_start_byte_does_not_lose_frame) {
  Pms7003Parser p; PmsFrame f{};
  std::vector<uint8_t> stream = {0x42};                                 // stray start byte
  std::vector<uint8_t> good = makeFrame(3, 4, 5);
  stream.insert(stream.end(), good.begin(), good.end());
  CHECK_EQ(feedAll(p, stream, f), 1);
  CHECK_EQ(f.pm25Atm, 4);
}

TEST(pms_rejects_bad_checksum_and_counts_it) {
  Pms7003Parser p; PmsFrame f{};
  std::vector<uint8_t> bad = makeFrame(3, 4, 5);
  bad[13] ^= 0x01;                                                      // corrupt payload
  CHECK_EQ(feedAll(p, bad, f), 0);
  CHECK_EQ(p.checksumErrors(), 1);
  CHECK_EQ(feedAll(p, makeFrame(1, 2, 3), f), 1);                       // recovers immediately
  CHECK_EQ(f.pm25Atm, 2);
}

TEST(pms_rejects_wrong_length_field) {
  Pms7003Parser p; PmsFrame f{};
  std::vector<uint8_t> bad = makeFrame(3, 4, 5);
  bad[3] = 20;
  std::vector<uint8_t> stream = bad;
  std::vector<uint8_t> good = makeFrame(6, 7, 8);
  stream.insert(stream.end(), good.begin(), good.end());
  CHECK_EQ(feedAll(p, stream, f), 1);
  CHECK_EQ(p.lengthErrors(), 1);
  CHECK_EQ(f.pm25Atm, 7);
}

TEST(pms_back_to_back_frames) {
  Pms7003Parser p; PmsFrame f{};
  std::vector<uint8_t> stream;
  for (int i = 0; i < 5; ++i) { auto g = makeFrame(i, i * 2, i * 3); stream.insert(stream.end(), g.begin(), g.end()); }
  CHECK_EQ(feedAll(p, stream, f), 5);
  CHECK_EQ(f.pm10Atm, 12);
}
