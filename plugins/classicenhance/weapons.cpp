// The classic pack's held weapons into the view-model layer (weapons.hpp): the frames turned
// into the held frame, the skin and its glow, the line down to the floor's lightmap, the flash
// and the map's flames as point lights.
//
// ge:summary The classic held weapons into view models; the floor's light under the player
// ge:keywords viewmodel held-weapon morph palette fullbright probe lightmap muzzle-flash torch
// ge:skill classicenhance
// ge:test tests/classicenhance_weapons_test.cpp
#include "classicenhance/weapons.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "morph/read.hpp"
#include "morph/smooth.hpp"
#include "vfs/pak.hpp"

namespace ge::classicenhance {

namespace {

// The file's eye frame (+x forward, +y left, +z up, units) into the held frame (+x right, +y up,
// +z forward, m). A reflection: the two frames differ in handedness.
void to_held(const float* in, float* out, double scale) {
  out[0] = float(-double(in[1]) * scale);
  out[1] = float(double(in[2]) * scale);
  out[2] = float(double(in[0]) * scale);
}

// The lightmap's colour at (u, v) across lightmap `l`, bilinear between luxel centres.
void luxel_rgb(const brush::Map& map, int32_t l, double u, double v, double out[3]) {
  const brush::Lightmap& lm = map.lightmaps[size_t(l)];
  const double x = std::clamp(u * lm.width - 0.5, 0.0, double(lm.width) - 1);
  const double y = std::clamp(v * lm.height - 0.5, 0.0, double(lm.height) - 1);
  const auto x0 = uint32_t(x), y0 = uint32_t(y);
  const uint32_t x1 = std::min(x0 + 1, lm.width - 1), y1 = std::min(y0 + 1, lm.height - 1);
  const double fx = x - x0, fy = y - y0;
  for (size_t c = 0; c < 3; c++) {
    const auto g = [&](uint32_t a, uint32_t b) {
      return double(map.light_rgb[lm.offset + (size_t(b) * lm.width + a) * 3 + c]);
    };
    out[c] = (g(x0, y0) * (1 - fx) + g(x1, y0) * fx) * (1 - fy) +
             (g(x0, y1) * (1 - fx) + g(x1, y1) * fx) * fy;
  }
}

bool parse_origin(const std::string& s, float out[3]) {
  return std::sscanf(s.c_str(), "%f %f %f", &out[0], &out[1], &out[2]) == 3;
}

// The file's own muzzle flash: glowing triangles parked BEHIND the eye (out of sight) on some
// frames and in front of it on others, as the classic files hide a flash between shots. A lamp
// or a sight glows on every frame in front of the eye and is not marked.
void mark_flame(viewmodel::HeldModel& h) {
  h.flame.assign(h.indices.size() / 3, 0);
  if (h.skin.glow.empty() || h.frames.size() < 2) return;
  const assets::Image& img = h.skin.image;
  for (size_t t = 0; t + 2 < h.indices.size(); t += 3) {
    double u = 0, v = 0;
    for (size_t k = 0; k < 3; k++) {
      u += h.uv[size_t{h.indices[t + k]} * 2] / 3.0;
      v += h.uv[size_t{h.indices[t + k]} * 2 + 1] / 3.0;
    }
    const int x = std::clamp(int(u * img.width), 0, img.width - 1);
    const int y = std::clamp(int(v * img.height), 0, img.height - 1);
    if (h.skin.glow[size_t(y) * size_t(img.width) + size_t(x)] == 0) continue;
    bool behind = false, ahead = false;
    for (const viewmodel::HeldFrame& f : h.frames) {
      double z = 0;
      for (size_t k = 0; k < 3; k++) z += f.positions[size_t{h.indices[t + k]} * 3 + 2];
      (z < 0 ? behind : ahead) = true;
    }
    if (behind && ahead) h.flame[t / 3] = 1;
  }
}

}  // namespace

viewmodel::HeldModel held_from_morph(const morph::Model& model, const Palette& palette, int skin) {
  viewmodel::HeldModel out;
  const uint32_t n = model.vertex_count();
  out.frames.reserve(model.frames.size());
  for (const morph::Frame& f : model.frames) {
    viewmodel::HeldFrame h;
    h.name = f.name;
    h.positions.resize(size_t(n) * 3);
    h.normals.resize(size_t(n) * 3);
    const std::vector<float> nrm = morph::frame_normals(model, f);
    for (size_t v = 0; v < n; v++) {
      to_held(&f.positions[v * 3], &h.positions[v * 3], brush::kUnitM);
      to_held(&nrm[v * 3], &h.normals[v * 3], 1.0);
    }
    out.frames.push_back(std::move(h));
  }
  // The reflection turns every front inward; the other winding turns it back out.
  out.indices.reserve(model.indices.size());
  for (size_t t = 0; t + 2 < model.indices.size(); t += 3) {
    out.indices.insert(out.indices.end(),
                       {model.indices[t], model.indices[t + 2], model.indices[t + 1]});
  }
  out.uv = model.st;
  if (!model.skins.empty()) {
    const morph::Skin& s = model.skins[size_t(std::clamp(skin, 0, int(model.skins.size()) - 1))];
    out.skin.image = to_rgba(s.indices, s.width, s.height, palette);
    out.skin.glow.resize(s.indices.size());
    bool any = false;
    for (size_t i = 0; i < s.indices.size(); i++) {
      out.skin.glow[i] = s.indices[i] >= kFullbrightFirst ? 1 : 0;
      any = any || out.skin.glow[i] != 0;
    }
    if (!any) out.skin.glow.clear();
  } else {
    out.colour.assign(size_t(n) * 3, 0.5f);
  }
  mark_flame(out);
  viewmodel::find_muzzle(out);
  return out;
}

viewmodel::PlaceRow classic_weapon_row() {
  viewmodel::PlaceRow row = viewmodel::classic_row();
  row.offset_m[0] = row.offset_m[1] = row.offset_m[2] = 0;
  return row;
}

viewmodel::Probe probe_under(const brush::Map& map, const float at[3], bool* found,
                             float* floor_z) {
  viewmodel::Probe probe;
  probe.ambient[0] = probe.ambient[1] = probe.ambient[2] = 0;
  probe.toward[0] = 0;
  probe.toward[1] = 1;
  probe.toward[2] = 0;
  if (found) *found = false;
  if (map.models.empty()) return probe;
  const brush::Model& world = map.models[0];
  double best = at[2] - kProbeDepth;
  bool hit = false;
  double rgb[3] = {0, 0, 0};
  for (uint32_t s = world.first_surface; s < world.first_surface + world.surface_count; s++) {
    if (s >= map.surfaces.size()) break;
    const brush::Surface& surf = map.surfaces[s];
    if (surf.lightmap < 0 || surf.kind != brush::SurfaceKind::planar) continue;
    for (uint32_t t = surf.first; t + 2 < surf.first + surf.count; t += 3) {
      const brush::Vertex& a = map.vertices[map.indices[t]];
      const brush::Vertex& b = map.vertices[map.indices[t + 1]];
      const brush::Vertex& c = map.vertices[map.indices[t + 2]];
      // The line straight down meets the triangle where (x, y) is inside it seen from above.
      const double d = (double(b.pos[1]) - c.pos[1]) * (double(a.pos[0]) - c.pos[0]) +
                       (double(c.pos[0]) - b.pos[0]) * (double(a.pos[1]) - c.pos[1]);
      if (std::abs(d) < 1e-9) continue;  // a wall: seen edge-on from above
      const double w0 = ((double(b.pos[1]) - c.pos[1]) * (double(at[0]) - c.pos[0]) +
                         (double(c.pos[0]) - b.pos[0]) * (double(at[1]) - c.pos[1])) /
                        d;
      const double w1 = ((double(c.pos[1]) - a.pos[1]) * (double(at[0]) - c.pos[0]) +
                         (double(a.pos[0]) - c.pos[0]) * (double(at[1]) - c.pos[1])) /
                        d;
      const double w2 = 1 - w0 - w1;
      if (w0 < 0 || w1 < 0 || w2 < 0) continue;
      const double z = w0 * a.pos[2] + w1 * b.pos[2] + w2 * c.pos[2];
      if (z > at[2] + 1e-3 || z <= best) continue;
      best = z;
      hit = true;
      luxel_rgb(map, surf.lightmap, w0 * a.lm[0] + w1 * b.lm[0] + w2 * c.lm[0],
                w0 * a.lm[1] + w1 * b.lm[1] + w2 * c.lm[1], rgb);
    }
  }
  if (!hit) return probe;
  if (found) *found = true;
  if (floor_z) *floor_z = float(best);
  // A luxel of 128 is the surface as painted: 1 in the light unit. Most of it from every way,
  // the rest from above, so the top of the gun reads a little brighter than its belly.
  for (size_t c = 0; c < 3; c++) {
    const auto l = float(rgb[c] / 128.0);
    probe.ambient[c] = 0.7f * l;
    probe.direct[c] = 0.45f * l;
  }
  return probe;
}

viewmodel::PointLight muzzle_flash_light(const double muzzle[3]) {
  viewmodel::PointLight l;
  for (size_t k = 0; k < 3; k++) l.position[k] = muzzle[k];
  l.colour[0] = 1.0f;
  l.colour[1] = 0.8f;
  l.colour[2] = 0.55f;
  l.intensity = 0.6f;
  l.radius_m = 0.3f;
  l.range_m = float(200 * brush::kUnitM);  // the classic flash reaches 200 units
  return l;
}

std::vector<viewmodel::PointLight> flame_lights(const brush::Map& map) {
  std::vector<viewmodel::PointLight> out;
  for (const brush::Entity& e : map.entities) {
    const std::string& cls = e.get("classname");
    if (cls.rfind("light_torch", 0) != 0 && cls.rfind("light_flame", 0) != 0) continue;
    float o[3];
    if (!parse_origin(e.get("origin"), o)) continue;
    double level = 200;  // the classic default for a light with no `light` key
    if (const std::string& s = e.get("light"); !s.empty()) {
      double v = 0;
      if (std::sscanf(s.c_str(), "%lf", &v) == 1 && v > 0) level = v;
    }
    viewmodel::PointLight l;
    brush::to_engine(o, l.position);
    l.colour[0] = 1.0f;
    l.colour[1] = 0.62f;
    l.colour[2] = 0.3f;
    // The baked light falls off over `level` units; a metre off it is about (level - 40) / 128.
    l.intensity = float(level / 150.0);
    l.range_m = float(level * brush::kUnitM);
    l.radius_m = 0.3f;
    out.push_back(l);
  }
  return out;
}

std::span<const ClassicFire> classic_fires() {
  // The shotguns throw a short wide flash, the nailguns a quick small jet, the grenade launcher a
  // thump and the rocket launcher a long jet; the rates are the classic game's.
  static constexpr ClassicFire kFires[] = {
      {"v_axe", "", 0.5},
      {"v_shot", "muzzle.shotgun", 0.5},
      {"v_shot2", "muzzle.shotgun", 0.7},
      {"v_nail", "muzzle.nailgun", 0.2},
      {"v_nail2", "muzzle.nailgun", 0.2},
      {"v_rock", "muzzle.grenade", 0.6},
      {"v_rock2", "muzzle.rocket", 0.8},
      {"v_light", "", 0.1},
  };
  return kFires;
}

const ClassicFire* classic_fire_of(std::string_view path) {
  std::string_view name =
      path.substr(path.rfind('/') == std::string_view::npos ? 0 : path.rfind('/') + 1);
  if (name.size() > 4 && name.substr(name.size() - 4) == ".mdl") name.remove_suffix(4);
  for (const ClassicFire& f : classic_fires()) {
    if (f.weapon == name) return &f;
  }
  return nullptr;
}

void classic_fire_frame(int frames, double since_s, int* a, int* b, double* t) {
  *a = *b = 0;
  *t = 0;
  if (frames < 2 || since_s < 0) return;
  const double at = since_s / kFrameSeconds;
  const auto k = int(at);
  if (k >= frames - 1) return;  // the firing frames are played out: at rest
  *a = 1 + k;
  *b = *a + 1 < frames ? *a + 1 : 0;
  *t = at - k;
}

std::vector<PackWeapon> pack_weapons(std::span<const uint8_t> pack,
                                     std::vector<std::string>* skipped) {
  std::vector<PackWeapon> out;
  vfs::Pak pak;
  std::string error;
  if (!vfs::read_pak(pack, &pak, &error)) {
    if (skipped) skipped->push_back(error);
    return out;
  }
  for (const vfs::PakEntry& e : pak.entries) {
    const std::string& p = e.path;
    if (p.rfind("progs/v_", 0) != 0 || p.size() < 4 || p.substr(p.size() - 4) != ".mdl") continue;
    PackWeapon w;
    w.name = p;
    if (!morph::read_model(vfs::pak_file(pack, e), p, &w.model, &error)) {
      if (skipped) skipped->push_back(error);
      continue;
    }
    out.push_back(std::move(w));
  }
  std::sort(out.begin(), out.end(),
            [](const PackWeapon& a, const PackWeapon& b) { return a.name < b.name; });
  return out;
}

}  // namespace ge::classicenhance
