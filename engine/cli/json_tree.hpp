// A JSON document as a tree that writes back as it was read: objects keep their keys' order and
// numbers keep their own spelling, so a file edited in one place (a glTF's images packed into its
// binary chunk) changes nowhere else. The command line's writers (cli/json.hpp) build answers;
// this is for the files ge has to read, change and write again.
//
// ge:summary A JSON document as an ordered tree: parsed, edited and written back compact
// ge:keywords cli json tree parse dump ordered object array number spelling edit gltf
// ge:skill agent-interface
// ge:test tests/cli_model_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ge::cli {

struct JsonNode {
  enum class Kind : uint8_t { null, boolean, number, string, array, object } kind = Kind::null;
  bool boolean = false;
  std::string text;             // a string's value, or a number's spelling
  std::vector<JsonNode> items;  // an array's
  std::vector<std::pair<std::string, JsonNode>> members;  // an object's, in order

  // An object's member by key, or nullptr.
  JsonNode* find(std::string_view key);
  const JsonNode* find(std::string_view key) const;
  // An object's member by key, added (null) at the end when missing.
  JsonNode& at(std::string_view key);
  // Removes an object's member; false when there was none.
  bool erase(std::string_view key);
  // A number's value (0 for anything else).
  double number() const;

  static JsonNode of_number(double v);
  static JsonNode of_string(std::string s);
};

// `text` as one JSON value; false with `error` (what, and at which byte) when it is not.
bool parse_json(std::string_view text, JsonNode& out, std::string& error);
// The value as compact JSON text.
std::string dump_json(const JsonNode& v);

}  // namespace ge::cli
