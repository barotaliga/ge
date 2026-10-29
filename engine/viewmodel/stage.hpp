// THE STAGE the labs hold a weapon up on: a stone corridor with a floor, walls, a ceiling and
// pillars, its lights by name, and one call that draws a first-person frame of it on the CPU (the
// room through the view's lens, then the gun through its own). Every view-model lab, the engine's
// and a plugin's, looks at its weapons here, so their pictures compare.
//
// THE LIGHTS by name (`StageLight`), each a probe at the player and the point lights round them,
// in the light unit of viewmodel/light.hpp:
//   day       a bright, cool ambient and a warm key from above and the left: an open, sunlit room.
//   dim       a low grey ambient: a corridor lit from far off.
//   dark      no light at all: the floor alone shows the gun.
//   torch     dim, with a flickering-orange torch on the left wall just ahead.
//   flash     dark, and a muzzle flash at the gun's muzzle: the shot lights its own gun.
//   coloured  dim, a red lamp on the left and a blue one on the right.
//
// ge:summary The labs' stage for a held weapon: a corridor, named lights, one first-person frame
// ge:keywords viewmodel lab stage corridor room scene lights day dim dark torch flash coloured
// ge:keywords draw_stage stage_lights corridor_stage
// ge:skill render labs
// ge:test tests/viewmodel_raster_test.cpp
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "camera/pose.hpp"
#include "viewmodel/held.hpp"
#include "viewmodel/light.hpp"
#include "viewmodel/place.hpp"

namespace ge::viewmodel {

// The room's triangles in the world (+z down the corridor, the floor at y 0), a colour a vertex.
struct Stage {
  std::vector<float> positions, normals, colour;
  std::vector<uint32_t> indices;
  double eye[3] = {0, 1.6, 0};
};
Stage corridor_stage();

enum class StageLight : uint8_t { day, dim, dark, torch, flash, coloured };
inline constexpr StageLight kStageLights[] = {StageLight::day,   StageLight::dim,
                                              StageLight::dark,  StageLight::torch,
                                              StageLight::flash, StageLight::coloured};
std::string_view stage_light_name(StageLight kind);

struct StageLights {
  Probe probe;
  std::vector<PointLight> lights;
  std::vector<float> sources;     // xyz a light the room shows as a small bright box (a torch)
  bool flare = false;             // a muzzle flash drawn at the muzzle over the gun
  float flare_at[3] = {0, 0, 0};  // view
  float floor = 0.06f;
};
// The lights `kind` names for a player at `view`; a flash sits at `muzzle_view` (the
// muzzle in the view's frame: the gun's placement of its tip).
StageLights stage_lights(StageLight kind, const camera::Pose& view, const float muzzle_view[3]);

// One first-person frame: the room through `view`, then the gun (frames `a`, `b` blended by `t`)
// placed by `place` through its own lens (`place.fov_y`, or the view's). RGBA8, sRGB.
struct StageShot {
  int width = 480, height = 270;
  int supersample = 2;
  double exposure = 1.0;
  int frame_a = 0, frame_b = 0;
  double frame_t = 0;
};
std::vector<uint8_t> draw_stage(const Stage& stage, const StageLights& lights,
                                const camera::Pose& view, const HeldModel& gun,
                                const Placement& place, const StageShot& shot);

}  // namespace ge::viewmodel
