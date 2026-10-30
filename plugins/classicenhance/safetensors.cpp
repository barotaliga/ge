// The safetensors reader and writer (safetensors.hpp).
//
// ge:summary The safetensors weights file's reader and writer
// ge:keywords safetensors weights read write header json data-offsets
// ge:skill classicenhance
// ge:test tests/classicenhance_upscaler_test.cpp
#include "classicenhance/safetensors.hpp"

#include <cstring>
#include <string_view>

#include "core/json_read.hpp"

namespace ge::classicenhance {

int64_t Tensor::count() const {
  int64_t n = 1;
  for (const int64_t d : shape) n *= d;
  return n;
}

namespace {

constexpr std::string_view kMeta = "__metadata__.";

bool ends_with(std::string_view s, std::string_view tail, std::string_view& head) {
  if (s.size() <= tail.size() || s.substr(s.size() - tail.size()) != tail) return false;
  head = s.substr(0, s.size() - tail.size());
  return true;
}

// "name.shape.3" → name, "shape", 3; false for a key of another form.
bool indexed(std::string_view key, std::string_view field, std::string_view& name, size_t& i) {
  const size_t dot = key.rfind('.');
  if (dot == std::string_view::npos || dot + 1 >= key.size()) return false;
  i = 0;
  for (const char c : key.substr(dot + 1)) {
    if (c < '0' || c > '9') return false;
    i = i * 10 + size_t(c - '0');
  }
  return ends_with(key.substr(0, dot), std::string(".") + std::string(field), name);
}

void quoted(std::string& out, std::string_view s) {
  out += '"';
  for (const char c : s) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  out += '"';
}

}  // namespace

bool read_safetensors(std::span<const uint8_t> file, Weights& out, std::string& error) {
  out = {};
  if (file.size() < 8) {
    error = "the weights file is " + std::to_string(file.size()) +
            " bytes, shorter than its 8-byte header length; fetch it again";
    return false;
  }
  uint64_t n = 0;
  for (int i = 7; i >= 0; i--) n = (n << 8) | file[size_t(i)];
  if (n > file.size() - 8) {
    error = "the weights file's header says " + std::to_string(n) + " bytes but the file holds " +
            std::to_string(file.size() - 8) + " after it; the file is cut short, fetch it again";
    return false;
  }
  const std::string_view text(reinterpret_cast<const char*>(file.data() + 8), size_t(n));
  core::FlatJson flat;
  std::string why;
  if (!core::read_flat(text, flat, why, true)) {
    error = "the weights file's header is not JSON (" + why + "); fetch it again";
    return false;
  }
  const size_t data_begin = 8 + size_t(n);
  const size_t data_size = file.size() - data_begin;
  std::map<std::string, std::pair<uint64_t, uint64_t>> ranges;
  for (const auto& [key, value] : flat) {
    std::string_view k = key, name;
    size_t i = 0;
    if (k.substr(0, kMeta.size()) == kMeta) {
      out.metadata[std::string(k.substr(kMeta.size()))] = value.text;
    } else if (ends_with(k, ".dtype", name)) {
      out.tensors[std::string(name)].dtype = value.text;
    } else if (indexed(k, "shape", name, i)) {
      auto& shape = out.tensors[std::string(name)].shape;
      if (shape.size() <= i) shape.resize(i + 1);
      shape[i] = int64_t(value.number);
    } else if (indexed(k, "data_offsets", name, i) && i < 2) {
      auto& r = ranges[std::string(name)];
      (i == 0 ? r.first : r.second) = uint64_t(value.number);
    } else if (!(ends_with(k, ".shape", name) || ends_with(k, ".data_offsets", name))) {
      error = "the weights file's header has a field '" + key +
              "' that is not a tensor's dtype, shape or data_offsets; the file is not a "
              "safetensors file";
      return false;
    }
  }
  for (auto& [name, t] : out.tensors) {
    const auto r = ranges.find(name);
    if (t.dtype.empty() || r == ranges.end()) {
      error = "the weights file's tensor '" + name + "' has no dtype or no data_offsets";
      return false;
    }
    const auto [lo, hi] = r->second;
    if (lo > hi || hi > data_size) {
      error = "the weights file's tensor '" + name + "' spans bytes " + std::to_string(lo) + ".." +
              std::to_string(hi) + " of " + std::to_string(data_size) +
              "; the file is cut short, fetch it again";
      return false;
    }
    if (t.dtype != "F32") continue;
    if (hi - lo != uint64_t(t.count()) * 4) {
      error = "the weights file's tensor '" + name + "' holds " + std::to_string(hi - lo) +
              " bytes but its shape needs " + std::to_string(t.count() * 4);
      return false;
    }
    t.data.resize(size_t(t.count()));
    const uint8_t* p = file.data() + data_begin + lo;
    for (size_t j = 0; j < t.data.size(); j++, p += 4) {
      const uint32_t bits =
          uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
      std::memcpy(&t.data[j], &bits, 4);
    }
  }
  return true;
}

std::vector<uint8_t> write_safetensors(const Weights& weights) {
  std::string header = "{";
  bool first = true;
  if (!weights.metadata.empty()) {
    header += "\"__metadata__\":{";
    for (const auto& [k, v] : weights.metadata) {
      if (!first) header += ',';
      first = false;
      quoted(header, k);
      header += ':';
      quoted(header, v);
    }
    header += '}';
  }
  uint64_t offset = 0;
  for (const auto& [name, t] : weights.tensors) {
    if (t.dtype != "F32") continue;
    if (header.size() > 1) header += ',';
    quoted(header, name);
    header += ":{\"dtype\":\"F32\",\"shape\":[";
    for (size_t i = 0; i < t.shape.size(); i++) {
      if (i) header += ',';
      header += std::to_string(t.shape[i]);
    }
    const uint64_t end = offset + t.data.size() * 4;
    header += "],\"data_offsets\":[" + std::to_string(offset) + "," + std::to_string(end) + "]}";
    offset = end;
  }
  header += '}';
  while (header.size() % 8) header += ' ';
  std::vector<uint8_t> out;
  out.reserve(8 + header.size() + offset);
  for (int i = 0; i < 8; i++) out.push_back(uint8_t(uint64_t(header.size()) >> (8 * i)));
  out.insert(out.end(), header.begin(), header.end());
  for (const auto& [name, t] : weights.tensors) {
    if (t.dtype != "F32") continue;
    for (const float f : t.data) {
      uint32_t bits = 0;
      std::memcpy(&bits, &f, 4);
      for (int i = 0; i < 4; i++) out.push_back(uint8_t(bits >> (8 * i)));
    }
  }
  return out;
}

}  // namespace ge::classicenhance
