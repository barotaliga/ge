// Texture sequences (anim.hpp): the sets told by name or chained by the map, and the frame at a
// moment.
//
// ge:summary Texture sequences built from names; the frame a surface shows at a moment
// ge:keywords texture animated cycle random frame build_texture_anims texture_at chain next
// ge:skill brush
// ge:test tests/brush_anim_test.cpp
#include "brush/anim.hpp"

#include <map>

namespace ge::brush {
namespace {

std::string folded(const std::string& s) {
  std::string out = s;
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
  }
  return out;
}

// Keeps frames 0.. up to the first missing one; notes the gap.
std::vector<uint32_t> run(const std::vector<int64_t>& slots, const std::string& what,
                          std::vector<std::string>* notes) {
  std::vector<uint32_t> out;
  for (size_t f = 0; f < slots.size(); ++f) {
    if (slots[f] < 0) {
      for (size_t g = f + 1; g < slots.size(); ++g) {
        if (slots[g] >= 0) {
          notes->push_back(what + ": frame " + std::to_string(f) + " is missing, so frames from " +
                           std::to_string(f) + " on are not drawn");
          break;
        }
      }
      break;
    }
    out.push_back(uint32_t(slots[f]));
  }
  return out;
}

}  // namespace

TextureAnims build_texture_anims(const Map& map) {
  TextureAnims a;
  a.set_of.assign(map.textures.size(), -1);
  a.is_alt.assign(map.textures.size(), false);
  if (map.family != Family::v29 && map.family != Family::bsp2) {
    // The families that chain their frames: each closed chain one cycle, from its lowest texture.
    const size_t n = map.textures.size();
    for (size_t t = 0; t < n; ++t) {
      if (map.textures[t].next < 0 || a.set_of[t] >= 0) continue;
      std::vector<uint32_t> chain;
      std::vector<bool> on(n, false);
      int64_t at = int64_t(t);
      while (at >= 0 && size_t(at) < n && !on[size_t(at)] && a.set_of[size_t(at)] < 0) {
        on[size_t(at)] = true;
        chain.push_back(uint32_t(at));
        at = map.textures[size_t(at)].next;
      }
      if (at != int64_t(t)) {
        if (at < 0 || size_t(at) >= n || a.set_of[size_t(at)] < 0)
          a.notes.push_back(map.textures[t].name +
                            ": its animation's chain does not close, so it "
                            "is drawn still");
        continue;
      }
      if (chain.size() < 2) continue;
      TextureSet set;
      set.name = map.textures[t].name;
      set.frames = chain;
      const auto k = int32_t(a.sets.size());
      for (uint32_t f : set.frames) a.set_of[f] = k;
      a.sets.push_back(std::move(set));
    }
    return a;
  }

  struct Slots {
    std::string name;
    bool random = false;
    std::vector<int64_t> main = std::vector<int64_t>(10, -1), alt = std::vector<int64_t>(10, -1);
  };
  std::map<std::string, Slots> by_key;  // "+name" or "-name", folded
  for (size_t t = 0; t < map.textures.size(); ++t) {
    const std::string& n = map.textures[t].name;
    if (n.size() < 3 || (n[0] != '+' && n[0] != '-')) continue;
    char c = n[1];
    if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    const bool random = n[0] == '-';
    int frame = -1;
    bool alt = false;
    if (c >= '0' && c <= '9') {
      frame = c - '0';
    } else if (!random && c >= 'a' && c <= 'j') {
      frame = c - 'a';
      alt = true;
    }
    if (frame < 0) continue;
    Slots& s = by_key[std::string(1, n[0]) + folded(n.substr(2))];
    if (s.name.empty()) s.name = n.substr(2);
    s.random = random;
    std::vector<int64_t>& v = alt ? s.alt : s.main;
    if (v[size_t(frame)] < 0) v[size_t(frame)] = int64_t(t);
  }

  for (const auto& [key, s] : by_key) {
    TextureSet set;
    set.name = s.name;
    set.random = s.random;
    set.frames = run(s.main, key, &a.notes);
    set.alt = run(s.alt, key + " (alternate)", &a.notes);
    if (set.frames.empty()) {
      if (set.alt.empty()) continue;
      a.notes.push_back(key +
                        ": an alternate cycle with no frame 0 of its main one; the "
                        "alternate's frames are drawn for both");
      set.frames = set.alt;
    }
    const auto k = int32_t(a.sets.size());
    for (uint32_t t : set.frames) a.set_of[t] = k;
    for (uint32_t t : set.alt) {
      a.set_of[t] = k;
      a.is_alt[t] = true;
    }
    a.sets.push_back(std::move(set));
  }
  return a;
}

uint32_t texture_at(const TextureAnims& anims, uint32_t texture, double seconds, bool alternate,
                    uint32_t surface) {
  if (texture >= anims.set_of.size() || anims.set_of[texture] < 0) return texture;
  const TextureSet& s = anims.sets[size_t(anims.set_of[texture])];
  if (s.random) return s.frames[surface % s.frames.size()];
  const std::vector<uint32_t>& cycle = alternate && !s.alt.empty() ? s.alt : s.frames;
  const double ticks = seconds * kFramesPerSecond;
  auto tick = int64_t(ticks);
  if (double(tick) > ticks) --tick;  // floor for a negative moment
  const auto n = int64_t(cycle.size());
  return cycle[size_t(((tick % n) + n) % n)];
}

}  // namespace ge::brush
