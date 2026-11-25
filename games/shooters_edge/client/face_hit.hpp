// The face a line meets in a classic map, and its surface as `$cl_surface` says it.
//
// THE FACE A ROUND HIT: the map's triangle the line from `from` through `at` (map units) crosses
// within a few units of `at`, among the surfaces marked in the leaf just before it: its normal
// toward `from` (map axes, unit), its texture's surface class and its baked light there (its own
// lightmap's first style, 0..1 a channel; all -1 when it has none) and its texture coordinates
// there (Vertex::st's kind). False when no triangle is near.
//
// `surface_words` is `$cl_surface`'s text: the texture's name and its surface flags (`0x` and
// hexadecimal, a plain `0` for none).
//
// ge:summary The face a line meets in a classic map: its texture, normal, light; $cl_surface's text
// ge:keywords face_hit cl_surface surface-flags texture-under-crosshair ray-triangle
// ge:skill shooter
// ge:test tests/shooters_edge_place_names_test.cpp
#pragma once

#include <cstdint>
#include <string>

#include "brush/map.hpp"
#include "detail/surfaces.hpp"
#include "shooter/vec.hpp"

namespace shooters_edge::client {

bool face_hit(const ge::brush::Map& m, const ge::shooter::Vec3f& from, const ge::shooter::Vec3f& at,
              uint32_t* texture, ge::shooter::Vec3f* n, ge::detail::SurfaceClass* surface,
              float light[3], float st[2]);

// `$cl_surface` for a line from `from` that the solids stop at `solid_end`: the first liquid on
// the way stops it sooner; the face there named by `surface_words`, " 0" when none is near.
std::string surface_ahead(const ge::brush::Map& m, const ge::shooter::Vec3f& from,
                          const ge::shooter::Vec3f& solid_end);

// The texture's name and flags as `$cl_surface` writes them ("e1u1/floor1_3 0x1", "name 0").
std::string surface_words(const ge::brush::Map& m, uint32_t texture);

}  // namespace shooters_edge::client
