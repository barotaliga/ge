// The control tape's codec: base64, the run-length streams, the recorder and the reader.
// Header: racing/tape.hpp. Oracle: oss-game-framework `src/racing/tape.ts`.
//
// ge:summary The control tape's codec: base64, run-length streams, recorder, reader
// ge:keywords racing tape ghost codec base64 atob btoa rle encodeStream decodeStream
// ge:oracle oss-game-framework/src/racing/tape.ts
// ge:skill racing port
// ge:test tests/racing_tape_test.cpp
#include "racing/tape.hpp"

#include <algorithm>
#include <cstring>

namespace ge::racing {

namespace {

constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// atob's alphabet: the sextet a character stands for, or -1.
int sextet(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

// The HTML spec's ASCII whitespace, which atob strips before anything else.
bool is_ascii_space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\f' || c == '\r'; }

}  // namespace

std::string base64_encode(std::span<const uint8_t> bytes) {
  std::string out;
  out.reserve((bytes.size() + 2) / 3 * 4);
  size_t i = 0;
  for (; i + 2 < bytes.size(); i += 3) {
    const uint32_t w = uint32_t(bytes[i]) << 16 | uint32_t(bytes[i + 1]) << 8 | bytes[i + 2];
    out += kAlphabet[w >> 18];
    out += kAlphabet[(w >> 12) & 63];
    out += kAlphabet[(w >> 6) & 63];
    out += kAlphabet[w & 63];
  }
  const size_t left = bytes.size() - i;
  if (left > 0) {
    const uint32_t w = uint32_t(bytes[i]) << 16 | (left == 2 ? uint32_t(bytes[i + 1]) << 8 : 0);
    out += kAlphabet[w >> 18];
    out += kAlphabet[(w >> 12) & 63];
    out += left == 2 ? kAlphabet[(w >> 6) & 63] : '=';
    out += '=';
  }
  return out;
}

// The WHATWG "forgiving-base64 decode" atob runs: strip ASCII whitespace; at a length of 4n
// drop up to two trailing '='; then a length of 4n+1, or any character outside the alphabet
// (a '=' left anywhere included), is an error. Bits left over past the last whole byte are
// discarded, whatever they are.
bool base64_decode(std::string_view text, std::vector<uint8_t>& out) {
  out.clear();
  std::string s;
  s.reserve(text.size());
  for (const char c : text) {
    if (!is_ascii_space(c)) s += c;
  }
  if (s.size() % 4 == 0) {
    for (int k = 0; k < 2 && !s.empty() && s.back() == '='; k++) s.pop_back();
  }
  if (s.size() % 4 == 1) return false;
  out.reserve(s.size() * 3 / 4);
  uint32_t buffer = 0;
  int bits = 0;
  for (const char c : s) {
    const int v = sextet(c);
    if (v < 0) {
      out.clear();
      return false;
    }
    buffer = (buffer << 6 | uint32_t(v)) & 0xffffffu;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<uint8_t>(buffer >> bits));
    }
  }
  return true;
}

std::string encode_stream(std::span<const uint8_t> bytes, std::span<const uint8_t> nan) {
  std::vector<uint8_t> pairs;
  size_t i = 0;
  while (i < bytes.size()) {
    const bool is_nan = !nan.empty() && nan[i] != 0;
    const uint8_t value = is_nan ? uint8_t{0} : bytes[i];
    size_t run = 1;
    if (!is_nan) {
      while (run < 255 && i + run < bytes.size() && bytes[i + run] == value &&
             (nan.empty() || nan[i + run] == 0)) {
        run++;
      }
    }
    pairs.push_back(static_cast<uint8_t>(run));
    pairs.push_back(value);
    i += run;
  }
  return base64_encode(pairs);
}

void decode_stream(std::string_view text, std::span<uint8_t> out) {
  std::fill(out.begin(), out.end(), uint8_t{0});
  std::vector<uint8_t> bytes;
  if (!base64_decode(text, bytes)) return;
  size_t at = 0;
  for (size_t i = 0; i + 1 < bytes.size() && at < out.size(); i += 2) {
    const size_t run = std::min<size_t>(bytes[i], out.size() - at);
    std::memset(out.data() + at, bytes[i + 1], run);
    at += run;
  }
}

TapeRecorder::TapeRecorder(TapeSchema schema)
    : schema_(schema.begin(), schema.end()), streams_(schema.size()) {}

// The oracle buffers every byte and encodes at seal; this keeps each stream's open run instead.
// Same pairs: a run grows while the byte repeats, it is not a NaN, and it is under 255.
void TapeRecorder::record(std::span<const double> values) {
  for (size_t k = 0; k < schema_.size(); k++) {
    Stream& s = streams_[k];
    const bool is_nan = axis_is_nan(values[k], schema_[k].kind);
    const uint8_t byte = axis_to_byte(values[k], schema_[k].kind);
    if (s.run > 0 && !s.open_nan && !is_nan && byte == s.value && s.run < 255) {
      s.run++;
      continue;
    }
    if (s.run > 0) {
      s.pairs.push_back(s.run);
      s.pairs.push_back(s.value);
    }
    s.value = byte;
    s.run = 1;
    s.open_nan = is_nan;
  }
  steps_++;
}

ControlTape TapeRecorder::seal() const {
  ControlTape tape;
  tape.steps = steps_;
  tape.streams.reserve(streams_.size());
  std::vector<uint8_t> pairs;
  for (const Stream& s : streams_) {
    pairs = s.pairs;
    if (s.run > 0) {
      pairs.push_back(s.run);
      pairs.push_back(s.value);
    }
    tape.streams.push_back(base64_encode(pairs));
  }
  return tape;
}

TapeReader::TapeReader(const ControlTape& tape, TapeSchema schema) : steps_(tape.steps) {
  const size_t n = std::max<int32_t>(steps_, 0);
  kinds_.reserve(schema.size());
  bytes_.resize(schema.size() * n);
  for (size_t k = 0; k < schema.size(); k++) {
    kinds_.push_back(schema[k].kind);
    const std::string_view text = k < tape.streams.size() ? tape.streams[k] : std::string_view{};
    decode_stream(text, std::span<uint8_t>(bytes_.data() + k * n, n));
  }
}

bool TapeReader::at(int64_t step, std::span<double> out) const {
  if (step < 0 || step >= steps_) return false;
  const size_t n = size_t(steps_);
  for (size_t k = 0; k < kinds_.size(); k++) {
    out[k] = byte_to_axis(bytes_[k * n + size_t(step)], kinds_[k]);
  }
  return true;
}

std::string Fingerprint::digest() const {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out(8, '0');
  for (int i = 0; i < 8; i++) out[size_t(7 - i)] = kHex[(hash >> (4 * i)) & 15u];
  return out;
}

}  // namespace ge::racing
