// engine/render/mesh.hpp: every shape builder's faces wind outward (the cross product of each
// triangle points away from the shape's middle, the front face the instanced pipelines keep);
// the bounding sphere holds every vertex; the packed vertex comes back within its 16-bit steps,
// its normal within a hundredth of a degree's worth of cosine, and its part as written.
//
// ge:summary Tests for CPU meshes: outward winding, bounding spheres, the packed vertex
// ge:keywords test render mesh winding bounding-sphere pack unpack octahedral
// ge:skill render testing
#include <cmath>

#include "check.hpp"
#include "render/mesh.hpp"

namespace {

namespace r = ge::render;

// Every triangle's normal points away from `mid` (its centroid minus mid along the normal > 0).
int inward_faces(const r::MeshData& m, const float mid[3]) {
  int bad = 0;
  for (size_t t = 0; t + 2 < m.indices.size(); t += 3) {
    float c[3] = {0, 0, 0};
    for (int k = 0; k < 3; ++k) {
      for (int a = 0; a < 3; ++a) c[a] += m.positions[m.indices[t + k] * 3 + a] / 3.0f;
    }
    const float* n = &m.normals[m.indices[t] * 3];
    float d = 0;
    for (int a = 0; a < 3; ++a) d += (c[a] - mid[a]) * n[a];
    if (!(d > 0)) ++bad;
  }
  return bad;
}

}  // namespace

TEST(shapes_wind_outward) {
  const float centre[3] = {1, 2, 3}, half[3] = {0.5f, 2.0f, 1.0f};
  r::MeshData box;
  r::add_box(box, centre, half, 0);
  CHECK(box.indices.size() == 36);
  CHECK(inward_faces(box, centre) == 0);

  r::MeshData cyl;
  r::add_cylinder(cyl, 0.0f, 2.0f, 1.0f, 0.5f, 7, 0);
  const float cyl_mid[3] = {0, 1, 0};
  CHECK(inward_faces(cyl, cyl_mid) == 0);

  r::MeshData cone;
  r::add_cylinder(cone, 0.0f, 3.0f, 1.0f, 0.0f, 5, 0);
  const float cone_mid[3] = {0, 1, 0};
  CHECK(inward_faces(cone, cone_mid) == 0);

  r::MeshData ball;
  const float radii[3] = {1.0f, 0.4f, 1.0f};
  r::add_ellipsoid(ball, centre, radii, 4, 7, 0);
  CHECK(inward_faces(ball, centre) == 0);
}

TEST(bounding_sphere_holds_every_vertex) {
  r::MeshData m;
  r::add_cylinder(m, -0.05f, 1.0f, 1.0f, 0.0f, 9, 1);
  const float c[3] = {0, 0.6f, 0}, radii[3] = {0.3f, 0.3f, 0.3f};
  r::add_ellipsoid(m, c, radii, 3, 5, 0);
  float s[4];
  r::bounding_sphere(m, s);
  for (size_t i = 0; i < m.positions.size(); i += 3) {
    const float dx = m.positions[i] - s[0], dy = m.positions[i + 1] - s[1],
                dz = m.positions[i + 2] - s[2];
    CHECK(std::sqrt(dx * dx + dy * dy + dz * dz) <= s[3]);
  }
  CHECK(s[3] < 1.2f);  // round the box, not round the origin
}

TEST(packed_vertex_round_trips) {
  r::MeshData m;
  const float centre[3] = {0, 0, 0}, radii[3] = {2.0f, 1.0f, 3.0f};
  r::add_ellipsoid(m, centre, radii, 6, 9, 3);
  const float half[3] = {0.25f, 0.25f, 0.25f}, at[3] = {0, 2, 0};
  r::add_box(m, at, half, 1);
  const r::PackedMesh p = r::pack_mesh(m);
  CHECK(p.words.size() == size_t{m.vertex_count()} * 3);
  CHECK(p.indices == m.indices);
  for (uint32_t i = 0; i < m.vertex_count(); ++i) {
    float pos[3], n[3];
    uint32_t part = 0;
    r::unpack_vertex(p, i, pos, n, &part);
    float dot = 0;
    for (int k = 0; k < 3; ++k) {
      CHECK(std::fabs(pos[k] - m.positions[i * 3 + k]) <= p.box_size[k] / 65535.0f + 1e-6f);
      dot += n[k] * m.normals[i * 3 + k];
    }
    CHECK(dot > 0.99999f);
    CHECK(part == m.parts[i]);
  }
}
