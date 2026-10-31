// Fall Line's frame options from a player's settings: the r_ cvars and the server's sky knobs
// mapped onto FrameOptions, then the quality ladder's picture over the rows it owns.
//
// ge:summary Fall Line's frame options from the cfg: the r_ cvars and the quality ladder
// ge:keywords fall-line frame-options frame_options r_quality cvars settings
// ge:skill render console
#include <algorithm>
#include <cmath>
#include <string>

#include "core/output.hpp"
#include "fall_line/app/frame.hpp"
#include "fall_line/app/ladder.hpp"
#include "fall_line/app/mists.hpp"
#include "fall_line/app/sky_type.hpp"

namespace fall_line::app {

FrameOptions frame_options(const cfg::Settings& s) {
  FrameOptions o;
  o.taa = s.taa;
  o.scale = s.scale;
  o.occlusion = s.occlusion;
  o.shadow_size = s.shadow_size;
  o.bloom = s.bloom;
  o.look = {.sharpen = s.sharpen,
            .chromatic = s.chromatic,
            .vignette = s.vignette,
            .grain = s.grain,
            .dirt = s.lensdirt,
            .motion_blur = s.motionblur};
  o.lens = {.rays = s.sunrays, .flare = s.flare};
  o.ao = {.strength = s.ao, .radius_m = s.ao_radius};
  o.lights = s.lights;
  o.light_shadows = s.lights && s.light_shadows;
  o.dof = {.strength = s.dof, .focus_m = s.dof_focus};
  o.grade = {.contrast = s.contrast,
             .saturation = s.saturation,
             .temperature = s.temperature,
             .tint = s.tint,
             .gamma = s.gamma};
  o.snow = s.snow;
  o.trails = s.trails;
  o.spray = s.spray;
  o.precip = s.precip;
  o.splashes = s.splashes;
  o.wet = s.wet;
  o.lightning = s.lightning;
  o.fog = s.fog_drawn;
  o.froxels = fog_settings(static_cast<int>(s.fog_quality));
  o.mirror = s.reflections;
  o.fall.particles = static_cast<uint32_t>(
      std::lround(o.fall.particles * std::clamp(s.precip_density, 0.25, 2.0)));
  o.weather = s.weather;
  o.clouds = s.clouds;
  o.cover = s.cover;
  o.thick = s.thick;
  o.cloud_height = s.cloud_height;
  o.cloud_thickness = s.cloud_thickness;
  o.cloud_speed = s.cloud_speed;
  o.sun_size = s.disc_sun;
  o.moon_size = s.disc_moon;
  cloud_stack_of(s, o);
  // r_quality: the ladder's picture over the rows it owns; `custom` leaves the settings above.
  std::string error;
  if (const auto pick = quality_pick(s.quality, s.budget_ms, static_cast<uint32_t>(s.width),
                                     static_cast<uint32_t>(s.height), error))
    o = apply_picture(*pick, o);
  else if (!error.empty())
    ge::core::output::warn(error);
  return o;
}

}  // namespace fall_line::app
