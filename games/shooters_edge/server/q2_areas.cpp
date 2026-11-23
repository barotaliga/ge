// Quake II's areas: the map's areas flooded over the open area portals, and hearing through them
// (q2_map.hpp).
//
// ge:summary Quake II's areas joined through open portals: a shut door cuts hearing between rooms
// ge:keywords q2 area area-portal areas-connected hearing phs doors-block-hearing map_noareas
// ge:skill shooter brush
// ge:test tests/shooters_edge_q2_map_test.cpp
#include "brush/areas.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace br = ge::brush;

namespace {

const std::vector<int32_t>& groups(const Q2Server& s) {
  if (s.flooded_map != s.map.get() || s.flooded_portals != s.portals) {
    s.flooded_map = s.map.get();
    s.flooded_portals = s.portals;
    if (s.map)
      br::flood_areas(*s.map, s.portals, &s.area_groups);
    else
      s.area_groups.clear();
  }
  return s.area_groups;
}

// map_noareas: every area joined to every other, the portals ignored (a debugging aid).
bool no_areas(const Q2Server& s) {
  const auto* v = s.cvars.find("map_noareas");
  return v != nullptr && v->number != 0;
}

int32_t area_of(const br::Map& m, sh::Vec3f p) {
  const float at[3] = {p.x, p.y, p.z};
  return br::area_at(m, at);
}

}  // namespace

bool map_areas_connected(const Q2Server& s, sh::Vec3f a, sh::Vec3f b) {
  if (!s.map || s.map->areas.empty() || no_areas(s)) return true;
  return br::areas_connected(groups(s), area_of(*s.map, a), area_of(*s.map, b));
}

bool map_hears(const Q2Server& s, sh::Vec3f from, sh::Vec3f to) {
  if (!s.map || s.map->leaves.empty()) return true;
  const float a[3] = {from.x, from.y, from.z}, b[3] = {to.x, to.y, to.z};
  const int32_t ca = s.map->leaves[br::leaf_at(*s.map, a)].cluster;
  const int32_t cb = s.map->leaves[br::leaf_at(*s.map, b)].cluster;
  if (!s.map->phs.empty() && !br::cluster_hears(*s.map, ca, cb)) return false;
  return map_areas_connected(s, from, to);
}

const std::vector<int32_t>& map_area_groups(const Q2Server& s) {
  static const std::vector<int32_t> kNone;
  return s.map && !s.map->areas.empty() && !no_areas(s) ? groups(s) : kNone;
}

void map_area_bits(const Q2Server& s, sh::Vec3f eye, std::vector<uint8_t>& out) {
  out.clear();
  if (!s.map || s.map->areas.empty()) return;
  const size_t n = s.map->areas.size();
  const int32_t from = area_of(*s.map, eye);
  out.assign((n + 7) / 8, from == 0 || no_areas(s) ? 0xff : 0);
  if (from == 0 || no_areas(s)) return;
  const std::vector<int32_t>& g = groups(s);
  for (size_t a = 1; a < n; ++a)
    if (br::areas_connected(g, from, int32_t(a))) out[a / 8] |= uint8_t(1u << (a % 8));
}

}  // namespace shooters_edge::server
