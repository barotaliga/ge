// THE STORM IN THE PICTURE: a thunderstorm's flashes (engine/wind/lightning.hpp, a pure function
// of the storm and the clock) as a frame's lamps and channels (render/view.hpp's `Flashes`,
// render/bolts.hpp's segments), chosen each frame.
//
// - THE LAMPS (`lamp_of`): a flash's light over the frame's window as a lamp at its channel's
//   middle, its reach half the channel, its candela `light_of`'s power times `kUnitCandela`, and
//   the depth of cloud its light crosses on the way out: deep for a flash inside the cloud, a
//   little for a strike's channel, most of it below the base.
// - THE CHANNELS (`bolts_of`): a strike's segments lit over the window, the main channel by every
//   stroke and the branches by the first alone (the leader's branches the first return stroke
//   lights); none above the cloud's base, none of a flash inside the cloud. The flash's candela is
//   shared out along them by glow and length.
// - A FRAME (`storm_lights`): every flash alive in the window, the `render::kFlashLamps` brightest
//   as the eye sees them (`brightest`) the lamps, the open sky's one scalar (`sky_light`), and the
//   strikes among the lit flashes drawn, each channel built once (`build_channel`) and kept while
//   its flash lasts.
// - THE SKY (`storm_sky`): a summer afternoon's in place of the map's own (no lid's damp air, no
//   valley fog: the air clear but for the fall), towers of cumulonimbus in place of its cloud,
//   their cover the storm's nearness (`nearness`: the live cells' grown towers, each falling off
//   past `kStormReach`), the storm's slate closing over the dome as they near (the sun gone, the
//   deck's light a third of the day's and darker overhead), and the fall seen where
//   the eye is (air::fall_at with the storm's rain). The towers are the cloud kind's noise, not
//   placed on the cells: the lamps light whatever cloud stands where a flash is.
//
// ge:summary A storm's flashes as a frame's lamps and lightning channels, chosen each frame
// ge:keywords storm lightning flash lamps bolts channel brightest sky_light storm_lights lamp_of
// ge:keywords bolts_of strike frame storm_sky nearness cumulonimbus towers
// ge:skill mountain render
// ge:test tests/mountain_storm_look_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "mountain/air/air.hpp"
#include "mountain/app/cloud_field.hpp"
#include "mountain/mapgen/types.hpp"
#include "render/bolts.hpp"
#include "render/view.hpp"
#include "wind/lightning.hpp"

namespace ge::mountain::app {

// A flash's light over a window as the frame's lamp.
render::FlashLamp lamp_of(const wind::FlashLight& light);

// `flash`'s lamp over [a, b) (none when it gives no light then, or with no flash).
render::Flashes lamps_of(const wind::Flash* flash, double a, double b);

// The segments of `flash`'s channel `channel` lit over [a, b), appended to `out`; `base_y` is the
// cloud's base, world m.
void bolts_of(const wind::Flash& flash, const wind::Channel& channel, double a, double b,
              double base_y, std::vector<render::BoltSegment>& out);

// A frame's lightning, kept from frame to frame for the channels.
struct StormLights {
  render::Flashes lamps;                   // the brightest, as lamps
  double sky = 0;                          // the open sky's flash (`sky_light`), in the unit
  std::vector<render::BoltSegment> bolts;  // the strikes' channels lit this frame
  struct Kept {
    uint64_t key = 0;
    double end = 0;  // s: its flash's last glow
    std::unique_ptr<wind::Channel> channel;
  };
  std::vector<Kept> kept;          // the strikes' channels built
  std::vector<wind::Flash> alive;  // the window's flashes (scratch)
};

// The lamps, the sky's flash and (with `channels`) the strikes' channels of `storm` over the
// frame's window [a, b) seen from `eye` (world m).
void storm_lights(StormLights& lights, const wind::Storm& storm, const double eye[3], double a,
                  double b, bool channels = true);

// How near `storm` is to (x, z) at `t`: 0 none of it in the sky .. 1 a grown tower overhead.
inline constexpr double kStormReach = 12000;  // m, where a tower's share has halved
double nearness(const wind::Storm& storm, double t, double x, double z);

// The sky under `storm` at `t` from `eye` (world m): the towers in `clouds` (with `layers`; none
// without) and the weather they and the storm's fall make in `weather`.
void storm_sky(const mapgen::Level& level, const wind::Storm& storm, double t, const double eye[3],
               bool layers, const Aloft& aloft, render::Weather& weather, render::Clouds& clouds);

}  // namespace ge::mountain::app
