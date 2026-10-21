// engine/racing/tape against the TypeScript oracle (tests/golden/racing_tape_*.txt and
// racing_ghost.txt, written by tools/parity/golden-racing.mjs). The ghost is game5's own
// recorder on a bot lap: it must decode, read back and re-encode byte for byte (M3's exit).
//
// ge:summary Parity test: racing/tape and game5's own ghost, byte for byte
// ge:keywords test parity golden tape ghost
// ge:skill testing
#include "racing/tape.hpp"

#include <array>
#include <string>
#include <vector>

#include "golden.hpp"

using ge::test::as_double;
using ge::test::unhex;
namespace r = ge::racing;

namespace {

r::AxisKind kind_of(uint64_t k) { return static_cast<r::AxisKind>(k); }

// game5's ghost layout (pwa/src/game/ghost.ts `TAPE`), in its key order.
constexpr std::array<r::TapeAxis, 5> kGhost{{{"steer", r::AxisKind::Signed},
                                             {"lean", r::AxisKind::Signed},
                                             {"tuck", r::AxisKind::Lever},
                                             {"brake", r::AxisKind::Lever},
                                             {"flags", r::AxisKind::Flags}}};

}  // namespace

TEST(axis_snap_and_bytes_match_oracle) {
  const auto rows = ge::test::golden_rows("tests/golden/racing_tape_axis.txt");
  CHECK(rows.size() > 3000);
  for (const auto& row : rows) {
    CHECK(row.size() == 6);
    const r::AxisKind kind = kind_of(uint64_t(as_double(row[0])));
    const double v = as_double(row[1]);
    const double js_byte = as_double(row[3]);
    const auto u8 = static_cast<uint8_t>(as_double(row[4]));
    CHECK_JS_BITS(r::snap_axis(v, kind), as_double(row[2]));
    CHECK_BITS(r::snap_axis(r::snap_axis(v, kind), kind), r::snap_axis(v, kind));
    CHECK(r::axis_to_byte(v, kind) == u8);
    CHECK(r::axis_is_nan(v, kind) == (js_byte != js_byte));
    CHECK_BITS(r::byte_to_axis(u8, kind), as_double(row[5]));
  }
}

TEST(streams_encode_and_decode_like_oracle) {
  const auto rows = ge::test::golden_words("tests/golden/racing_tape_stream.txt");
  int enc = 0, dec = 0;
  for (const auto& row : rows) {
    if (row[0] == "enc") {
      enc++;
      const r::AxisKind kind = kind_of(std::stoull(row[1]));
      const size_t n = std::stoull(row[2]);
      CHECK(row.size() == 4 + n);
      const r::TapeAxis axis{"a", kind};
      r::TapeRecorder rec(std::span<const r::TapeAxis>(&axis, 1));
      std::vector<uint8_t> bytes, nan;
      for (size_t i = 0; i < n; i++) {
        const double v = as_double(std::stoull(row[3 + i], nullptr, 16));
        rec.record(std::span<const double>(&v, 1));
        bytes.push_back(r::axis_to_byte(v, kind));
        nan.push_back(r::axis_is_nan(v, kind) ? 1 : 0);
      }
      const std::string want = unhex(row[3 + n]);
      const r::ControlTape tape = rec.seal();
      CHECK(tape.steps == int32_t(n));
      CHECK(tape.streams.size() == 1 && tape.streams[0] == want);
      CHECK(r::encode_stream(bytes, nan) == want);
    } else {
      dec++;
      CHECK(row[0] == "dec" && row.size() == 4);
      std::vector<uint8_t> out(std::stoull(row[1]), 0xee);
      r::decode_stream(unhex(row[2]), out);
      const std::string want = unhex(row[3]);
      CHECK(std::string(out.begin(), out.end()) == want);
    }
  }
  CHECK(enc == 66 && dec == 63);
}

TEST(game5_ghost_decodes_and_reencodes_byte_for_byte) {
  const auto rows = ge::test::golden_words("tests/golden/racing_ghost.txt");
  CHECK(rows.size() > 400 && rows[0][0] == "tape" && rows[0].size() == 7);
  r::ControlTape tape;
  tape.steps = std::stoi(rows[0][1]);
  for (size_t k = 0; k < kGhost.size(); k++) tape.streams.push_back(rows[0][2 + k]);

  // Every sample the oracle's reader gave, and the steps it refused.
  const r::TapeReader reader(tape, kGhost);
  CHECK(reader.steps() == tape.steps);
  std::array<double, 5> got{};
  for (size_t i = 1; i < rows.size(); i++) {
    const auto& row = rows[i];
    const int64_t step = std::stoll(row[1]);
    if (row[0] == "null") {
      CHECK(!reader.at(step, got));
      continue;
    }
    CHECK(reader.at(step, got));
    for (size_t k = 0; k < 5; k++)
      CHECK_BITS(got[k], as_double(std::stoull(row[2 + k], nullptr, 16)));
  }

  // Read the whole lap back and record it again: the same five strings.
  r::TapeRecorder rec(kGhost);
  for (int32_t s = 0; s < reader.steps(); s++) {
    reader.at(s, got);
    rec.record(got);
  }
  const r::ControlTape again = rec.seal();
  CHECK(again.steps == tape.steps);
  CHECK(again.streams == tape.streams);

  // And stream by stream through the batch codec.
  std::vector<uint8_t> bytes(size_t(tape.steps));
  for (const auto& text : tape.streams) {
    r::decode_stream(text, bytes);
    CHECK(r::encode_stream(bytes) == text);
  }
}

TEST(fingerprints_match_oracle) {
  const auto rows = ge::test::golden_rows("tests/golden/racing_fingerprint.txt");
  CHECK(rows.size() == 63);
  for (const auto& row : rows) {
    const size_t n = row[0];
    CHECK(row.size() == n + 2);
    r::Fingerprint print;
    for (size_t i = 0; i < n; i++) print.mix(as_double(row[1 + i]));
    CHECK(print.hash == uint32_t(row[1 + n]));
    char want[9];
    std::snprintf(want, sizeof want, "%08llx", static_cast<unsigned long long>(row[1 + n]));
    CHECK(print.digest() == want);
  }
}

TEST(base64_round_trips_every_length) {
  std::vector<uint8_t> bytes, back;
  for (int n = 0; n < 40; n++) {
    CHECK(r::base64_decode(r::base64_encode(bytes), back) && back == bytes);
    bytes.push_back(static_cast<uint8_t>(n * 37 + 11));
  }
}
