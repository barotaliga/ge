// Models drawn between their frames (games/shooters_edge/client/model_lerp.hpp): a frame change
// blended over the time the frames last, a new model
// or a first sight never blended, the listen server's places drawn one server frame behind and
// evenly spaced at a picture rate that is not the server's, a monster's strides drawn over their
// time, a teleport not dragged across the map, and both switches off drawing each state as held.
//
// ge:summary Tests: models' animation and places drawn between frames, the snap rules
// ge:keywords test model-lerp lerp_models frame-lerp step-lerp r_lerpmodels r_lerpmove
// ge:skill shooter testing
// ge:test tests/shooters_edge_model_lerp_test.cpp
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/client/model_lerp.hpp"

namespace cl = shooters_edge::client;
namespace sh = ge::shooter;

namespace {

bool near(float a, float b, float tol = 1e-3f) { return std::fabs(a - b) <= tol; }

cl::LerpModel thing(int entity, int frame, sh::Vec3f at = {}) {
  cl::LerpModel p;
  p.entity = entity;
  p.modelindex = 3;
  p.frame = frame;
  p.origin = at;
  return p;
}

constexpr cl::ModelLerpSwitches kFrames{.frames = true, .moves = false};
constexpr cl::ModelLerpSwitches kBoth{.frames = true, .moves = true};

}  // namespace

TEST(a_frame_change_is_blended_over_the_frames_time) {
  cl::ModelLerp l;
  std::vector<cl::LerpModel> v{thing(40, 0)};
  cl::lerp_models(l, v, 0.0, {}, kFrames);
  CHECK(v[0].frame_from == -1);  // first seen: nothing to come from
  CHECK(v[0].lerp);
  // The frame steps to 1 at 0.5 s: drawn from 0, a quarter of the way 25 ms later.
  v = {thing(40, 0)};
  cl::lerp_models(l, v, 0.4, {}, kFrames);
  v = {thing(40, 1)};
  cl::lerp_models(l, v, 0.5, {}, kFrames);
  CHECK(v[0].frame_from == 0);
  CHECK(near(v[0].frame_f, 0));
  v = {thing(40, 1)};
  cl::lerp_models(l, v, 0.525, {}, kFrames);
  CHECK(v[0].frame_from == 0);
  CHECK(near(v[0].frame_f, 0.25f));
  // Frames changing every 40 ms are blended over 40 ms, not a whole 100.
  v = {thing(40, 2)};
  cl::lerp_models(l, v, 0.54, {}, kFrames);
  v = {thing(40, 2)};
  cl::lerp_models(l, v, 0.56, {}, kFrames);
  CHECK(v[0].frame_from == 1);
  CHECK_MSG(near(v[0].frame_f, 0.5f), std::to_string(v[0].frame_f).c_str());
  // Done after the frame's time: the frame alone.
  v = {thing(40, 2)};
  cl::lerp_models(l, v, 0.61, {}, kFrames);
  CHECK(v[0].frame_from == -1);
  // A change after a long hold takes the last pace (40 ms), not the hold.
  v = {thing(40, 3)};
  cl::lerp_models(l, v, 2.0, {}, kFrames);
  v = {thing(40, 3)};
  cl::lerp_models(l, v, 2.03, {}, kFrames);
  CHECK(v[0].frame_from == 2);
  CHECK(near(v[0].frame_f, 0.75f));
}

TEST(a_new_model_a_warp_or_a_gap_is_never_blended) {
  cl::ModelLerp l;
  std::vector<cl::LerpModel> v{thing(40, 0)};
  cl::lerp_models(l, v, 0.0, {}, kFrames);
  v = {thing(40, 5)};
  v[0].modelindex = 4;  // another model on the entity
  cl::lerp_models(l, v, 0.01, {}, kFrames);
  CHECK(v[0].frame_from == -1);
  v = {thing(40, 6)};
  v[0].modelindex = 4;
  v[0].warping = true;  // a spawn's warp
  cl::lerp_models(l, v, 0.02, {}, kFrames);
  CHECK(v[0].frame_from == -1);
  std::vector<cl::LerpModel> none;
  cl::lerp_models(l, none, 0.03, {}, kFrames);  // a picture without it
  CHECK(l.tracks.empty());
  v = {thing(40, 7)};
  v[0].modelindex = 4;
  cl::lerp_models(l, v, 0.04, {}, kFrames);
  CHECK(v[0].frame_from == -1);
  // A seek (the clock back) starts over.
  v = {thing(40, 8)};
  v[0].modelindex = 4;
  cl::lerp_models(l, v, 0.01, {}, kFrames);
  CHECK(v[0].frame_from == -1);
}

TEST(off_draws_each_frame_as_held) {
  cl::ModelLerp l;
  for (int k = 0; k < 10; k++) {
    std::vector<cl::LerpModel> v{thing(40, k, {float(k) * 4, 0, 0})};
    cl::lerp_models(l, v, k * 0.05,
                    {.moves = true, .state_s = k * 0.05, .drawn_s = k * 0.05 - 0.01}, {});
    CHECK(v[0].frame_from == -1);
    CHECK(!v[0].lerp);
    CHECK(v[0].origin.x == float(k) * 4);
  }
}

// The listen server at 72 frames a second drawn at 60 pictures a second: a player running +x at
// 288 u/s (4 units a frame) is drawn one frame behind, every picture the same 4.8 units on.
TEST(live_places_are_drawn_one_server_frame_behind_and_even) {
  cl::ModelLerp l;
  const double tic = 1.0 / 72, pic = 1.0 / 60;
  double sv = 0, owed = 0;
  int frames = 0;
  float least = 1e9f, most = -1e9f, last = 0;
  bool have = false;
  for (int k = 0; k < 120; k++) {
    owed += pic;
    while (owed >= tic) {
      owed -= tic;
      sv += tic;
      frames++;
    }
    std::vector<cl::LerpModel> v{thing(2, 0, {float(frames) * 4, 0, 0})};
    cl::lerp_models(l, v, k * pic, {.moves = true, .state_s = sv, .drawn_s = sv + owed - tic},
                    kBoth);
    const float x = v[0].origin.x;
    CHECK(x <= float(frames) * 4 && x >= float(frames - 1) * 4 - 1e-3f);  // a frame behind
    if (have && k > 2) {
      least = std::min(least, x - last);
      most = std::max(most, x - last);
    }
    last = x;
    have = true;
  }
  CHECK_MSG(most - least < 0.05f,
            ("steps from " + std::to_string(least) + " to " + std::to_string(most)).c_str());
}

// A monster the program moves 8 units every tenth of a second (on the 72 Hz server's frames: every
// 7 or 8 of them, the rest still) is drawn walking: every picture a little further, never standing
// then jumping.
TEST(a_monsters_strides_are_drawn_over_their_time) {
  cl::ModelLerp l;
  const double tic = 1.0 / 72, pic = 1.0 / 60;
  double sv = 0, owed = 0, next_stride = 0.1;
  float x = 0, last = 0;
  float most = 0;
  int still = 0;
  for (int k = 0; k < 120; k++) {
    owed += pic;
    while (owed >= tic) {
      owed -= tic;
      sv += tic;
      if (sv >= next_stride) {
        x += 8;
        next_stride += 0.1;
      }
    }
    std::vector<cl::LerpModel> v{thing(60, 0, {x, 0, 0})};
    cl::lerp_models(l, v, k * pic, {.moves = true, .state_s = sv, .drawn_s = sv + owed - tic},
                    kBoth);
    if (k > 12) {
      const float step = v[0].origin.x - last;
      most = std::max(most, step);
      if (step < 1e-4f) still++;
    }
    last = v[0].origin.x;
  }
  // 80 u/s at 60 pictures a second is 1.33 units a picture; a jump would be the whole 8.
  CHECK_MSG(most < 3, std::to_string(most).c_str());
  CHECK_MSG(still < 5, std::to_string(still).c_str());
}

TEST(a_teleport_is_not_dragged_across_the_map) {
  cl::ModelLerp l;
  const double tic = 1.0 / 72;
  std::vector<cl::LerpModel> v{thing(2, 0, {0, 0, 0})};
  cl::lerp_models(l, v, 0, {.moves = true, .state_s = 0, .drawn_s = -tic}, kBoth);
  v = {thing(2, 0, {4, 0, 0})};
  cl::lerp_models(l, v, tic, {.moves = true, .state_s = tic, .drawn_s = tic * 0.5}, kBoth);
  CHECK(near(v[0].origin.x, 2));
  v = {thing(2, 0, {1000, 0, 0})};
  cl::lerp_models(l, v, 2 * tic, {.moves = true, .state_s = 2 * tic, .drawn_s = tic * 1.5}, kBoth);
  CHECK(v[0].origin.x == 1000);
}

// A held weapon's recorded frame (a demo, a joined server): blended from the last over its pace,
// alone once there, and started over when the clock runs back.
TEST(a_recorded_frame_is_drawn_between_its_changes) {
  cl::FrameTrack tr;
  int from = 0;
  float f = 0;
  cl::track_frame(tr, 0, 1.0, from, f);
  CHECK(from == -1 && f == 1);
  cl::track_frame(tr, 1, 1.1, from, f);
  CHECK(from == 0 && near(f, 0));
  cl::track_frame(tr, 1, 1.15, from, f);
  CHECK(from == 0 && near(f, 0.5f));
  cl::track_frame(tr, 2, 1.2, from, f);  // a pace of 0.1 s
  cl::track_frame(tr, 2, 1.275, from, f);
  CHECK(from == 1 && near(f, 0.75f));
  cl::track_frame(tr, 2, 1.4, from, f);
  CHECK(from == -1 && f == 1);
  cl::track_frame(tr, 5, 0.5, from, f);  // a seek back: nothing to come from
  CHECK(from == -1 && f == 1);
}
