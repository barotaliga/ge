// AN ASSET'S DATA: what a builder script is fed from the game's own code, so a model is made off
// the numbers the simulation runs on (a ski's length and sidecut, a skeleton's joints) and never
// off a copy of them typed into the script. A data set is a function that writes one JSON object,
// registered by name with GE_ASSET_DATA in a `data.cpp` beside its builder (assets/manifest.hpp);
// every such file is compiled into the `assets` executable, which prints it (`assets data
// <name>`) for `ge asset build` to hand the builder, and hashes it into the model's stamp, so a
// change to the numbers makes the model stale.
//
// `JsonOut` writes the object: numbers in their shortest round-trip form (core/num_text.hpp), so
// the same numbers give the same bytes, and the same hash, on every platform.
//
// ge:summary The game data a model's builder is fed: GE_ASSET_DATA sets and their JSON writer
// ge:keywords assets data asset-data builder json ge_asset_data registry jsonout game-data
// ge:skill assets
// ge:test tests/assets_manifest_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace ge::assets {

// A JSON value written in order: objects and arrays opened and closed, keys before values.
class JsonOut {
 public:
  JsonOut& begin_object();
  JsonOut& end_object();
  JsonOut& begin_array();
  JsonOut& end_array();
  JsonOut& key(std::string_view name);
  JsonOut& num(double x);
  JsonOut& str(std::string_view s);
  JsonOut& boolean(bool b);
  // A key and a number, or a key and a string, in one call.
  JsonOut& field(std::string_view name, double x) { return key(name).num(x); }
  JsonOut& field(std::string_view name, std::string_view s) { return key(name).str(s); }
  const std::string& text() const { return out_; }

 private:
  void comma();
  std::string out_;
  std::vector<bool> first_;  // per open container: no value written yet
  bool after_key_ = false;
};

struct DataSpec {
  const char* name;     // what a manifest's `data:` line names
  const char* summary;  // one line: what the builder gets
  std::string (*make)();
};

// Every registered data set, by name.
const std::vector<const DataSpec*>& registered_data();
const DataSpec* find_data(std::string_view name);

struct DataRegister {
  explicit DataRegister(const DataSpec& spec);
};

#define GE_ASSET_DATA_CAT2(a, b) a##b
#define GE_ASSET_DATA_CAT(a, b) GE_ASSET_DATA_CAT2(a, b)
// Register `spec` (a DataSpec with static storage) with the `assets` executable.
#define GE_ASSET_DATA(spec) \
  static ::ge::assets::DataRegister GE_ASSET_DATA_CAT(ge_asset_data_, __LINE__)(spec)

}  // namespace ge::assets
