// A map's intent drafted from its items (intent_draft.hpp).
//
// ge:summary A map's intent drafted from its items: an area a room round each power and weapon
// ge:keywords intent draft-intent auto-intent area camp hold power fight room flood-fill slice
// ge:skill map-design shooter
// ge:test tests/shooter_intent_draft_test.cpp
#include "shooter/intent_draft.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <map>
#include <string_view>
#include <utility>
#include <vector>

#include "brush/survey.hpp"
#include "core/num_text.hpp"

namespace ge::shooter {

namespace {

struct Kind {
  std::string_view classname, name, intent, what;
};

// Camp, hold, power, fight: the order the intents rank in when two items share a room.
constexpr Kind kKinds[] = {
    {"item_armorInv", "ra", "camp", "the red armour"},
    {"item_armor_body", "ra", "camp", "the body armour"},
    {"item_armor2", "ya", "hold", "the yellow armour"},
    {"item_armor_combat", "ya", "hold", "the combat armour"},
    {"item_artifact_super_damage", "quad", "power", "the quad damage"},
    {"item_quad", "quad", "power", "the quad damage"},
    {"item_artifact_invulnerability", "pent", "power", "the pentagram"},
    {"item_invulnerability", "pent", "power", "the invulnerability"},
    {"item_artifact_invisibility", "ring", "power", "the ring"},
    {"item_health_mega", "mh", "power", "the mega health"},
    {"mega", "mh", "power", "the mega health"},  // item_health with spawnflags 2
    {"weapon_rocketlauncher", "rl", "fight", "the rocket launcher"},
    {"weapon_lightning", "lg", "fight", "the lightning gun"},
    {"weapon_railgun", "rg", "fight", "the railgun"},
};

int rank(std::string_view intent) {
  if (intent == "camp") return 0;
  if (intent == "power") return 1;
  if (intent == "hold") return 2;
  return 3;
}

struct Box {
  double x0, y0, x1, y1;
};

double overlap(const Box& a, const Box& b) {
  const double ix = std::max(0.0, std::min(a.x1, b.x1) - std::max(a.x0, b.x0));
  const double iy = std::max(0.0, std::min(a.y1, b.y1) - std::max(a.y0, b.y0));
  const double small = std::min((a.x1 - a.x0) * (a.y1 - a.y0), (b.x1 - b.x0) * (b.y1 - b.y0));
  return small > 0 ? ix * iy / small : 0;
}

// The room round (x, y) on the cut at height `z`: the open cells joined to the item's, out to
// `reach` each way, as a box; a 256-unit box when the item's cell is not open.
Box room(const ge::brush::Map& map, double x, double y, double z, double reach,
         std::map<long, ge::brush::Slice>& cuts) {
  constexpr double kStep = 16;
  const Box none{x - 128, y - 128, x + 128, y + 128};
  const long key = std::lround(z + 24);
  auto it = cuts.find(key);
  if (it == cuts.end()) {
    ge::brush::Slice s;
    std::string error;
    if (!ge::brush::slice_map(map, 2, double(key), kStep, &s, &error)) s.rows.clear();
    it = cuts.emplace(key, std::move(s)).first;
  }
  const ge::brush::Slice& s = it->second;
  if (s.rows.empty()) return none;
  const long rows = long(s.rows.size()), cols = long(s.rows[0].size());
  const long c0 = std::lround((x - s.u0) / kStep), r0 = std::lround((s.v0 - y) / kStep);
  const auto open = [&](long r, long c) {
    if (r < 0 || r >= rows || c < 0 || c >= cols) return false;
    const char ch = s.rows[size_t(r)][size_t(c)];
    return ch == '.' || ch == '~';
  };
  if (!open(r0, c0)) return none;
  const long lim = long(reach / kStep);
  std::vector<char> seen(size_t(rows * cols), 0);
  std::deque<std::pair<long, long>> q{{r0, c0}};
  seen[size_t(r0 * cols + c0)] = 1;
  long rmin = r0, rmax = r0, cmin = c0, cmax = c0;
  while (!q.empty()) {
    const auto [r, c] = q.front();
    q.pop_front();
    rmin = std::min(rmin, r), rmax = std::max(rmax, r);
    cmin = std::min(cmin, c), cmax = std::max(cmax, c);
    constexpr long kD[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const auto& d : kD) {
      const long nr = r + d[0], nc = c + d[1];
      if (std::labs(nr - r0) > lim || std::labs(nc - c0) > lim || !open(nr, nc)) continue;
      char& v = seen[size_t(nr * cols + nc)];
      if (v == 0) {
        v = 1;
        q.push_back({nr, nc});
      }
    }
  }
  const double h = kStep / 2;
  return {s.u0 + double(cmin) * kStep - h, s.v0 - double(rmax) * kStep - h,
          s.u0 + double(cmax) * kStep + h, s.v0 - double(rmin) * kStep + h};
}

struct Area {
  std::string name, intent, what;
  double z;
  Box box;
};

}  // namespace

std::string draft_intent(const ge::brush::Map& map, const IntentDraftOptions& opt) {
  std::map<long, ge::brush::Slice> cuts;
  std::map<std::string, int> used;
  std::vector<Area> areas;
  for (const ge::brush::Entity& e : map.entities) {
    std::string cn = e.get("classname");
    if (cn == "item_health" && e.get("spawnflags") == "2") cn = "mega";
    const Kind* k = nullptr;
    for (const Kind& c : kKinds)
      if (c.classname == cn) k = &c;
    double x = 0, y = 0, z = 0;
    if (k == nullptr || std::sscanf(e.get("origin").c_str(), "%lf %lf %lf", &x, &y, &z) != 3)
      continue;
    const int n = ++used[std::string(k->name)];
    std::string name(k->name);
    if (n > 1) name += std::to_string(n);
    const Box b = room(map, x, y, z, opt.reach, cuts);
    bool merged = false;
    for (Area& a : areas) {
      if (overlap(a.box, b) > 0.6 && std::fabs(a.z - z) < 96) {
        a.what += " and " + std::string(k->what);
        if (rank(k->intent) < rank(a.intent)) a.intent = k->intent, a.name = name;
        merged = true;
        break;
      }
    }
    if (!merged) areas.push_back({name, std::string(k->intent), std::string(k->what), z, b});
  }
  std::string out =
      "# Drafted from the map's items (ge::shooter::draft_intent): one area a room round a power\n"
      "# item or a key weapon. A draft: check each box and intent on the overview, add the\n"
      "# middle's fights and the routes the map is known for.\n";
  for (const Area& a : areas) {
    const auto n = [](double v) { return ge::core::num::to_fixed(v, 0); };
    out += "area " + a.name + " z=" + n(a.z - 40) + ".." + n(a.z + 200) + " box " + n(a.box.x0) +
           "," + n(a.box.y0) + " " + n(a.box.x1) + "," + n(a.box.y1) + " intent=" + a.intent +
           " note=\"" + a.what + "\"\n";
  }
  return out;
}

}  // namespace ge::shooter
