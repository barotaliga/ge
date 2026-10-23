// The lab registry (module state: one list per process, filled by static registration before
// main), the Lab context's flag readers and sinks, the digest, and the caption band a shot is
// framed with.
//
// ge:summary The lab registry, the Lab context's sinks, the digest and the caption band
// ge:keywords lab registry digest fnv caption band shot metric module-state
// ge:skill labs
// ge:test tests/lab_runner_test.cpp
#include "lab/lab.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <map>

namespace ge::lab {

namespace {

std::vector<const LabSpec*>& registry() {
  static std::vector<const LabSpec*> labs;
  return labs;
}

std::map<std::string, const char*>& sources() {
  static std::map<std::string, const char*> files;
  return files;
}

const Rgba kBand{24, 26, 32};
const Rgba kTitle{255, 255, 255};
const Rgba kText{200, 204, 212};
const Rgba kLook{255, 214, 102};
const Rgba kInputs{140, 200, 255};

// `text` broken into lines of at most `cols` characters, at spaces where it can.
std::vector<std::string> wrap(const std::string& text, size_t cols) {
  std::vector<std::string> lines;
  std::string line;
  size_t at = 0;
  while (at < text.size()) {
    size_t end = text.find(' ', at);
    if (end == std::string::npos) end = text.size();
    std::string word = text.substr(at, end - at);
    at = end + 1;
    while (word.size() > cols) {
      if (!line.empty()) lines.push_back(std::move(line)), line.clear();
      lines.push_back(word.substr(0, cols));
      word = word.substr(cols);
    }
    if (!line.empty() && line.size() + 1 + word.size() > cols) {
      lines.push_back(std::move(line));
      line.clear();
    }
    if (!line.empty()) line += ' ';
    line += word;
  }
  if (!line.empty()) lines.push_back(std::move(line));
  return lines;
}

}  // namespace

// THE CAPTION BAND: the picture with its words under it — the title, the caption, what to look
// for, and the inputs it was made under.
Picture framed(const Shot& shot, const Picture& picture) {
  const int width = std::max(picture.width, 480);
  const size_t cols = static_cast<size_t>((width - 16) / kGlyphW);
  std::string inputs;
  for (const auto& [k, v] : shot.inputs) {
    if (!inputs.empty()) inputs += "  ";
    inputs.append(k).append("=").append(v);
  }
  const auto caption = wrap(shot.caption, cols);
  const auto look = wrap(shot.look_for.empty() ? "" : "LOOK FOR: " + shot.look_for, cols);
  const auto in = wrap(inputs, cols);
  const int lines = static_cast<int>(caption.size() + look.size() + in.size());
  const int band = 8 + 2 * kGlyphH + 6 + lines * (kGlyphH + 2) + 8;
  Picture out = create_picture(width, picture.height + band, kBand);
  blit(out, picture, 0, 0);
  int y = picture.height + 8;
  draw_text(out, 8, y, shot.title, kTitle, 2);
  y += 2 * kGlyphH + 6;
  for (const auto* group : {&caption, &look, &in}) {
    const Rgba c = group == &caption ? kText : group == &look ? kLook : kInputs;
    for (const std::string& l : *group) {
      draw_text(out, 8, y, l, c);
      y += kGlyphH + 2;
    }
  }
  return out;
}

std::string_view kind_name(LabKind kind) {
  switch (kind) {
    case LabKind::sim:
      return "sim";
    case LabKind::scenario:
      return "scenario";
    case LabKind::picture:
      return "picture";
    case LabKind::drive:
      return "drive";
    case LabKind::bench:
      return "bench";
  }
  return "?";
}

LabRegister::LabRegister(const LabSpec& spec, const char* source) {
  sources()[spec.id] = source;
  auto& labs = registry();
  const auto at = std::lower_bound(labs.begin(), labs.end(), &spec,
                                   [](auto* a, auto* b) { return std::strcmp(a->id, b->id) < 0; });
  labs.insert(at, &spec);
}

const std::vector<const LabSpec*>& registered_labs() { return registry(); }

const char* lab_source(std::string_view id) {
  const auto it = sources().find(std::string(id));
  return it == sources().end() ? "" : it->second;
}

const LabSpec* find_lab(std::string_view id) {
  for (const LabSpec* spec : registry()) {
    if (id == spec->id) return spec;
  }
  return nullptr;
}

Lab::Lab(const LabSpec& spec, std::vector<std::pair<std::string, std::string>> args)
    : spec_(&spec), args_(std::move(args)) {}

const std::string& Lab::arg(std::string_view name) const {
  for (const auto& [k, v] : args_) {
    if (k == name) return v;
  }
  static const std::string empty;
  return empty;
}

double Lab::arg_num(std::string_view name) const { return std::strtod(arg(name).c_str(), nullptr); }

bool Lab::arg_bool(std::string_view name) const {
  const std::string& v = arg(name);
  return !v.empty() && v != "0" && v != "false";
}

std::vector<double> Lab::arg_nums(std::string_view name) const {
  std::vector<double> out;
  const std::string& v = arg(name);
  size_t at = 0;
  while (at < v.size()) {
    size_t end = v.find(',', at);
    if (end == std::string::npos) end = v.size();
    if (end > at) out.push_back(std::strtod(v.substr(at, end - at).c_str(), nullptr));
    at = end + 1;
  }
  return out;
}

void Lab::hash(const void* bytes, size_t n) {
  const auto* b = static_cast<const uint8_t*>(bytes);
  for (size_t i = 0; i < n; i++) digest_ = (digest_ ^ b[i]) * 0x100000001b3ull;
}

void Lab::metric(std::string key, double value, std::string unit, Det det) {
  if (det == Det::deterministic) {
    hash(key.data(), key.size() + 1);
    hash(&value, sizeof value);
  }
  metrics_.push_back(Metric{std::move(key), value, std::move(unit), det});
}

void Lab::table(LabTable t, Det det) {
  if (det == Det::measured) {
    tables_.push_back(std::move(t));
    return;
  }
  for (const auto& row : t.rows) {
    for (const auto& cell : row) hash(cell.data(), cell.size() + 1);
  }
  tables_.push_back(std::move(t));
}

void Lab::note(std::string line) { notes_.push_back(std::move(line)); }

void Lab::shot(Shot s, const Picture& picture, Det det, DistanceMap distance) {
  Picture out = framed(s, picture);
  hash(s.id.data(), s.id.size() + 1);
  if (det == Det::deterministic) hash(out.rgba.data(), out.rgba.size());
  shots_.push_back(
      Taken{std::move(s), std::move(out), det, picture.width, picture.height, std::move(distance)});
}

void Lab::attach(std::string name, std::vector<uint8_t> bytes, Det det) {
  hash(name.data(), name.size() + 1);
  if (det == Det::deterministic) hash(bytes.data(), bytes.size());
  files_.push_back(File{std::move(name), std::move(bytes), det});
}

void Lab::fail(const std::string& what, const std::string& next_action) {
  std::string line = spec_->id;
  line.append(": ").append(what).append("\n    ").append(next_action);
  failures_.push_back(std::move(line));
}

}  // namespace ge::lab
