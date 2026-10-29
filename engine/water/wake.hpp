// WAKES AND RINGS (docs/water.md § Wakes): the waves a moving hull, a swimmer's stroke or a body
// dropped in leaves on the water, as RINGS: each a volume of water pushed aside at a point and a
// moment, spreading as the deep-water ring of an impulse (the Cauchy-Poisson problem). Its far
// field at a distance r and a time tau after it was set off is
//
//   eta = V g tau^2 / (4 sqrt(2) pi r^3) cos(g tau^2 / (4 r)),
//
// a train whose local wavenumber k = g tau^2 / (4 r^2) shortens behind its front, each length
// running at its own group speed. The source is a patch `size` m across (its spectrum
// exp(-k^2 size^2 / 4): a body makes few waves much shorter than itself), and the formula holds
// only where the train has turned through a few radians, so its first half turn behind the front
// is faded in. A hull moving on emits rings along its track (`wake_trail`); summed, they interfere
// into the Kelvin wake: TRANSVERSE waves curving across the track behind the hull and DIVERGING
// waves, short crests feathered outward along its two arms, the arms at 19.47 degrees either side
// of the track whatever the speed (the `float` lab measures it). A splash is one ring
// (`splash_ring`, float.hpp).
//
// A BODY THROUGH THE SURFACE (entry.hpp) also opens a CRATER, the near field the far-field
// formula softens away: a ring with `crater` > 0 adds, round its point, a cavity `depth` m deep
// and `crater` m across with its rim raised (the shape (1 - rho^2) exp(-rho^2), rho = r /
// crater, which moves no water), opening and closing again over 2 sqrt(crater / g) s, and then
// the jet the closing throws up in the middle, 0.6 of the depth high and a third of the crater
// across, up and down ballistically. Both are in `ring_height` and `wake_at`, so the drawn
// crater is the one a float drops into.
//
// THE TIER. Rings are evaluated where asked, like the waves; what is kept is bounded:
// `Wakes::life` s of rings (faded out over its last third), at most kMaxRings (the oldest go
// first), waves shorter than `Wakes::shortest` m damped out (the short waves near the source,
// which viscosity and the picture's pixels would lose anyway), and a ring's centre softened over
// its source's size so nothing is infinite. A game drops the rings of a hull no actor is near.
//
// `water::sample` adds a body's rings to its lake's or ocean's surface (world.hpp), so a float
// rocks in a passing boat's wake; the renderer draws the same rings (render/water.hpp).
//
// ge:summary Wakes and rings: impulse rings on deep water, a hull's trail, the Kelvin wake
// ge:keywords water wake ring rings kelvin cauchy-poisson impulse trail hull boat swimmer splash
// ge:keywords emit_ring wake_trail wake_at wakes dispersion group-speed crater cavity jet
// ge:keywords worthington crater_height near-field
// ge:skill water
// ge:test tests/water_wake_test.cpp
#pragma once

#include <cstdint>
#include <vector>

namespace ge::water {

inline constexpr int kMaxRings = 1024;

// One impulse: `volume` m^3 pushed aside at (x, z) at `t0` over a source `size` m across.
struct Ring {
  double x = 0, z = 0;
  double t0 = 0;
  double volume = 0;
  double size = 0.5;
  double shortest = 0;  // m: its own shortest wave (a trail's: an eighth of its transverse wave)
  int body = 0;         // the body whose surface it is on
  double crater = 0;    // m: a body's crater round the point (entry.hpp); 0: none
  double depth = 0;     // m: the crater's depth
};

struct Wakes {
  std::vector<Ring> rings;  // oldest first
  double life = 24;         // s a ring is kept
  double shortest = 0.3;    // m: shorter waves are damped out
};

// What a body's rings add at a point: the height, its slope, and the water's velocity.
struct WakeAt {
  double height = 0;
  double dx = 0, dz = 0;       // d height / dx, dz
  double u = 0, v = 0, w = 0;  // m/s
};

// A ring set off now; rings older than the wakes' life, and the oldest past kMaxRings, go.
void emit_ring(Wakes& wakes, const Ring& ring);

// Where a hull last set off a ring.
struct Trail {
  double last = -1e300;  // s
  double x = 0, z = 0;   // m
};
// A hull `length` m long and `beam` m wide drawing `draught` m, at (x, z) moving at (vx, vz) m/s:
// a ring each time it has run a third of the shortest wave kept, or `interval` s have gone by,
// carrying a share of the water it pushes aside meanwhile (beam x draught x the distance run,
// times `strength`; 0.3 gives a 4 to 5 m boat's wake waves of 5 to 15 cm, their slopes up to
// about 0.2 by the hull), its source a tenth of the hull's length across. The shortest wave kept is
// an eighth of the transverse wave (2 pi U^2 / g) and no shorter than `Wakes::shortest`: the
// diverging waves run down from two thirds of the transverse one at the arms to ever shorter
// toward the track, and a third of a wave between two rings is what keeps the sum from aliasing
// into ridges along the track. So a slow hull sets off rings closer together than a fast one.
// A hull at rest sets off nothing.
void wake_trail(Wakes& wakes, Trail& trail, int body, double x, double z, double vx, double vz,
                double length, double beam, double draught, double t, double interval = 0.1,
                double strength = 0.3);

// One ring's height at a distance `r` m, `tau` s after it was set off (the far field above,
// softened and damped as the wakes keep it).
double ring_height(const Wakes& wakes, const Ring& ring, double r, double tau);

// A ring's crater and jet at a distance `r` m, `tau` s after it was set off (0 without one).
double crater_height(const Ring& ring, double r, double tau);
// How long a ring's crater and its jet last, s (0 without one).
double crater_time(const Ring& ring);

// Every ring of `body` at (x, z) at `t`.
WakeAt wake_at(const Wakes& wakes, int body, double x, double z, double t);

}  // namespace ge::water
