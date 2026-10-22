// MESHES ON THE CPU, AND THEIR COMPACT FORM (docs/rendering.md § Geometry and resources): a
// `MeshData` is built at load from a game's seeded data with the shape builders here (faceted:
// every face has its own vertices and its own normal, so a low-poly shape reads as cut), then
// packed into the vertex-pulling format every instanced draw reads.
//
// THE PACKED VERTEX is three 32-bit words (12 bytes, against 28 for floats):
//   word 0: x | y << 16   (16-bit unorm positions inside the mesh's box)
//   word 1: z | part << 16 (the part picks the instance's material: material + part)
//   word 2: the normal, octahedral, two 16-bit snorms (x | y << 16)
// `unpack_vertex` is the CPU twin of the shaders' decode (render/wgsl/mesh.wgsl), and the test
// holds the two to each other's rounding.
//
// WINDING: a triangle's front is the side its (b - a) x (c - a) points to, the world's own cross
// product. The world is left-handed (view.hpp), so on screen a front face winds clockwise; the
// instanced pipelines cull the back faces with that front.
//
// ge:summary CPU meshes: faceted shape builders, the bounding sphere and the packed vertex
// ge:keywords render mesh mesh-data vertex-pulling packed-vertex octahedral-normal unorm16
// ge:keywords add_box add_cylinder add_ellipsoid pack_mesh unpack_vertex bounding-sphere part
// ge:skill render
// ge:test tests/render_mesh_test.cpp
#pragma once

#include <cstdint>
#include <vector>

namespace ge::render {

struct MeshData {
  std::vector<float> positions;  // xyz a vertex, m
  std::vector<float> normals;    // xyz a vertex, unit
  std::vector<uint8_t> parts;    // a vertex's part (its material's offset)
  std::vector<uint32_t> indices;
  uint32_t vertex_count() const { return static_cast<uint32_t>(parts.size()); }
};

// One flat triangle (three new vertices, its face's normal).
void add_triangle(MeshData& m, const float a[3], const float b[3], const float c[3], uint8_t part);
// A box of half sizes `half` round `centre`.
void add_box(MeshData& m, const float centre[3], const float half[3], uint8_t part);
// A cylinder (or a cone, a frustum) along +y from y0 to y1, radius r0 at y0 and r1 at y1, of
// `segments` faces round; a cap closes each end of non-zero radius.
void add_cylinder(MeshData& m, float y0, float y1, float r0, float r1, int segments, uint8_t part);
// An ellipsoid of radii `radii` round `centre`: `rings` bands from pole to pole, `segments` round.
void add_ellipsoid(MeshData& m, const float centre[3], const float radii[3], int rings,
                   int segments, uint8_t part);

// The smallest sphere round the box of the mesh's vertices that holds them all: centre xyz, r.
void bounding_sphere(const MeshData& m, float out[4]);

struct PackedMesh {
  float box_min[3] = {0, 0, 0};
  float box_size[3] = {0, 0, 0};  // the box's extent; a flat axis gets 1e-6
  std::vector<uint32_t> words;    // three a vertex
  std::vector<uint32_t> indices;
};
PackedMesh pack_mesh(const MeshData& m);
// Packed into a given box (positions outside it are clamped to its walls).
PackedMesh pack_mesh(const MeshData& m, const float box_min[3], const float box_size[3]);
// The vertex `i` of a packed mesh as the shaders decode it.
void unpack_vertex(const PackedMesh& p, uint32_t i, float pos[3], float normal[3], uint32_t* part);

}  // namespace ge::render
