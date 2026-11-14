// LAB `se-remix`: CLASSIC REMIX'S ITEM SWAP ON REAL MAPS (games/shooters_edge/client/remix.hpp,
// profiles/remix.hpp). A map of each game (`--map` from the classic game's install `--game`,
// `--sequel-map` from the sequel's `--sequel`) is drawn from above, its floors shaded by height,
// twice: with its own items, and with every item made the other game's by the remix table, each
// item a dot coloured by what it is and named. A table counts each swap, so a reviewer sees that
// every weapon, ammunition, health, armour and powerup found a match by role and what was left
// out. Without installs it rewrites a made-up hall of both games' items (the smoke run).
//
// ge:summary Lab: classic remix's item table on a real map of each game, drawn from above
// ge:keywords lab remix classic-remix item-swap top-view map-items cr_items remix_entities
// ge:skill shooter labs
// ge:test tests/shooters_edge_remix_test.cpp
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "classicenhance/profile.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "progs/entity_text.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/remix.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
namespace ce = ge::classicenhance;

constexpr int kSide = 760;   // a panel's map area, pixels
constexpr int kMargin = 20;  // round it

// A made-up hall with both games' items, for the smoke run.
constexpr const char* kHall =
    "{\n\"classname\" \"worldspawn\"\n}\n"
    "{\n\"classname\" \"info_player_deathmatch\"\n\"origin\" \"0 0 24\"\n}\n"
    "{\n\"classname\" \"weapon_rocketlauncher\"\n\"origin\" \"128 0 0\"\n}\n"
    "{\n\"classname\" \"weapon_lightning\"\n\"origin\" \"256 0 0\"\n}\n"
    "{\n\"classname\" \"weapon_supernailgun\"\n\"origin\" \"384 0 0\"\n}\n"
    "{\n\"classname\" \"item_cells\"\n\"origin\" \"0 128 0\"\n}\n"
    "{\n\"classname\" \"item_rockets\"\n\"spawnflags\" \"1\"\n\"origin\" \"128 128 0\"\n}\n"
    "{\n\"classname\" \"item_health\"\n\"spawnflags\" \"2\"\n\"origin\" \"256 128 0\"\n}\n"
    "{\n\"classname\" \"item_armorInv\"\n\"origin\" \"384 128 0\"\n}\n"
    "{\n\"classname\" \"item_artifact_super_damage\"\n\"origin\" \"0 256 0\"\n}\n";

struct Spot {
  std::string cls;
  float x = 0, y = 0;
};

std::string value_of(const ge::progs::EntityBlock& b, const char* key) {
  for (const auto& [k, v] : b.pairs)
    if (k == key) return v;
  return "";
}

std::vector<Spot> items_of(const std::string& text) {
  std::vector<ge::progs::EntityBlock> blocks;
  std::string error;
  std::vector<Spot> out;
  if (!ge::progs::parse_entity_text(text.c_str(), blocks, error)) return out;
  for (const auto& b : blocks) {
    const std::string cls = value_of(b, "classname");
    if (cls.rfind("weapon_", 0) != 0 && cls.rfind("item_", 0) != 0 && cls.rfind("ammo_", 0) != 0)
      continue;
    Spot s;
    s.cls = cls;
    float z = 0;
    std::sscanf(value_of(b, "origin").c_str(), "%f %f %f", &s.x, &s.y, &z);
    out.push_back(s);
  }
  return out;
}

// An item's colour by what it is.
gl::Rgba colour_of(const std::string& c) {
  if (c.rfind("weapon_", 0) == 0) return {205, 40, 40, 255};
  if (c.find("health") != std::string::npos || c == "item_adrenaline" || c == "item_ancient_head")
    return {40, 160, 60, 255};
  if (c.find("armor") != std::string::npos || c.find("power_") != std::string::npos)
    return {40, 90, 210, 255};
  if (c.rfind("ammo_", 0) == 0 || c == "item_shells" || c == "item_spikes" || c == "item_rockets" ||
      c == "item_cells" || c == "item_bandolier" || c == "item_pack")
    return {210, 160, 20, 255};
  return {150, 50, 190, 255};  // powerups
}

// An item's short name: its class name without the family's word.
std::string short_name(const std::string& c) {
  for (const char* p : {"weapon_", "item_artifact_", "item_", "ammo_"})
    if (c.rfind(p, 0) == 0) return c.substr(std::string(p).size());
  return c;
}

struct Frame {
  float lo[2] = {0, 0}, hi[2] = {1, 1};
  double scale = 1;
  double px(float x) const { return kMargin + (x - lo[0]) * scale; }
  double py(float y) const { return kMargin + (hi[1] - y) * scale; }
};

void fill_triangle(gl::Picture& p, const double* x, const double* y, gl::Rgba c) {
  const int x0 = std::max(0, static_cast<int>(std::floor(std::min({x[0], x[1], x[2]}))));
  const int x1 = std::min(p.width - 1, static_cast<int>(std::ceil(std::max({x[0], x[1], x[2]}))));
  const int y0 = std::max(0, static_cast<int>(std::floor(std::min({y[0], y[1], y[2]}))));
  const int y1 = std::min(p.height - 1, static_cast<int>(std::ceil(std::max({y[0], y[1], y[2]}))));
  const double area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);
  if (std::abs(area) < 1e-9) return;
  for (int py = y0; py <= y1; py++)
    for (int px = x0; px <= x1; px++) {
      const double cx = px + 0.5, cy = py + 0.5;
      const double w0 = ((x[1] - cx) * (y[2] - cy) - (x[2] - cx) * (y[1] - cy)) / area;
      const double w1 = ((x[2] - cx) * (y[0] - cy) - (x[0] - cx) * (y[2] - cy)) / area;
      const double w2 = 1 - w0 - w1;
      if (w0 >= 0 && w1 >= 0 && w2 >= 0) gl::set_pixel(p, px, py, c);
    }
}

// The map's floors from above, shaded by height (higher lighter), drawn low to high.
gl::Picture plan_of(const ge::brush::Map* map, Frame& f, const std::vector<Spot>& spots) {
  if (map != nullptr && !map->models.empty()) {
    const auto& m = map->models[0];
    f.lo[0] = m.mins[0];
    f.lo[1] = m.mins[1];
    f.hi[0] = m.maxs[0];
    f.hi[1] = m.maxs[1];
  } else {
    f.lo[0] = f.lo[1] = 1e9f;
    f.hi[0] = f.hi[1] = -1e9f;
    for (const Spot& s : spots) {
      f.lo[0] = std::min(f.lo[0], s.x - 96);
      f.lo[1] = std::min(f.lo[1], s.y - 96);
      f.hi[0] = std::max(f.hi[0], s.x + 96);
      f.hi[1] = std::max(f.hi[1], s.y + 96);
    }
  }
  const double span = std::max(f.hi[0] - f.lo[0], f.hi[1] - f.lo[1]);
  f.scale = kSide / std::max(1.0, span);
  const int w = static_cast<int>((f.hi[0] - f.lo[0]) * f.scale) + 2 * kMargin;
  const int h = static_cast<int>((f.hi[1] - f.lo[1]) * f.scale) + 2 * kMargin;
  gl::Picture p = gl::create_picture(w, h, {28, 28, 32, 255});
  if (map == nullptr) return p;
  const auto& m = map->models[0];
  struct Tri {
    float z;
    std::array<uint32_t, 3> v;
  };
  std::vector<Tri> tris;
  for (uint32_t s = m.first_surface; s < m.first_surface + m.surface_count; s++) {
    const ge::brush::Surface& sf = map->surfaces[s];
    if (sf.kind == ge::brush::SurfaceKind::sky) continue;
    for (uint32_t i = sf.first; i + 2 < sf.first + sf.count; i += 3) {
      const std::array<uint32_t, 3> v{map->indices[i], map->indices[i + 1], map->indices[i + 2]};
      const auto& a = map->vertices[v[0]].pos;
      const auto& b = map->vertices[v[1]].pos;
      const auto& c = map->vertices[v[2]].pos;
      const float nz = (b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1]);
      const float nx = (b[1] - a[1]) * (c[2] - a[2]) - (b[2] - a[2]) * (c[1] - a[1]);
      const float ny = (b[2] - a[2]) * (c[0] - a[0]) - (b[0] - a[0]) * (c[2] - a[2]);
      const float len2 = nx * nx + ny * ny + nz * nz;
      if (len2 <= 0 || nz * nz < 0.49f * len2) continue;  // floors and ceilings face up or down
      tris.push_back({(a[2] + b[2] + c[2]) / 3, v});
    }
  }
  std::sort(tris.begin(), tris.end(), [](const Tri& a, const Tri& b) { return a.z < b.z; });
  const float zlo = m.mins[2], zhi = m.maxs[2];
  for (const Tri& t : tris) {
    double x[3], y[3];
    for (int k = 0; k < 3; k++) {
      x[k] = f.px(map->vertices[t.v[k]].pos[0]);
      y[k] = f.py(map->vertices[t.v[k]].pos[1]);
    }
    const double s = std::clamp((t.z - zlo) / std::max(1.0f, zhi - zlo), 0.0f, 1.0f);
    const auto g = static_cast<uint8_t>(70 + 150 * s);
    fill_triangle(p, x, y, {g, g, static_cast<uint8_t>(g + 8 > 255 ? 255 : g + 8), 255});
  }
  return p;
}

void mark(gl::Picture& p, const Frame& f, const std::vector<Spot>& spots) {
  for (const Spot& s : spots) {
    const double x = f.px(s.x), y = f.py(s.y);
    gl::fill_disc(p, x, y, 5, colour_of(s.cls));
    gl::draw_ring(p, x, y, 6, {255, 255, 255, 255});
    gl::draw_text(p, x + 8, y - 4, short_name(s.cls), {255, 255, 255, 255});
  }
}

// The two panels side by side with their headings.
gl::Picture pair(const gl::Picture& a, const gl::Picture& b, const std::string& left,
                 const std::string& right) {
  const int head = 22;
  gl::Picture p = gl::create_picture(a.width + b.width + 12, std::max(a.height, b.height) + head,
                                     {16, 16, 18, 255});
  gl::blit(p, a, 0, head);
  gl::blit(p, b, a.width + 12, head);
  gl::draw_text(p, 6, 6, left, {240, 240, 240, 255}, 2);
  gl::draw_text(p, a.width + 18, 6, right, {240, 240, 240, 255}, 2);
  return p;
}

// One map remixed: its two panels, its table rows and its metrics.
void remix_map(gl::Lab& lab, const std::string& id, const std::string& name,
               const std::string& text, const ge::brush::Map* map, const std::string& from,
               const std::string& to, std::vector<std::vector<std::string>>& rows) {
  std::string out, error;
  cl::RemixReport rep;
  if (!cl::remix_entities(text, from, to, out, &rep, error))
    return lab.fail(name + ": " + error,
                    "next: read the entity text's parse error; skill: shooter.");
  const std::vector<Spot> before = items_of(text), after = items_of(out);
  Frame f;
  gl::Picture a = plan_of(map, f, before);
  gl::Picture b = a;
  mark(a, f, before);
  mark(b, f, after);
  const ce::Profile* pf = ce::find_profile(from);
  const ce::Profile* pt = ce::find_profile(to);
  const std::string left = name + ": " + std::string(pf->what) + "'s items";
  const std::string right = "with " + std::string(pt->what) + "'s items";
  lab.shot({id,
            name + " WITH ITS ITEMS SWAPPED",
            "The map from above, floors shaded by height, with its own items (left) and with each "
            "made " +
                std::string(pt->what) +
                "'s by the remix table (right): weapons red, ammunition "
                "gold, health green, armour blue, powerups violet.",
            "every item on the left still has a dot on the right, a match by role (the nail "
            "weapons the bullet ones, the beam the rail), and only items with no match gone",
            {{"map", name}, {"from", from}, {"to", to}}},
           pair(a, b, left, right));
  lab.metric(id + "-items", rep.items, "items");
  lab.metric(id + "-swapped", rep.swapped, "items");
  lab.metric(id + "-dropped", rep.dropped, "items");
  lab.note(cl::remix_line(rep, to));
  for (const cl::RemixSwap& s : rep.swaps)
    rows.push_back({name, s.from, s.to.empty() ? "(left out)" : s.to, std::to_string(s.count)});
}

// The health a map's items give, in its own game's numbers.
int health_of(const std::string& text, const std::string& game) {
  std::vector<ge::progs::EntityBlock> blocks;
  std::string error;
  if (!ge::progs::parse_entity_text(text.c_str(), blocks, error)) return 0;
  int total = 0;
  for (const auto& b : blocks) {
    const std::string c = value_of(b, "classname");
    const int f = std::atoi(value_of(b, "spawnflags").c_str());
    if (game == "classic" && c == "item_health") total += (f & 2) ? 100 : (f & 1) ? 15 : 25;
    if (game == "sequel") {
      if (c == "item_health_small") total += 2;
      if (c == "item_health") total += 10;
      if (c == "item_health_large") total += 25;
      if (c == "item_health_mega") total += 100;
    }
  }
  return total;
}

// The weapons a map's text lays down, in its order, by their short names.
std::string weapons_of(const std::string& text) {
  std::string out;
  for (const Spot& s : items_of(text))
    if (s.cls.rfind("weapon_", 0) == 0) out += (out.empty() ? "" : " ") + short_name(s.cls);
  return out;
}

std::vector<std::string> split(const std::string& list) {
  std::vector<std::string> out;
  for (size_t at = 0; at <= list.size();) {
    const size_t comma = std::min(list.find(',', at), list.size());
    if (comma > at) out.push_back(list.substr(at, comma - at));
    at = comma + 1;
  }
  return out;
}

void run(gl::Lab& lab) {
  std::vector<std::vector<std::string>> rows, survey, unmatched, notes;
  const std::string q1 = lab.arg("game"), q2 = lab.arg("sequel");
  if (q1.empty() && q2.empty()) {
    remix_map(lab, "hall-to-sequel", "the made-up hall", kHall, nullptr, "classic", "sequel", rows);
  }
  struct Side {
    std::string dir, map, maps, from, to, id;
  };
  const Side sides[] = {
      {q1, lab.arg("map"), lab.arg("maps"), "classic", "sequel", "classic-map"},
      {q2, lab.arg("sequel-map"), lab.arg("sequel-maps"), "sequel", "classic", "sequel-map"}};
  for (const Side& s : sides) {
    if (s.dir.empty()) continue;
    const ce::Profile* content = ce::find_profile(s.from);
    cl::SearchPath files;
    std::string error;
    if (!cl::open_game_dir(s.dir, "", files, error, *content))
      return lab.fail(error, "next: pass the install folder that holds " +
                                 std::string(content->base_dir) + "; skill: shooter.");
    std::unique_ptr<cl::LoadedMap> m = cl::load_named_map(files, s.map, error);
    if (!m)
      return lab.fail(error, "next: name a map the install holds with --map or --sequel-map.");
    remix_map(lab, s.id, s.map, m->spec.entities, &m->bsp.map, s.from, s.to, rows);
    // The survey: every map named, weighed as a whole.
    for (const std::string& name : split(s.maps)) {
      std::unique_ptr<cl::LoadedMap> one = cl::load_named_map(files, name, error);
      if (!one) {
        lab.note(std::string(name).append(": ").append(error));
        continue;
      }
      std::string out, by_role;
      cl::RemixReport rep, plain;
      shooters_edge::profiles::RemixBalance off;
      off.strong_min = 0;
      off.strong_share = 1;
      off.pool_radius = 0;
      if (!cl::remix_entities(one->spec.entities, s.from, s.to, out, &rep, error) ||
          !cl::remix_entities(one->spec.entities, s.from, s.to, by_role, &plain, error, off)) {
        lab.note(std::string(name).append(": ").append(error));
        continue;
      }
      survey.push_back({name, s.from + " > " + s.to, weapons_of(one->spec.entities),
                        weapons_of(by_role), weapons_of(out),
                        std::to_string(rep.strong_by_role) + " > " + std::to_string(rep.strong) +
                            " of " + std::to_string(rep.weapons),
                        std::to_string(health_of(one->spec.entities, s.from)) + " > " +
                            std::to_string(health_of(out, s.to)),
                        std::to_string(rep.pooled) + " > " + std::to_string(rep.laid)});
      for (const cl::RemixSwap& w : rep.swaps)
        if (w.to.empty() && !shooters_edge::profiles::pool_item(s.from, s.to, w.from))
          unmatched.push_back({name, s.from, w.from, std::to_string(w.count)});
      for (const std::string& n : rep.notes) notes.push_back({name, n});
      lab.metric(name + "-strong", rep.strong, "weapons");
      lab.metric(name + "-health", health_of(out, s.to), "health");
    }
  }
  lab.table({"the remix table on each map", {"map", "item", "becomes", "count"}, rows});
  if (!survey.empty())
    lab.table({"each map weighed whole: its weapons, by role, balanced; strong ones; health",
               {"map", "games", "its weapons", "by role", "balanced", "strong", "health", "pooled"},
               survey});
  if (!unmatched.empty())
    lab.table(
        {"items with no match in the other game", {"map", "game", "item", "count"}, unmatched});
  if (!notes.empty()) lab.table({"the balance's steps", {"map", "step"}, notes});
}

const gl::LabSpec kSpec{
    "se-remix",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/client/remix and profiles/remix: classic remix's item swap",
    "shooter",
    "Does every item on a real map of each classic game find its match by role in the other "
    "game's set, and which are left out?",
    {
        {"game", "", "the classic game's install (the folder holding id1; empty: none)"},
        {"map", "dm3", "the classic game's map"},
        {"sequel", "", "the sequel's install (the folder holding baseq2; empty: none)"},
        {"sequel-map", "q2dm1", "the sequel's map"},
        {"maps", "dm2,dm3,dm4,dm6,e1m2,e2m1", "the classic game's maps weighed whole"},
        {"sequel-maps", "q2dm1,q2dm2,q2dm3,q2dm4,q2dm5,q2dm6,q2dm7,q2dm8",
         "the sequel's maps weighed whole"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
