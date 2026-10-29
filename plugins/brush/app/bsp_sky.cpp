// Our sky picked by a classic map's sky texture (bsp_sky.hpp).
//
// ge:summary Our sky picked by a classic map's sky texture: the rules from hue and brightness
// ge:keywords bsp brush sky sky-type match pick_sky hue dusk tint
// ge:skill brush render
// ge:test tests/brush_bsp_sky_test.cpp
#include "brush/app/bsp_sky.hpp"

#include <algorithm>
#include <cmath>

#include "core/num_text.hpp"

namespace ge::brush::app {

namespace {

double luminance(const double c[3]) { return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]; }

std::string pct(double v) { return core::num::to_fixed(v * 100, 0) + "%"; }

}  // namespace

SkyPick pick_sky(const SkyColours& c) {
  SkyPick p;
  if (!c.found) {
    render::sky_style_named(p.type, p.style);
    p.why = "no sky texture: the Earth's fair day";
    return p;
  }
  // The sky as seen: the back layer under the cloud layer's share.
  double seen[3];
  for (int k = 0; k < 3; ++k) seen[k] = c.back[k] * (1 - c.cover) + c.cloud[k] * c.cover;
  const double y = luminance(seen);
  const double hi = std::max({seen[0], seen[1], seen[2]});
  const double lo = std::min({seen[0], seen[1], seen[2]});
  const double sat = hi > 0 ? (hi - lo) / hi : 0;
  const double r = seen[0] / std::max(hi, 1e-9), g = seen[1] / std::max(hi, 1e-9),
               b = seen[2] / std::max(hi, 1e-9);
  std::string hue;
  if (sat < 0.2) {
    p.type = y < 0.01 ? "void" : "ash";
    hue = y < 0.01 ? "black" : "grey";
  } else if (r >= g && r >= b) {
    if (g > 0.65) {
      p.type = "dust";
      hue = "amber";
    } else if (y < 0.03) {
      p.type = "ash";  // a dark red-brown smoke
      hue = "dark red-brown";
    } else {
      p.type = "hell";
      hue = "red";
    }
  } else if (g >= r && g >= b) {
    p.type = "toxic";
    hue = "green";
  } else if (r >= 0.95 * g && r > 0.35) {
    p.type = "nebula";
    hue = "violet";
  } else {
    p.type = y > 0.3 ? "ice" : "earth";
    hue = "blue";
  }
  render::sky_style_named(p.type, p.style);
  if (p.type == "ash") {
    // The ash's air thinned: its smoke drinks so much of the day that the exposure, keyed on the
    // day's light, would lift the lava and the lamps; the veil darkens the sky instead.
    render::SkyKnobs thin;
    thin.air = 0.35;
    render::with_knobs(p.style, thin);
  }
  // The sky's light against the rooms by the sky's brightness, never so dim that the sky reads as
  // a hole. The sun stays well up: the exposure keys the day's light on a white face, so a low sun
  // would brighten the lava and the lamps against the rooms; the veil darkens the sky instead.
  p.sun_elevation = std::clamp(25 + 60 * y, 25.0, 45.0);
  p.sky_light = std::clamp(std::sqrt(y / 0.25), 0.2, 1.0);
  // A tint halfway toward the texture's own hue, its brightest channel 1.
  for (int k = 0; k < 3; ++k) p.tint[k] = std::sqrt(seen[k] / std::max(hi, 1e-9));
  p.cover = std::clamp(c.cover, 0.1, 0.9);
  p.why = "a " + hue + " sky (" + pct(y) + " bright, " + pct(sat) + " saturated) with " +
          pct(c.cover) + " cloud: " + p.type + ", the sun at " +
          core::num::to_fixed(p.sun_elevation, 0) + " degrees, the sky at " + pct(p.sky_light) +
          " against the rooms";
  return p;
}

bool with_type(SkyPick& pick, std::string_view name) {
  render::SkyStyle style;
  if (!render::sky_style_named(name, style)) return false;
  pick.type = std::string(name);
  pick.style = style;
  return true;
}

}  // namespace ge::brush::app
