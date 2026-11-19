// THE FLY BETWEEN THE MAP AND THE EYE, for any game's spectators: from the level seen from above
// (the overview's picture, then the level itself drawn straight down) a dive to the ground where
// the followed player stands, the view turning to his heading on the way down and tilting up into
// his eye as it lands; and the same climb back out. Pure functions of the time the fly has run:
//
// - THE POSE (`fly_pose`): the eye over the middle of the level at the height that fits the whole
//   level in view (`top_pose`), sliding over the player early in the dive, falling fast and then
//   slower (the height eased in log space, so the ground grows at an even rate), looking straight
//   down until the last `tilt_s`, when it tilts up into the player's own pitch;
// - THE MAP's SHARE (`fly_map_share`): the 2D picture fades out over the dive's first `fade_s` so
//   the level drawn from above takes its place, and back in over the climb's last;
// - THE DEMO's RATE (`fly_demo_rate`): held still while it dives (nothing the player does is
//   missed), then played at `catch_rate` easing to 1x over the time that gives back what the dive
//   held, so the watcher lands in the match as it is. The climb out does not hold it.
//
// Engine metres, y up, the world's heading and pitch (engine/core/math.hpp).
//
// ge:summary The fly between the map from above and a player's eye: the dive, the tilt, catch-up
// ge:keywords overview fly-down dive tilt-up zoom live-map catch-up demo-rate transition camera
// ge:keywords fly_pose top_pose fly_demo_rate fly_map_share
// ge:skill map-design camera
// ge:test tests/overview_fly_test.cpp
#pragma once

#include <cstdint>

namespace ge::overview {

// Where the eye is and where it looks.
struct FlyPose {
  double eye[3] = {0, 0, 0};  // m, y up
  double heading = 0;         // rad, 0 down +z, clockwise from above
  double pitch = 0;           // rad, up positive
};

struct FlyTimes {
  double dive_s = 1.6;      // from the top to the eye
  double tilt_s = 0.55;     // the dive's last part, tilting up into the eye
  double fade_s = 0.25;     // the map's picture fading out at the start
  double catch_rate = 3.5;  // the demo's speed as it starts catching up
};

enum class FlyWay : uint8_t { none, down, up };

struct Fly {
  FlyWay way = FlyWay::none;
  double p = 0;          // how far down: 0 the map from above, 1 the eye
  double owed_s = 0;     // the demo's seconds held by the dive, still to give back
  double catch_t = -1;   // s into the catch-up, -1 none
  double catch_len = 0;  // s the catch-up lasts
};

// The pose that sees the whole level (`lo`, `hi`, m) straight down, north (+z) up the screen, for
// a view `fov_y` rad tall and `aspect` wide over tall.
FlyPose top_pose(const double lo[3], const double hi[3], double fov_y, double aspect);

// The pose from straight above that lays the level's plan (`lo`, `hi`, m: its x and z, at a height
// a third of the way up) onto `box_px` (x0, y0, x1, y1) of a `width` x `height` px view `fov_y`
// rad tall whose lens is shifted up by `shift_y` (render/view.hpp's): the 3D level drawn where the
// overview's 2D picture of it stands, so one fades into the other.
FlyPose top_pose_onto(const double lo[3], const double hi[3], double fov_y, double width,
                      double height, double shift_y, const double box_px[4]);

// Start a way: down from the map to the eye, or up from the eye to the map. Asked for the other
// way while one runs, it turns back from where it is.
void fly_start(Fly& f, FlyWay way);
// `dt` s of wall clock on (the fly's own clock, never the demo's).
void fly_advance(Fly& f, double dt, const FlyTimes& times);
// Whether a way is running (the catch-up after a dive counts: the demo is not at 1x yet).
bool fly_running(const Fly& f);

// The eye now, between `top` and `eye` (the followed player's eye this frame).
FlyPose fly_pose(const Fly& f, const FlyTimes& times, const FlyPose& top, const FlyPose& eye);
// How much of the map's 2D picture shows, 0..1.
double fly_map_share(const Fly& f, const FlyTimes& times);
// The demo's speed against 1x: 0 held, over 1 catching up.
double fly_demo_rate(const Fly& f, const FlyTimes& times);
// Whether the frame is the 3D world (true) or the map alone (false).
bool fly_shows_world(const Fly& f);

}  // namespace ge::overview
