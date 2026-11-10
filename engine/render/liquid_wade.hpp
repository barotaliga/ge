// WADING THROUGH LAVA AND SLIME (docs/liquids.md § Wading): a body moving through a liquid the
// renderer draws (render/liquid.hpp) leaves its mark on the surface, and how the surface moves
// says how thick the liquid is. A game hands the bodies standing in its liquids each frame
// (`LiquidWader`: where its feet are, how tall it is); `wade_liquids` turns each body's track into
// STAMPS on the body of liquid it is in, a stamp each `WadeLook::spacing_m` moved (and a small one
// every kIdleS standing still), and the surface draws, from every stamp:
//
// - THE FURROW: a trough along the track, as wide as the body, its lips raised either side (the
//   shape (1 - rho^2) exp(-rho^2) across it, rho the distance over `leg_m`, which moves no
//   liquid), filling back in over `furrow_s`: the liquid parts round the legs and closes slowly
//   behind them. A BOW piled up ahead of the body while it moves.
// - THE RINGS: a packet of waves running out from every stamp at `speed_mps`, `wavelength_m`
//   long, dying over `ring_s` and spreading as 1 / sqrt(r); summed along the track they make the
//   wake.
// - IN LAVA, THE CRUST TORN: the furrow is open melt, hot and glowing, its edges cracked, skinning
//   over again from the edge over `heal_s`.
// - IN SLIME, THE SCUM PARTED: what floats on it (scum, grime, debris) pushed off the furrow.
//
// THE VISCOSITY (`WadeLook::viscosity`): the liquid's thickness as the surface shows it, relative
// to water's (1). It is the picture's knob, not the stuff's viscosity (water/liquid.hpp: lava's is
// hundreds of thousands of times water's, and a surface that thick would not move at all within a
// game's seconds); the named looks set water 1, slime 2, lava 4. A thicker liquid's waves are
// longer and slower and die sooner (a third as far out at 4), and its furrow is deeper
// and closes more slowly (`wade_waves`, every rule a power of the viscosity):
//
//   speed ~ v^-1/2    wavelength ~ v^1/2    ring life ~ v^-1/4    furrow and crust ~ v    depth ~
//   v^1/4
//
// The same surface is worked out on the CPU (`wade_height`, the shader's twin) for the lab's
// charts and the test.
//
// ge:summary Wading through lava and slime: the furrow, the bow, the rings, viscosity as a knob
// ge:keywords render liquid wade wading wake furrow trough bow rings ripples viscosity viscous
// ge:keywords lava slime crust-torn scum-parted wade_liquids wade_waves wade_height wade_look
// ge:keywords liquid_wader liquid_wading stamps track body-in-liquid ce_wade
// ge:skill render water
// ge:test tests/render_liquid_wade_test.cpp
#pragma once

#include <span>
#include <vector>

namespace ge::render {

struct Liquids;

// How a liquid's surface takes a body wading through it (a row of render/liquid.hpp's
// LiquidLook).
struct WadeLook {
  double viscosity = 1;     // relative to water's: water 1, slime 2, lava 4 (the named looks)
  double leg_m = 0.32;      // m: the furrow's half width (a body's)
  double spacing_m = 0.45;  // m: a stamp each this far moved
};

inline constexpr int kLiquidWades = 48;    // the stamps a body of liquid keeps (the latest)
inline constexpr double kIdleS = 0.7;      // s: a stamp this often from a body standing still
inline constexpr double kHeadMostS = 0.4;  // s: the most the newest stamp is drawn on ahead

// What a viscosity makes of the surface's motion (`wade_waves`).
struct WadeWaves {
  double speed_mps = 0;     // the rings' speed out
  double wavelength_m = 0;  // their length
  double ring_s = 0;        // their life's e-folding time
  double furrow_s = 0;      // the furrow's closing (e-folding) time
  double heal_s = 0;        // lava: the torn crust's skinning over (e-folding)
  double depth_m = 0;       // the furrow's depth under a body wading in to its waist
  double amp_m = 0;         // a ring's height at the body
  double life_s = 0;        // how long a stamp is drawn (every part of it faded out by then)
};
// `crust`: a lava's (its torn crust's healing sets how long a stamp lives).
WadeWaves wade_waves(const WadeLook& look, bool crust = false);

// A body standing in a liquid this frame: `id` the game's own (the same body frame to frame),
// `feet` its lowest point (m), `height_m` its height.
struct LiquidWader {
  int id = 0;
  double feet[3] = {0, 0, 0};
  double height_m = 1.4;
};

// One stamp of a track (the GPU's two rows of it, liquid.hpp's LiquidGpuBody::wades).
struct WadeStamp {
  double x = 0, z = 0;    // m: where the body was
  double t = 0;           // s: when (the frame's clock)
  double strength = 0;    // 0..1: how deep it was in, a share of its height (a swimmer's halved)
  double vx = 0, vz = 0;  // m/s: its velocity from the stamp before it
  double span_s = 0;      // s: since the stamp before it on the same track (0: the first)
  bool head = false;      // the newest of its track: drawn on ahead along vx, vz
  int track = 0;          // the wader's id
};

// The tracks and stamps of every body of liquid, kept frame to frame by the game.
struct LiquidWading {
  struct Track {
    int id = 0;
    int body = -1;        // the body of liquid it is in (-1: none)
    double x = 0, z = 0;  // m: its last stamp's place
    double t = 0;         // s: its last stamp's time
    bool seen = false;
  };
  std::vector<Track> tracks;
  std::vector<std::vector<WadeStamp>> stamps;  // a body's, oldest first, at most kLiquidWades
  double last_t = -1;
};

// One frame: the bodies in `waders` stamped onto the bodies of `liquids` they stand in (a body of
// liquid whose box holds the feet, whose level is over them and whose floor is under them, and
// that is drawn), every body's stamps written to its uniform for prepare_liquid. `on` false (the
// game's switch), a jump back in time or a gap of over a second clears them all.
void wade_liquids(Liquids& liquids, LiquidWading& wading, std::span<const LiquidWader> waders,
                  double t_s, bool on);

// THE CPU TWIN of the shader's surface (liquid.wgsl's `wade_at`): at plan point (x, z) at `t_s`,
// over `stamps` with `look`'s waves: the height over the level (m) and the share of torn crust
// (open melt, 0..1). `rings` false leaves the rings out (what the surface's vertices take).
struct WadeAt {
  double height_m = 0;
  double open = 0;
  double furrow = 0;  // 0..1: how far into a furrow (the scum parted)
};
WadeAt wade_height(std::span<const WadeStamp> stamps, const WadeLook& look, bool crust, double x,
                   double z, double t_s, bool rings = true);

}  // namespace ge::render
