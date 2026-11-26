// THE MOVING CASTERS' CHOICES (bsp_draw.hpp's caster cubes and capsules, v2 lit in real time): the
// models a frame hands over (players, monsters, the first-person player who is not drawn) shadow
// the kCasterLights shadowed lights that light one of them most through a cube of their own each,
// and every other light through the kShadowCapsules of them nearest the eye as capsules. Which
// lights and which capsules, worked on the CPU each frame; bsp.wgsl and capsules.wgsl read them.
//
// ge:summary The moving casters' lights and capsules: the lights that light them most, the capsules
// ge:keywords bsp brush model-shadows caster-cubes caster_lights shadow_capsules capsule-shadow
// ge:keywords moving-casters set_shadow_casters caster_cones blocker-search box_in_face
// ge:skill brush render
// ge:test tests/brush_bsp_casters_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "brush/app/light_grid.hpp"

namespace ge::brush::app {

inline constexpr int kCasterLights = 8;  // the lights with a casters' cube a frame
inline constexpr int kShadowCapsules = 4;
// A caster this far from the eye counts half as much when the lights are dealt their cubes, m.
inline constexpr double kCasterSeenM = 4;

// A moving caster's box, world m, and its triangles' vertices in the casters' list (count 0:
// not known, drawn into every face).
struct CasterBox {
  float lo[3], hi[3];
  uint32_t first = 0, count = 0;
};

// Whether any of box `b` could fall in face `face` (cube_uv's order: +x, -x, +y, -y, +z, -z) of
// the cube of the light at `pos` with reach `reach`: a test that never leaves out a box that
// reaches the face's quarter of the ways within the reach, so the faces drawn without the rest
// keep every texel.
bool box_in_face(const CasterBox& b, const float pos[3], float reach, int face);

// The shadowed lights (indices into `lights`, each with a world cube: info[0] >= 0) that get a
// casters' cube, at most kCasterLights, the first the most: each light's most light on one caster
// (its strength over the distance squared to the box, inside its reach), a caster's counting the
// more the nearer it is to `eye` (the shadows seen, the player's own first). A light that reaches
// no caster gets none.
std::vector<size_t> caster_lights(std::span<const GpuLight> lights,
                                  std::span<const size_t> shadowed,
                                  std::span<const CasterBox> boxes, const double eye[3]);

// A caster as a vertical capsule: the ends of its axis and its radius, world m.
struct Capsule {
  double a[3], b[3];
  double r = 0;
};
// The kShadowCapsules casters nearest `eye` (their boxes' middles), nearest first, each its box's
// vertical capsule: the radius a quarter of the box's mean width, the axis up the middle from a
// radius over its floor to a radius under its top.
std::vector<Capsule> shadow_capsules(std::span<const CasterBox> boxes, const double eye[3]);

// THE CASTERS SEEN FROM A LIGHT WITH THEIR CUBE: the ways from the light the casters within its
// reach could hide a point along, as at most kCasterCones cones, so the soft shadow's blocker
// search (bsp.wgsl's soft_shadow) is skipped for a point whose way lies outside all of them, and
// for one nearer the light than any caster. A cone is its axis and the cosine of its half-angle
// widened by `widen` radians (the search's reach round the point's way); w 2 marks no cone (no way
// inside), -2 every way (the light inside a caster's sphere). Each box counts as its bounding
// sphere; past kCasterCones of them, the two cones whose union is narrowest merge. The skip changes
// no pixel: outside the cones the search would find no blocker.
inline constexpr int kCasterCones = 4;
struct CasterCones {
  float cone[kCasterCones][4];
  float nearest = 0;  // the nearest any caster comes to the light, m (its reach: none in it)
};
CasterCones caster_cones(const GpuLight& light, std::span<const CasterBox> boxes, double widen);

}  // namespace ge::brush::app
