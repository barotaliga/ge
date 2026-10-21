// THE CONTROL TAPE: a run kept as the CONTROLS that rode it. Port of oss-game-framework
// `src/racing/tape.ts`, byte for byte: a tape game5 stored decodes here and re-encodes to the
// same text, so a ghost or a replay crosses between the two runtimes unchanged.
//
// On a deterministic engine the same map and the same inputs put the vehicle over the same
// metre of ground, so a ghost is what the hands did, not where the vehicle went. The bargain is
// ONE BYTE PER AXIS PER STEP, on a fixed grid: every control is snapped (`snap_axis`) where the
// input is produced and again where the engine is handed it, so the figure the engine is ridden
// on IS the figure the tape writes down.
//
// THE FORMAT, stated here and nowhere else:
// - A tape is a schema of axes (`TapeAxis`): `Signed` (-1..1 on 255 positions about a centre),
//   `Lever` (0..1 on 256) and `Flags` (eight booleans in a byte). The names are the stored
//   tape's keys, so renaming one is a format change.
// - Each axis is one stream: a byte per step, run-length encoded as (run, value) pairs with a
//   run capped at 255, then base64 (with `=` padding, as `btoa` writes it).
// - Decoding is forgiving the way `atob` is (ASCII whitespace skipped, missing padding
//   accepted), and a short or damaged stream leaves its tail at zero instead of failing.
//
// Not ported: `isControlTape` (the shape check on a parsed JSON blob). It belongs with the
// ghost store and its JSON reader (M10). Parity: tests/racing_tape_test.cpp against
// tests/golden/racing_tape_*.txt and racing_ghost.txt (tools/parity/golden-racing.mjs).
//
// ge:summary The control tape a ghost and a replay are kept on: axes, RLE, base64
// ge:keywords racing tape ghost replay controls input recording rle base64 snapAxis recorder reader
// ge:keywords fingerprint ControlTape
// ge:oracle oss-game-framework/src/racing/tape.ts
// ge:skill racing port
// ge:test tests/racing_tape_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/js.hpp"
#include "core/math.hpp"

namespace ge::racing {

// Positions each side of centre on a signed axis.
inline constexpr int kSignedSteps = 127;
// Positions on a lever, 0 to full.
inline constexpr int kLeverSteps = 255;

// How an axis is kept on the tape (the oracle's "signed" | "lever" | "flags").
enum class AxisKind : uint8_t { Signed, Lever, Flags };

// One stream of a tape's layout. The order of a schema's axes is the order the oracle's schema
// object lists its keys, and every value array below is in that order.
struct TapeAxis {
  std::string_view name;
  AxisKind kind;
};
using TapeSchema = std::span<const TapeAxis>;

// One run's controls: how many steps it runs for, and one encoded stream per schema axis.
struct ControlTape {
  int32_t steps = 0;
  std::vector<std::string> streams;
};

// Snap onto a grid of `steps` positions per unit. Centre comes back as +0: rounding a hair
// below it gives -0, which the tape cannot write down.
inline double snap_to_grid(double v, double steps) {
  const double index = core::js::round(v * steps);
  return index == 0 ? 0.0 : index / steps;
}

// ONE AXIS ON THE GRID THE TAPE WRITES: clamped to the axis's range, then snapped. Idempotent.
// A flags value comes back as it went in. (core::clamp passes NaN through, as the oracle's does.)
inline double snap_axis(double v, AxisKind kind) {
  if (kind == AxisKind::Signed) return snap_to_grid(core::clamp(v, -1, 1), kSignedSteps);
  if (kind == AxisKind::Lever) return snap_to_grid(core::clamp(v, 0, 1), kLeverSteps);
  return v;
}

// The byte an axis value is written as. The oracle computes a number and stores it into a byte
// array, which maps NaN to 0; this returns that stored byte. (Only the tape encoder can tell a
// NaN byte from a 0, and it asks `axis_is_nan`.)
inline uint8_t axis_to_byte(double v, AxisKind kind) {
  double b;
  if (kind == AxisKind::Signed) {
    b = core::js::round(core::clamp(v, -1, 1) * kSignedSteps) + kSignedSteps;
  } else if (kind == AxisKind::Lever) {
    b = core::js::round(core::clamp(v, 0, 1) * kLeverSteps);
  } else {
    return static_cast<uint8_t>(core::js::to_int32(v) & 0xff);
  }
  return b == b ? static_cast<uint8_t>(b) : uint8_t{0};  // in 0..255 once clamped
}

// Whether `axis_to_byte` saw a NaN the oracle would carry as NaN: a signed or lever value (a
// flags value is masked to an integer first). The encoder writes such a byte as 0 but never
// merges it into a run, because NaN === NaN is false.
inline bool axis_is_nan(double v, AxisKind kind) { return kind != AxisKind::Flags && v != v; }

// The value a byte reads back as.
inline double byte_to_axis(uint8_t b, AxisKind kind) {
  if (kind == AxisKind::Signed) return (double(b) - kSignedSteps) / kSignedSteps;
  if (kind == AxisKind::Lever) return double(b) / kLeverSteps;
  return double(b);
}

// Base64 as `btoa` writes it, and `atob`'s forgiving decode. `base64_decode` returns false
// (leaving `out` empty) where `atob` throws.
std::string base64_encode(std::span<const uint8_t> bytes);
bool base64_decode(std::string_view text, std::vector<uint8_t>& out);

// Run-length encode a byte per step, then base64 it. `nan` (may be empty) marks the steps whose
// byte the oracle held as NaN (`axis_is_nan`); they are written as 0 in runs of one.
std::string encode_stream(std::span<const uint8_t> bytes, std::span<const uint8_t> nan = {});

// Decode `out.size()` steps. A short or damaged stream leaves its tail at 0; text `atob` rejects
// leaves all of it at 0.
void decode_stream(std::string_view text, std::span<uint8_t> out);

// Writes a tape step by step, run-length encoding as it goes: one value per schema axis each
// step, the input the engine ACTUALLY received. No allocation per step once a stream's pairs
// have grown to the run's length.
class TapeRecorder {
 public:
  explicit TapeRecorder(TapeSchema schema);
  void record(std::span<const double> values);
  int32_t steps() const { return steps_; }
  // The tape as it stands; callable mid-run.
  ControlTape seal() const;

 private:
  struct Stream {
    std::vector<uint8_t> pairs;  // closed (run, value) pairs
    uint8_t value = 0;           // the open run's byte
    uint8_t run = 0;             // the open run's length, 0 before the first step
    bool open_nan = false;       // the open run is a NaN, which nothing joins
  };
  std::vector<TapeAxis> schema_;
  std::vector<Stream> streams_;
  int32_t steps_ = 0;
};

// A tape put back on the ground: every stream decoded once, stream-major.
class TapeReader {
 public:
  TapeReader(const ControlTape& tape, TapeSchema schema);
  int32_t steps() const { return steps_; }
  // The controls step `step` was ridden on, written into `out` (one per axis). False before the
  // tape starts or once it has run out: the caller's cue to hand the engine its neutral input.
  bool at(int64_t step, std::span<double> out) const;

 private:
  std::vector<AxisKind> kinds_;
  std::vector<uint8_t> bytes_;  // kinds_.size() streams of steps_ bytes
  int32_t steps_ = 0;
};

// A FINGERPRINT: FNV-1a over numbers, each rounded to a hundredth and mixed as a 32-bit
// integer. Enough to tell a map the generator has moved from the one a tape was ridden on.
struct Fingerprint {
  uint32_t hash = 0x811c9dc5u;
  Fingerprint& mix(double v) {
    const auto n = static_cast<uint32_t>(core::js::to_int32(core::js::round(v * 100)));
    for (int s = 0; s < 32; s += 8) {
      hash ^= (n >> s) & 0xffu;
      hash *= 0x01000193u;  // Math.imul then >>> 0: the low 32 bits
    }
    return *this;
  }
  // Eight lowercase hex digits.
  std::string digest() const;
};

}  // namespace ge::racing
