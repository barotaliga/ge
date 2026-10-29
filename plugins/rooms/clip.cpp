// The clip world: prisms built flat, caves as grown, brushes met, every triangle handed to the
// mesh world.
//
// ge:summary The room level's mover world (flat prisms, grown caves) and a point's room
// ge:keywords rooms clip collision mesh-world flat relief cave room_at point-in-polygon
// ge:skill rooms move
// ge:test tests/rooms_clip_test.cpp
#include "rooms/clip.hpp"

#include <vector>

#include "core/parallel.hpp"
#include "rooms/brush_build.hpp"
#include "rooms/build.hpp"

namespace ge::rooms {

namespace {

// `level` with only the brushes `keep` takes (their faces carried along), its rooms kept when
// `rooms` is set.
Level with_brushes(const Level& level, bool rooms, uint16_t group, bool only_group) {
  Level out;
  out.materials = level.materials;
  out.projections = level.projections;
  if (rooms) out.rooms = level.rooms;
  for (const Brush& b : level.brushes) {
    if (only_group ? b.group != group : b.group != kWorld) continue;
    Brush c = b;
    c.first_face = uint32_t(out.faces.size());
    out.faces.insert(out.faces.end(), level.faces.begin() + b.first_face,
                     level.faces.begin() + b.first_face + b.faces);
    out.brushes.push_back(c);
  }
  return out;
}

void brush_corners(const Level& level, size_t workers, std::vector<move::Vec3>& corners) {
  if (level.brushes.empty()) return;
  const BrushMesh bm = build_brushes(level, workers);
  for (size_t t = 0; t < bm.triangles(); ++t) {
    const Contents c = bm.contents[t];
    if (c != Contents::solid && c != Contents::sky && c != Contents::clip) continue;
    for (size_t k = 0; k < 3; ++k) {
      const float* p = &bm.pos[size_t(bm.idx[t * 3 + k]) * 3];
      corners.push_back({double(p[0]), double(p[1]), double(p[2])});
    }
  }
}

}  // namespace

move::MeshWorld clip_group(const Level& level, uint16_t group, size_t workers, double cell_m) {
  std::vector<move::Vec3> corners;
  brush_corners(with_brushes(level, false, group, true), workers, corners);
  move::MeshWorld w = move::build_mesh_world(corners, cell_m);
  w.skin = 1.0 / 1024.0;
  return w;
}

move::MeshWorld clip_world(const Level& level, size_t workers, double cell_m, bool groups) {
  Level flat = level;
  for (Material& m : flat.materials) m.relief = Relief::none;
  std::vector<Mesh> meshes(level.rooms.size());
  core::parallel_bands(meshes.size(), workers, [&](size_t begin, size_t end) {
    for (size_t r = begin; r < end; ++r) {
      meshes[r] = build_room(level.rooms[r].shape == Shape::cave ? level : flat, r);
    }
  });
  std::vector<move::Vec3> corners;
  size_t count = 0;
  for (const Mesh& m : meshes) count += m.idx.size();
  corners.reserve(count);
  for (const Mesh& m : meshes) {
    for (const uint32_t i : m.idx) {
      const float* p = &m.pos[size_t(i) * 3];
      corners.push_back({double(p[0]), double(p[1]), double(p[2])});
    }
  }
  // The brushes a body meets: solid, sky and clip, hidden faces too; a liquid is swum, not met.
  brush_corners(groups ? level : with_brushes(level, false, kWorld, false), workers, corners);
  move::MeshWorld w = move::build_mesh_world(corners, cell_m);
  // A millimetre: a walker blocked by a step's riser climbs it only once a step's move from rest
  // carries it past the skin, so the skin is kept far under one step's creep.
  w.skin = 1.0 / 1024.0;
  return w;
}

int room_at(const Level& level, const move::Vec3& p) {
  for (size_t r = 0; r < level.rooms.size(); ++r) {
    const Room& room = level.rooms[r];
    const double y0 = metres(int64_t(room.origin[1]) + room.floor_y);
    const double y1 = metres(int64_t(room.origin[1]) + room.ceil_y);
    if (p.y < y0 || p.y > y1) continue;
    // Crossings of a ray along +x with the footprint's edges: odd is inside.
    bool inside = false;
    const size_t n = room.corners.size();
    for (size_t i = 0; i < n; ++i) {
      double a[2], b[2];
      corner_m(room, i, a);
      corner_m(room, (i + 1) % n, b);
      if ((a[1] > p.z) != (b[1] > p.z)) {
        const double x = a[0] + (p.z - a[1]) * (b[0] - a[0]) / (b[1] - a[1]);
        if (x > p.x) inside = !inside;
      }
    }
    if (inside) return int(r);
  }
  return -1;
}

}  // namespace ge::rooms
