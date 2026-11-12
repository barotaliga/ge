// MOVEMENT RECIPES READ OFF A TRACK (plugins/shooter/recipes.hpp): synthetic tracks on a flat
// floor at 77 frames a second. A stand, a run forward and three chained hops gaining speed read as
// one hop start from the last slow frame, made, its run's keys forward, three takeoffs; the same
// hops entered already running read as none; hops that never gain read as a start that stalled;
// a sudden rise in the air (a blast) is left out.
//
// ge:summary Tests: hop starts read off synthetic tracks: entry, made, stalled, running, blast
// ge:keywords test recipe hop-start read_hop_starts entry keys stall
// ge:skill shooter testing
#include <vector>

#include "check.hpp"
#include "shooter/brush_hull.hpp"
#include "shooter/recipes.hpp"

namespace sh = ge::shooter;

namespace {

constexpr double kDt = 1.0 / 77;
constexpr int kFlight = 52;   // frames in the air a hop
constexpr float kStand = 24;  // the origin's height over the floor

sh::Brush box(sh::Vec3f lo, sh::Vec3f hi) {
  sh::Brush b;
  b.planes = {{{1, 0, 0}, hi.x},   {{-1, 0, 0}, -lo.x}, {{0, 1, 0}, hi.y},
              {{0, -1, 0}, -lo.y}, {{0, 0, 1}, hi.z},   {{0, 0, -1}, -lo.z}};
  return b;
}

struct Floor {
  sh::HullData point, clip;
  sh::MoveWorld world;
  Floor() {
    const std::vector<sh::Brush> brushes = {box({-4096, -4096, -64}, {4096, 4096, 0})};
    point = sh::build_point_hull(brushes);
    clip = sh::build_box_hull(brushes, sh::kPlayerMins, sh::kPlayerMaxs);
    world.solids[0].point_hull = point.hull();
    world.solids[0].clip_hull = clip.hull();
    world.solids[0].mins = {-4096, -4096, -64};
    world.solids[0].maxs = {4096, 4096, 0};
    world.count = 1;
  }
};

// A track written frame by frame: x moves at the speed asked, z flies the hops.
struct Writer {
  std::vector<sh::TrackPoint> track;
  double t = 0;
  float x = 0, speed = 0;
  void frame(float z) {
    x += speed * static_cast<float>(kDt);
    t += kDt;
    track.push_back({t, {x, 0, z}, 0, 0, false});
  }
  void stand(int frames) {
    speed = 0;
    for (int i = 0; i < frames; i++) frame(kStand);
  }
  void run(float to, float accel) {
    while (speed < to) {
      speed = speed + accel * static_cast<float>(kDt) > to
                  ? to
                  : speed + accel * static_cast<float>(kDt);
      frame(kStand);
    }
  }
  // Hops chained on the landing frame, the speed across up `gain` a second in the air; `kick`:
  // a sudden rise of that much on a hop's 10th air frame.
  void hops(int count, float gain, float kick = 0, int kick_hop = -1) {
    const float rise = 400.f * kFlight * static_cast<float>(kDt);  // lands on frame kFlight
    for (int h = 0; h < count; h++) {
      for (int k = 1; k <= kFlight; k++) {
        const float s = static_cast<float>(k * kDt);
        speed += gain * static_cast<float>(kDt);
        if (h == kick_hop && k == 10) speed += kick;
        frame(k == kFlight ? kStand : kStand + rise * s - 400 * s * s);
      }
    }
    frame(kStand);  // the last landing, stood on
  }
};

std::vector<sh::HopStart> read_starts(const Floor& fl, const std::vector<sh::TrackPoint>& track) {
  sh::HopReading hops;
  sh::read_hops(track, fl.world, sh::HopSettings{}, hops);
  std::vector<sh::HopStart> out;
  sh::read_hop_starts(track, hops, sh::HopStartSettings{}, out);
  return out;
}

}  // namespace

TEST(a_stand_a_run_and_three_gaining_hops_read_as_one_made_hop_start) {
  Floor fl;
  Writer w;
  w.stand(30);
  w.run(320, 1100);
  w.hops(4, 80);
  const std::vector<sh::HopStart> s = read_starts(fl, w.track);
  CHECK(s.size() == 1u);
  if (s.empty()) return;
  const sh::HopStart& h = s[0];
  CHECK(h.speed_in < 250);
  CHECK(h.stood);
  CHECK(h.low < 1);
  CHECK(h.made);
  CHECK(h.takeoffs >= 3);
  CHECK(h.to_target_s > 0.8f && h.to_target_s < 2.0f);
  CHECK(h.run_keys == sh::kKeyForward);
  CHECK(h.speed_off > 300 && h.speed_off < 340);
  CHECK(h.frames.size() == static_cast<size_t>(h.end - h.low_frame + 1));
  CHECK(h.low_frame < h.entry);
  CHECK(h.frames.front().ground && h.frames.front().t < 0.f);
  CHECK(h.low_to_target_s > h.to_target_s);
  int jumps = 0;
  for (const sh::RecipeFrame& f : h.frames) jumps += f.jump ? 1 : 0;
  CHECK(jumps == h.takeoffs);
}

TEST(hops_entered_already_running_are_no_hop_start) {
  Floor fl;
  Writer w;
  w.stand(30);
  w.run(320, 1100);
  for (int i = 0; i < 154; i++) w.frame(kStand);  // two seconds at 320
  w.hops(4, 80);
  CHECK(read_starts(fl, w.track).empty());
}

TEST(hops_that_never_gain_read_as_a_start_that_stalled) {
  Floor fl;
  Writer w;
  w.stand(30);
  w.run(320, 1100);
  w.hops(4, 0);
  const std::vector<sh::HopStart> s = read_starts(fl, w.track);
  CHECK(s.size() == 1u);
  if (s.empty()) return;
  CHECK(!s[0].made);
  CHECK(s[0].to_target_s < 0);
  CHECK(s[0].top < 400);
}

TEST(a_blast_in_the_air_is_left_out) {
  Floor fl;
  Writer w;
  w.stand(30);
  w.run(320, 1100);
  w.hops(4, 0, 300, 1);
  CHECK(read_starts(fl, w.track).empty());
}
