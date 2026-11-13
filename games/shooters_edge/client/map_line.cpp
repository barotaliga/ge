// ge:summary Walks a line through a map's world: the point hull, or the sequel's brushes
// ge:keywords map-line line-end trace_hull brush-tracer
// ge:skill shooter
// ge:test tests/shooters_edge_map_line_test.cpp
#include "shooters_edge/client/map_line.hpp"

#include "brush/trace.hpp"
#include "shooter/hull.hpp"
#include "shooter/sequel_move.hpp"

namespace shooters_edge::client {

namespace sh = ge::shooter;

sh::Vec3f line_end(const server::BspMap& map, sh::Vec3f from, sh::Vec3f to) {
  if (map.map.family != ge::brush::Family::v29 && map.map.family != ge::brush::Family::bsp2) {
    ge::brush::BrushTracer tracer(map.map);
    const float a[3] = {from.x, from.y, from.z}, b[3] = {to.x, to.y, to.z}, zero[3] = {0, 0, 0};
    const ge::brush::Trace t = tracer.trace(a, b, zero, zero, sh::kSqSolid | sh::kSqWindow);
    if (t.start_solid) return from;
    return {t.end[0], t.end[1], t.end[2]};
  }
  if (map.world.models.empty() || map.world.models[0].hulls[0].nodes == nullptr) return from;
  const sh::HullTrace t = sh::trace_hull(map.world.models[0].hulls[0], from, to);
  if (t.startsolid) return from;
  return t.endpos;
}

}  // namespace shooters_edge::client
