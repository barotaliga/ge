// A JSON object read FLAT: every scalar under its dotted path (`{"input":{"steer":1}}` gives
// `input.steer`). It is all the play protocol and `ge diverge` need: a protocol line is one
// object of scalars and nested objects, and a run's state is compared field by field. Arrays are
// refused unless asked for, and then read as `path.0`, `path.1`, ... (a state may list its
// racers). Numbers are read with the C locale's strtod.
//
// ge:summary A JSON object read flat into dotted paths: the play protocol's and diverge's reader
// ge:keywords json read parse flat dotted path protocol line value object
// ge:skill agent-interface
// ge:test tests/play_session_test.cpp
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace ge::play {

struct JsonValue {
  enum class Kind : uint8_t { number, boolean, string, null } kind = Kind::null;
  double number = 0;
  bool boolean = false;
  std::string text;  // the string's, or the number's own spelling
};

using FlatJson = std::map<std::string, JsonValue>;

// `text` as one JSON object, flat; false with `error` (what, and at which column) when it is not.
bool read_flat(std::string_view text, FlatJson& out, std::string& error, bool arrays = false);

// A value as JSON text again (a number in its own spelling).
std::string json_text(const JsonValue& v);

}  // namespace ge::play
