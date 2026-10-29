// LIGHTNING AS A PURE FUNCTION OF THE STORM AND THE CLOCK (docs/storms.md § Lightning). Which
// flashes a storm makes, when, of which kind, with which strokes, and how much light each gives
// over any window of time are computed from the storm's cells and never stepped; a flash's
// channel is built on demand into a caller's fixed buffer.
//
// - THE SCHEDULE (`flashes_between`): each live cell's clock in 1 s slots; a slot holds a flash
//   with the chance the cell's rate gives it (`cell_at`'s flashes a minute over 60), hashed off the
//   cell's seed and the slot, so any window lists the same flashes whoever asks.
// - THE KINDS: a CLOUD flash (intra-cloud, about three in four) runs between the charge layers,
//   6 to 9 km up, 3 to 12 km long and mostly level, lighting the cloud from inside: seen through
//   the cloud it is sheet lightning, and too far off to be heard, heat lightning. A GROUND flash
//   (cloud-to-ground) leaves the lower charge region 4 to 6 km up under the core and strikes the
//   ground within about a core's radius of the cell's centre; one in 25 leaves the anvil's top and
//   strikes 8 to 15 km out ahead of the storm (a bolt from the blue).
// - A GROUND FLASH'S STROKES: a stepped leader (15 to 40 ms, faint), then 1 to 12 return strokes,
//   a quarter of flashes a single one and about four on average; the strokes between 8 and 300 ms
//   apart (log-normal about 60 ms); the first the brightest, the rest a quarter to three quarters
//   of it; two flashes in five carry a continuing current, 40 to 300 ms of steady glow after one
//   stroke. That gap and that glow are the flicker the eye sees.
// - A CLOUD FLASH'S LIGHT: 0.2 to 0.8 s of dim glow broken by 3 to 10 recoil pulses (K-changes),
//   each a few milliseconds long.
// - THE LIGHT (`light_over`): the mean power over a window, integrated in closed form (a stroke's
//   light decays exponentially, 2 ms for a return stroke and 6 ms for a pulse in the cloud), so a
//   frame of 1/60 s or 1/120 s gets the light that fell in it however short the stroke. The unit
//   is the mean over a 1/60 s frame of a stroke of energy 1.
// - WHAT A VIEWER SEES (`sky_light`, `brightest`): every flash's light falling off with its
//   distance from the viewer (half at 4 km), one scalar for the frame, and the strongest few as
//   positioned lights (the channel's middle and half its length as a radius) for a renderer that
//   lights a cloud from inside.
// - THE CHANNEL (`build_channel`): a stepped walk from the flash's start to its end in 60 m steps
//   (80 m in the cloud), bent about 20 degrees a step and drawn toward its end, branching about
//   one step in 16 and sub-branching on the branches; the main channel glows 1, a branch less
//   along its length. A return stroke lights the main channel; the first stroke lights the
//   branches too.
//
// Heights are world y: a storm's ground plus its heights over the ground. Every draw is a hash of
// a cell's seed and the slot, or a stream seeded off them.
//
// ge:summary Lightning: the flash schedule, ground and cloud flashes, strokes, flicker, channels
// ge:keywords lightning flash stroke return-stroke leader stepped-leader continuing-current
// ge:keywords flicker intra-cloud cloud-to-ground sheet-lightning heat-lightning k-change
// ge:keywords bolt-from-the-blue channel branch tortuosity flashes_between light_over sky_light
// ge:keywords brightest build_channel flash-light
// ge:skill water
// ge:test tests/wind_lightning_test.cpp
#pragma once

#include <cstdint>

#include "wind/storm.hpp"

namespace ge::wind {

inline constexpr double kSlot = 1.0;      // s, a cell's flash clock
inline constexpr double kMaxFlash = 1.6;  // s, the longest a flash lasts from its leader
inline constexpr int kMaxStrokes = 12;

enum class FlashKind : uint8_t { cloud, ground };

// The kinds' words ("cloud", "ground").
const char* flash_kind_name(FlashKind kind);

// One burst of light: a return stroke, or a pulse in the cloud.
struct Stroke {
  double t = 0;       // s after the flash's start
  double energy = 0;  // its light, in the unit (the first ground stroke about 1)
  double tau = 0;     // s, its light's decay
  double hold = 0;    // s of glow after it (a continuing current, a cloud flash's glow)
  double glow = 0;    // the glow's power, in the unit
};

struct Flash {
  double t0 = 0;                  // s, its leader's start
  double x = 0, y = 0, z = 0;     // where it starts in the cloud
  double ex = 0, ey = 0, ez = 0;  // where it ends: the ground strike, or the cloud channel's end
  double leader = 0;              // s, the stepped leader before the first stroke (ground)
  double length = 0;              // s, from t0 to the last glow
  Stroke stroke[kMaxStrokes]{};
  int cell = 0;
  int strokes = 0;
  uint32_t seed = 0;  // its channel's stream
  FlashKind kind = FlashKind::cloud;
  bool blue = false;  // a bolt from the blue
};

// Every flash of `storm` alive in [t0, t1) into `out` (up to `cap`), by start time; returns how
// many were found (more than `cap` means some were left out).
int flashes_between(const Storm& storm, double t0, double t1, Flash* out, int cap);

// The flash cell `k` makes in slot `n`, if it makes one.
bool flash_in_slot(const Storm& storm, int k, int64_t n, Flash& out);

// The mean power of `flash`'s light over [a, b) (absolute s, b > a), in the unit.
double light_over(const Flash& flash, double a, double b);

// How much of a flash's light reaches a viewer `d` m away (1 at the flash, half at 4 km).
double reach_of(double d);

// The flash's middle and its radius (half its extent), for a light placed in the world.
struct FlashLight {
  double x = 0, y = 0, z = 0;
  double radius = 0;  // m
  double power = 0;   // the frame's mean power at the flash, in the unit
  bool cloud = false;
};
FlashLight light_of(const Flash& flash, double a, double b);

// THE FRAME'S ONE SCALAR: every flash's light over [a, b) brought to a viewer at (x, y, z).
double sky_light(const Flash* flashes, int n, double x, double y, double z, double a, double b);

// The `cap` strongest lights as a viewer at (x, y, z) sees them over [a, b), strongest first;
// returns how many.
int brightest(const Flash* flashes, int n, double x, double y, double z, double a, double b,
              FlashLight* out, int cap);

// THE CHANNEL: segments of a flash's lightning, the main channel first.
inline constexpr int kMaxSegments = 1024;
struct Segment {
  float x0, y0, z0, x1, y1, z1;
  float glow;     // 1 the main channel, less on a branch and along it
  uint8_t order;  // 0 the main channel, 1 a branch, 2 a branch's branch
};
struct Channel {
  int count = 0;
  Segment seg[kMaxSegments];
};
void build_channel(const Flash& flash, Channel& out);

}  // namespace ge::wind
