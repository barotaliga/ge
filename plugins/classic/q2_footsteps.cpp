// Quake II's footsteps by surface: see q2_footsteps.hpp.
//
// ge:summary The .mat materials read, the step sets found, the surface underfoot traced
// ge:keywords cl_footsteps footsteps step-sets material surface-sound
// ge:skill classic audio
// ge:test tests/classic_q2_footsteps_test.cpp
#include "classic/q2_footsteps.hpp"

#include <cctype>
#include <map>
#include <utility>

#include "shooter/brush_contents.hpp"

namespace ge::classic {

namespace {

std::string lower(std::string_view s) {
  std::string o(s);
  for (char& c : o) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return o;
}

bool material_ok(std::string_view m) {
  for (const char c : m)
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') return false;
  return true;
}

// The material a `.mat` file's text names: its first 15 characters, blanks round them trimmed.
std::string material_of(const std::string& text) {
  std::string m = text.substr(0, 15);
  const size_t a = m.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return {};
  const size_t b = m.find_last_not_of(" \t\r\n");
  return m.substr(a, b - a + 1);
}

std::vector<std::string> find_set(const std::string& stem,
                                  const std::function<bool(const std::string&)>& has_sound) {
  std::vector<std::string> out;
  for (int k = 1; k <= kQ2StepVariants; k++) {
    std::string name = stem + std::to_string(k) + ".wav";
    if (!has_sound(name)) break;
    out.push_back(std::move(name));
  }
  return out;
}

}  // namespace

Q2StepSets q2_step_sets(
    const brush::Map& map,
    const std::function<std::optional<std::string>(const std::string&)>& read_text,
    const std::function<bool(const std::string&)>& has_sound, std::vector<std::string>* said) {
  Q2StepSets s;
  s.sets.push_back(find_set("player/step", has_sound));
  s.materials.emplace_back();
  s.sets.push_back(find_set("player/steps/ladder", has_sound));
  s.materials.emplace_back("ladder");
  std::map<std::string, int> by_name;  // a texture's name (without case) to its set
  std::map<std::string, int> by_material{{"default", kQ2StepDefault}, {"ladder", kQ2StepLadder}};
  s.by_texture.assign(map.textures.size(), kQ2StepDefault);
  for (size_t t = 0; t < map.textures.size(); t++) {
    const std::string name = lower(map.textures[t].name);
    if (name.empty()) continue;
    if (const auto it = by_name.find(name); it != by_name.end()) {
      s.by_texture[t] = it->second;
      continue;
    }
    int set = kQ2StepDefault;
    const std::string file = "textures/" + map.textures[t].name + ".mat";
    if (const std::optional<std::string> text = read_text(file)) {
      const std::string m = material_of(*text);
      if (!material_ok(m)) {
        if (said != nullptr) {
          std::string line = "Bad material \"";
          line += m;
          line += "\" in ";
          line += file;
          said->push_back(std::move(line));
        }
      } else if (!m.empty()) {
        const std::string key = lower(m);
        if (const auto mt = by_material.find(key); mt != by_material.end()) {
          set = mt->second;
        } else {
          set = static_cast<int>(s.sets.size());
          s.sets.push_back(find_set("player/steps/" + m, has_sound));
          s.materials.push_back(m);
          by_material.emplace(key, set);
          s.any_material = true;
        }
      }
    }
    by_name.emplace(name, set);
    s.by_texture[t] = set;
  }
  return s;
}

int32_t q2_step_surface(brush::BrushTracer& tracer, const float origin[3], const float* mins,
                        const float* maxs) {
  using namespace ge::shooter;
  const float start[3] = {origin[0], origin[1], origin[2] + 1};
  float lo[3] = {0, 0, 0}, hi[3] = {0, 0, 0};
  float end[3] = {start[0], start[1], start[2] - 75};  // 9 and the largest monster's 66
  if (mins != nullptr && maxs != nullptr) {
    lo[0] = mins[0], lo[1] = mins[1], hi[0] = maxs[0], hi[1] = maxs[1];
    end[2] = origin[2] + mins[2] - 9;
  }
  const brush::Trace solid = tracer.trace(start, end, lo, hi, kSqSolid | kSqWindow);
  if (solid.fraction >= 1) return -1;
  const float near[3] = {solid.end[0], solid.end[1], solid.end[2] + 1};
  const brush::Trace wet = tracer.trace(start, near, lo, hi, kSqSolid | kSqWindow | kSqMaskWater);
  return wet.fraction < 1 ? wet.texture : solid.texture;
}

int q2_step_set(const Q2StepSets& s, int32_t texture) {
  return texture >= 0 && static_cast<size_t>(texture) < s.by_texture.size()
             ? s.by_texture[static_cast<size_t>(texture)]
             : kQ2StepDefault;
}

const std::string* q2_step_pick(const Q2StepSets& s, int set, uint32_t roll,
                                std::string_view last) {
  if (set < 0 || static_cast<size_t>(set) >= s.sets.size() || s.sets[size_t(set)].empty())
    set = kQ2StepDefault;
  if (s.sets.empty() || s.sets[size_t(set)].empty()) return nullptr;
  const std::vector<std::string>& v = s.sets[size_t(set)];
  size_t k = roll % v.size();
  if (v[k] == last) k = (k + 1) % v.size();
  return &v[k];
}

bool q2_unpack_solid(int solid, float mins[3], float maxs[3]) {
  if (solid <= 0 || solid == 31) return false;
  const auto x = static_cast<float>(8 * (solid & 31));
  const auto zd = static_cast<float>(8 * ((solid >> 5) & 31));
  const auto zu = static_cast<float>(8 * ((solid >> 10) & 63) - 32);
  mins[0] = mins[1] = -x, mins[2] = -zd;
  maxs[0] = maxs[1] = x, maxs[2] = zu;
  return true;
}

}  // namespace ge::classic
