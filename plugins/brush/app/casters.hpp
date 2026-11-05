// THE MOVING CASTERS' CHOICES (bsp_draw.hpp's caster cubes and capsules, v2 lit in real time): the
// models a frame hands over (players, monsters, the first-person player who is not drawn) shadow
// the kCasterLights shadowed lights that light one of them most through a cube of their own each,
// and every other light through the kShadowCapsules of them nearest the eye as capsules. Which
// lights and which capsules, worked on the CPU each frame; bsp.wgsl and capsules.wgsl read them.
//
// ge:summary The moving casters' lights and capsules: the lights that light them most, the capsules
// ge:keywords bsp brush model-shadows caster-cubes caster_lights shadow_capsules capsule-shadow
// ge:keywords moving-casters set_shadow_casters
// ge:skill brush render
// ge:test tests/brush_bsp_casters_test.cpp
#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "brush/app/light_grid.hpp"

namespace ge::brush::app {

inline constexpr int kCasterLights = 8;  // the lights with a casters' cube a frame
inline constexpr int kShadowCapsules = 4;
// A caster this far from the eye counts half as much when the lights are dealt their cubes, m.
inline constexpr double kCasterSeenM = 4;

// A moving caster's box, world m.
struct CasterBox {
  float lo[3], hi[3];
};

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

}  // namespace ge::brush::app
