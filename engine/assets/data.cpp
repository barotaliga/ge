// The data sets' registry and the JSON they are written in.
//
// ge:summary The asset data registry and its JSON writer
// ge:keywords assets data registry json jsonout
// ge:skill assets
// ge:test tests/assets_manifest_test.cpp
#include "assets/data.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "core/num_text.hpp"

namespace ge::assets {

namespace {

std::vector<const DataSpec*>& registry() {
  static std::vector<const DataSpec*> specs;
  return specs;
}

}  // namespace

void JsonOut::comma() {
  if (after_key_) {
    after_key_ = false;
    return;
  }
  if (!first_.empty()) {
    if (!first_.back()) out_ += ',';
    first_.back() = false;
  }
}

JsonOut& JsonOut::begin_object() {
  comma();
  out_ += '{';
  first_.push_back(true);
  return *this;
}

JsonOut& JsonOut::end_object() {
  out_ += '}';
  first_.pop_back();
  return *this;
}

JsonOut& JsonOut::begin_array() {
  comma();
  out_ += '[';
  first_.push_back(true);
  return *this;
}

JsonOut& JsonOut::end_array() {
  out_ += ']';
  first_.pop_back();
  return *this;
}

JsonOut& JsonOut::key(std::string_view name) {
  str(name);
  out_ += ':';
  after_key_ = true;
  return *this;
}

JsonOut& JsonOut::num(double x) {
  comma();
  out_ += x == x && x - x == 0 ? core::num::to_string(x) : "null";
  return *this;
}

JsonOut& JsonOut::str(std::string_view s) {
  comma();
  out_ += '"';
  for (const char c : s) {
    if (c == '"' || c == '\\') {
      out_ += '\\';
      out_ += c;
    } else if (static_cast<unsigned char>(c) < 0x20) {
      char buf[8];
      std::snprintf(buf, sizeof buf, "\\u%04x", static_cast<unsigned>(c));
      out_ += buf;
    } else {
      out_ += c;
    }
  }
  out_ += '"';
  return *this;
}

JsonOut& JsonOut::boolean(bool b) {
  comma();
  out_ += b ? "true" : "false";
  return *this;
}

DataRegister::DataRegister(const DataSpec& spec) {
  auto& specs = registry();
  const auto at = std::lower_bound(specs.begin(), specs.end(), &spec, [](auto* a, auto* b) {
    return std::strcmp(a->name, b->name) < 0;
  });
  specs.insert(at, &spec);
}

const std::vector<const DataSpec*>& registered_data() { return registry(); }

const DataSpec* find_data(std::string_view name) {
  for (const DataSpec* spec : registry()) {
    if (name == spec->name) return spec;
  }
  return nullptr;
}

}  // namespace ge::assets
