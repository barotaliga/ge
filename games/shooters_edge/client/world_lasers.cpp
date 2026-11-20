// Quake II's lasers in the window: the beam rows the rules show (their render bit), each drawn as
// a held beam in either look, its colour one of its four palette shades picked again each frame.
//
// ge:summary Quake II's lasers: the beam rows gathered and drawn as held beams in a shade a frame
// ge:keywords laser target-laser beam-entity render-beam held-beam palette-shade
// ge:skill render shooter
// ge:test tests/shooters_edge_q2_targets_test.cpp
#include <cstddef>
#include <cstdint>
#include <vector>

#include "brush/map.hpp"
#include "core/prng.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/world_fx.hpp"

namespace shooters_edge::client {

namespace {
constexpr int kBeamRender = 128;  // Quake II's render bit for a beam entity (a laser)
}  // namespace

void scene_lasers(const Listen& l, std::vector<SceneBeam>& out) {
  out.clear();
  if (l.server == nullptr) return;
  for (size_t e = 1; e < l.server->world.entities.size(); e++) {
    const sv::WorldEntity& row = l.server->world.entities[e];
    if (row.free || (row.render_fx & kBeamRender) == 0) continue;
    out.push_back({static_cast<int>(e), row.origin, row.beam_end, row.frame,
                   static_cast<uint32_t>(row.skin)});
  }
}

void laser_beams(const std::vector<SceneBeam>& beams, uint32_t seed,
                 const std::vector<uint8_t>& palette, double t, std::vector<HeldBeam>& out) {
  ge::core::Rng shade = ge::core::make_rng(seed);
  for (const SceneBeam& sb : beams) {
    HeldBeam b;
    b.entity = sb.entity;
    b.laser = true;
    b.laser_m = static_cast<float>(sb.width * ge::brush::kUnitM);
    const uint32_t index = (sb.colours >> (8 * ge::core::int_in(shade, 0, 3))) & 0xff;
    for (int k = 0; k < 3; ++k)
      b.laser_rgb[k] = 3 * index + 2 < palette.size()
                           ? static_cast<float>(palette[3 * index + k]) / 255.0f
                           : 1.0f;
    b.shot.born_s = t - 1;  // long on: no swell
    b.shot.seed = static_cast<uint32_t>(sb.entity);
    const ge::camera::Vec3 from = engine_point(sb.from), to = engine_point(sb.to);
    b.shot.from[0] = from.x;
    b.shot.from[1] = from.y;
    b.shot.from[2] = from.z;
    b.shot.to[0] = to.x;
    b.shot.to[1] = to.y;
    b.shot.to[2] = to.z;
    out.push_back(b);
  }
}

}  // namespace shooters_edge::client
