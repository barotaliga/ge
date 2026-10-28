// HOW THE HELD MODEL IS LIT: the first-person weapon takes the light of the place the player
// stands in, so it is dark in a dark corridor, warm beside a torch and flashes with its own shot.
// A game gathers the light at the player once a display frame; this file turns it into the view's
// frame and says how bright any point of the gun is.
//
// THE INPUTS, in the world's frame and in one linear unit (1: a white surface lit by it is white):
//   THE PROBE     what the world's baked light says at the player: an `ambient` colour from every
//                 way, and a `direct` colour arriving from `toward` (a unit vector pointing TO the
//                 light). A game with lightmaps samples the floor under the player (the surface
//                 the player stands on is lit the way the player is); a game with a light grid
//                 reads its nearest cell; a sunlit world gives the sky and the sun. Its own call.
//   POINT LIGHTS  lamps, torches, flames, a muzzle flash, a rocket going by: each at its world
//                 place, `colour` times `intensity` arriving at 1 m, falling off with the inverse
//                 square softened by the emitter's size (1 / (d^2 + radius^2)) and ending smoothly
//                 at `range_m`
//                 ((1 - (d/r)^4)^2), the same law the renderer's local lights use, so a lamp
//                 lights the gun as it lights the wall beside it.
//
// `light_held` keeps the kHeldLights lights that reach the gun hardest (at the grip's place,
// `at_view`, order fixed by strength then index), turns everything into the view's frame (+x
// right, +y up, +z forward, the eye at the origin) and adds the FLOOR: the least the gun is ever
// lit (a share of white), so a gun in the dark still reads as a silhouette with a little form.
//
// `shade_held` is the surface: the ambient from above a little more than from below, the direct
// and every point light by a wrapped Lambert (a soft terminator, as a small object reads under a
// big source), a Blinn highlight of `gloss` for the metal, at a point and its unit normal, in the
// view's frame. It is the CPU twin of the GPU pass's shader, and the labs draw with it.
//
// ge:summary How the held weapon is lit: the world's probe, point lights, a floor, the shading
// ge:keywords viewmodel view-model lighting light probe ambient direct lightmap light-grid
// ge:keywords point-light torch flame muzzle-flash dynamic-light inverse-square range window
// ge:keywords min-light floor wrap-lambert blinn gloss shade_held light_held held-lighting
// ge:skill render
// ge:test tests/viewmodel_light_test.cpp
#pragma once

#include <cstdint>
#include <span>

#include "camera/pose.hpp"

namespace ge::viewmodel {

inline constexpr int kHeldLights = 8;  // point lights on the gun at once

struct Probe {
  float ambient[3] = {0.25f, 0.25f, 0.25f};
  float direct[3] = {0, 0, 0};
  float toward[3] = {0, 1, 0};  // unit, world: toward the direct light
};

struct PointLight {
  double position[3] = {0, 0, 0};  // world, m
  float colour[3] = {1, 1, 1};     // linear, about unit luminance
  float intensity = 1;             // arriving at 1 m
  float range_m = 10;
  float radius_m = 0.05f;
};

struct HeldLight {
  float at[3];   // view, m
  float rgb[3];  // colour times intensity
  float range_m, radius_m;
};

struct Lighting {
  float ambient[3] = {0, 0, 0};
  float direct[3] = {0, 0, 0};
  float toward[3] = {0, 1, 0};  // view
  int count = 0;
  HeldLight lights[kHeldLights];
  float floor = 0.06f;  // the least light, share of white
  float gloss = 0.35f;  // the highlight's strength
  int shininess = 24;   // the highlight's tightness (an integer power)
};

// The world's light at the player into the view's frame, the strongest lights at `at_view` (the
// grip's place in the view) kept. `floor` is the least light (0..1 of white).
Lighting light_held(const Probe& probe, std::span<const PointLight> lights,
                    const camera::Pose& view, const float at_view[3], float floor = 0.06f);

// How much light reaches a point of the gun at `p` (view, m) with unit normal `n` (view): the
// diffuse factor to multiply the surface's colour by into `diffuse`, and the highlight to add on
// top into `specular` (linear rgb each).
void shade_held(const Lighting& l, const float p[3], const float n[3], float diffuse[3],
                float specular[3]);

// A world point or direction in the view's frame (+x right, +y up, +z forward, the eye at the
// origin), and a view point back in the world.
void world_to_view(const camera::Pose& view, const double world[3], float out[3]);
void world_to_view_dir(const camera::Pose& view, const float world[3], float out[3]);
void view_to_world(const camera::Pose& view, const float in[3], double out[3]);

// The fall-off of a point light at `d` m, the renderer's law: 1 / (d^2 + radius^2), times the
// smooth window to the range.
float light_falloff(float d, float range_m, float radius_m);

}  // namespace ge::viewmodel
