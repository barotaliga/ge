// A map file as the mod server plays it (bsp_map.hpp).
//
// ge:summary Reads a map file into the mod server's MapSpec and answers its vis hook from the map
// ge:keywords bsp map read_bsp_map use_map_sets pvs phs vis cluster
// ge:skill shooter progs brush
// ge:test tests/shooters_edge_bsp_map_test.cpp
#include "shooters_edge/server/bsp_map.hpp"

#include "brush/read.hpp"

namespace shooters_edge::server {

namespace br = ge::brush;
namespace sh = ge::shooter;

namespace {

int32_t cluster_at(const br::Map& m, sh::Vec3f p) {
  const float at[3] = {p.x, p.y, p.z};
  return m.leaves[br::leaf_at(m, at)].cluster;
}

bool map_sets(void* ctx, sh::Vec3f from, sh::Vec3f to, bool see) {
  const br::Map& m = static_cast<const BspMap*>(ctx)->map;
  const int32_t a = cluster_at(m, from), b = cluster_at(m, to);
  return see ? br::cluster_sees(m, a, b) : br::cluster_hears(m, a, b);
}

}  // namespace

bool read_bsp_map(std::span<const uint8_t> bytes, const std::string& name, BspMap* out,
                  MapSpec* spec, std::string& error) {
  *out = BspMap{};
  std::string why;
  if (!br::read_bsp(bytes, &out->map, &why)) {
    error = "read_bsp_map: maps/" + name + ".bsp: " + why;
    return false;
  }
  if (!sh::bsp_world(out->map, &out->world, &why)) {
    error = "read_bsp_map: maps/" + name + ".bsp: " + why;
    return false;
  }
  br::build_phs(&out->map);
  spec->name = name;
  spec->models = out->world.models;
  spec->entities = out->map.entity_text;
  return true;
}

void use_map_sets(QcServer& s, const BspMap& map) {
  s.vis = &map_sets;
  s.vis_ctx = const_cast<BspMap*>(&map);
}

}  // namespace shooters_edge::server
