// ANOTHER GAME'S FIGURE IN THIS ONE'S POSES (figure_frames.hpp).
//
// ge:summary Shooter's Edge's map from one figure's frames to another's by their names' kinds
// ge:keywords figure_frames frame_kind runs-by-kind
// ge:skill shooter
// ge:test tests/shooters_edge_figure_frames_test.cpp
#include "shooters_edge/app/figure_frames.hpp"

#include <array>

namespace shooters_edge::models {
namespace {

std::string stem_of(std::string_view name) {
  size_t digits = 0;
  while (digits < name.size() && name[name.size() - 1 - digits] >= '0' &&
         name[name.size() - 1 - digits] <= '9')
    digits++;
  // Three digits or more: the first names the variant ("pain2" of "pain203"), the rest the frame.
  name.remove_suffix(digits >= 3 ? digits - 1 : digits);
  std::string out(name);
  for (char& c : out) c = c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
  return out;
}

// A run: frames in a row sharing a stem.
struct Run {
  int first = 0, count = 0;
  FrameKind kind = FrameKind::none;
};

std::vector<Run> runs_of(std::span<const std::string> names) {
  std::vector<Run> out;
  std::string last;
  for (size_t i = 0; i < names.size(); i++) {
    std::string stem = stem_of(names[i]);
    if (out.empty() || stem != last) {
      out.push_back({int(i), 0, frame_kind(names[i])});
      last = std::move(stem);
    }
    out.back().count++;
  }
  return out;
}

}  // namespace

FrameKind frame_kind(std::string_view name) {
  const std::string s = stem_of(name);
  const auto has = [&s](std::string_view w) { return s.find(w) != std::string::npos; };
  if (s.starts_with("cr")) return FrameKind::none;
  if (has("death") || has("deth")) return FrameKind::death;
  if (has("pain")) return FrameKind::pain;
  if (has("att") || s == "light") return FrameKind::attack;
  if (has("run") || has("walk")) return FrameKind::run;
  if (has("stand") || has("stnd")) return FrameKind::stand;
  if (has("jump")) return FrameKind::jump;
  return FrameKind::none;
}

std::vector<int> figure_frames(std::span<const std::string> from, std::span<const std::string> to) {
  std::vector<int> out;
  if (to.empty()) return out;
  const std::vector<Run> target = runs_of(to);
  constexpr size_t kKinds = 7;
  std::array<std::vector<const Run*>, kKinds> of_kind;
  for (const Run& r : target) of_kind[size_t(r.kind)].push_back(&r);
  const int rest = of_kind[size_t(FrameKind::stand)].empty()
                       ? 0
                       : of_kind[size_t(FrameKind::stand)].front()->first;
  std::array<int, kKinds> seen{};
  out.reserve(from.size());
  for (const Run& r : runs_of(from)) {
    const std::vector<const Run*>& same = of_kind[size_t(r.kind)];
    const int n = seen[size_t(r.kind)]++;
    for (int k = 0; k < r.count; k++) {
      if (r.kind == FrameKind::none || same.empty()) {
        out.push_back(rest);
        continue;
      }
      const Run& t = *same[size_t(n) % same.size()];
      out.push_back(t.first + int(int64_t{k} * t.count / r.count));
    }
  }
  return out;
}

}  // namespace shooters_edge::models
