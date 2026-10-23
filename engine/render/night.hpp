// THE NIGHT IN THE PICTURE, as numbers: the moon's light and disc from where it stands, which of
// the sun and the moon is the KEY LIGHT, and the dark sky's own glow. Plain arithmetic a game
// calls once a frame after it has placed the sun (render/view.hpp's `Night`); the shaders draw it
// (wgsl/frame.wgsl's exposure and night vision, sky.wgsl's moon, stars and band).
//
// - THE MOON'S LIGHT. A full moon at its mean distance gives 0.317 lx outside the air. The phase
//   law is its measured magnitude curve (0.026 magnitudes a degree of phase angle and a fourth
//   power term), steep near full (the opposition surge), so a half moon gives a tenth of a full
//   one's light, not a half. Nearer it is brighter by the square of the distance.
// - THE DISC. Drawn at its true angular size (29.4' across at apogee, 33.5' at perigee), its lit
//   part by Lommel-Seeliger's law (a dusty body's disc is nearly flat at full, brightest at the
//   bright limb in a crescent), scaled so the whole disc gives the phase law's light
//   (`surface_surge`). The dark part is earthlit: a ten-thousandth of the lit surface's
//   brightness under a full Earth (a new moon), less as the Earth wanes.
// - TWO SKIES. At night the sky is the twilight's (the sun's own sky table) and the moon's (a
//   second table at the moon's height, lit by its light) at once, so the dusk's glow fades on
//   its own as the moon's sky comes up under it.
// - THE KEY LIGHT (the one direct light, the one that casts shadows) is the sun until it is gone
//   from every eye (three degrees under: a horizon seen from 4 km dips two), then the moon when
//   it is up. Its direct light while the sun still keys is spread from above
//   (`Night::other_light`). The air's haze is the brighter sky's light scattered.
// - THE DARK SKY with no moon: a zenith of 2e-4 cd/m2 (21.8 magnitudes a square arc-second, a
//   dark mountain site's: the airglow, the zodiacal light, the stars too faint to see one by
//   one) and 0.0015 lx on level snow. Set only once the sun is down, with the moon's light and
//   sky, so a day's bits never move.
//
// ge:summary The night's numbers: the moon's light, disc and phase, the key light, the dark sky
// ge:keywords render night moon moonlight phase-law opposition-surge lommel-seeliger earthshine
// ge:keywords key-light twilight dark-sky airglow sidereal stars set_night moon_lux phase_law
// ge:keywords surface_surge twilight_lux moon_radius night-sky two-skies moon-sky pre_exposure
// ge:skill render
// ge:test tests/render_night_test.cpp
#pragma once

#include "render/view.hpp"

namespace ge::render {

// The sun's illuminance outside the air, lx: what one unit of FrameView::sun_illuminance is.
inline constexpr double kSunLux = 128000;
// A full moon's illuminance outside the air at its mean distance, lx (magnitude -12.74).
inline constexpr double kMoonLux = 0.317;
inline constexpr double kMoonRadiusKm = 1737.4;
inline constexpr double kMoonMeanKm = 384400;
// The earthlit part's brightness against the lit part's under a full Earth.
inline constexpr double kEarthshine = 1.0e-4;
// The dark sky's zenith luminance, cd/m2, and the light it gives level snow, lx.
inline constexpr double kDarkSkyCdm2 = 2.0e-4;
inline constexpr double kDarkSkyLux = 0.0015;

// The moon's light at phase angle `i` (rad, 0 full) against a full moon's.
double phase_law(double i);
// The phase angle of a moon whose disc is `lit` lit, rad (0 full, pi new).
double phase_angle(double lit);
// The moon's illuminance outside the air at phase angle `i`, `km` away, lx.
double moon_lux(double i, double km);
// The moon's angular radius `km` away, rad.
double moon_radius(double km);
// How much brighter the lit surface is at phase angle `i` than Lommel-Seeliger's shape alone
// would make it, so the whole disc gives the phase law's light: 1 at full.
double surface_surge(double i);
// The light the twilight gives a level face with the sun at `elevation` (rad, under the horizon),
// lx: the sky tables' own, a degree at a time (the pre-exposure's estimate and the haze's
// choice read it); a sun up gives far more than any moon.
double twilight_lux(double elevation);
// The moon's light on a level face at `elevation` (rad) through the air, from `lux` outside it.
double moonlight_on_ground(double lux, double elevation);

// THE PRE-EXPOSURE: the scale the frame stores light at (FrameUniforms::night_sun[3]), so a
// night's luminances, a millionth of the day's, keep their bits in a half-float target: the light
// on a white face estimated (the moon's or the twilight's, and the dark sky's) brought to about
// a quarter of a unit. 1 whenever the sun is up, so a day's bits never move; the exposure
// divides it back out, and every absolute reading (the eye's adaptation, a star's light) is
// taken through it.
double pre_exposure(const FrameView& view);

struct Moon {
  double elevation;    // rad, seen from the surface
  double azimuth;      // world heading
  double lit;          // share of the disc lit, 0..1
  double distance_km;  // from the eye
};

// Fill `view.night` from `moon` (the sun already placed in `view`), the latitude (degrees) and
// the local sidereal angle (degrees, core/solar.hpp's sidereal_time plus the longitude), and make
// the moon the key light once the sun is gone.
void set_night(FrameView& view, const Moon& moon, double latitude_deg, double sidereal_deg);

}  // namespace ge::render
