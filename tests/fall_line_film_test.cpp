// Fall Line's directors (games/fall_line/camera/film.hpp): the lens names the shot rules use map
// to the film camera's lenses; over the first half minute of the bot's run on seed 38, a cut of
// a flight, a fall and the finish is planned into planted lenses that see their stretch, every
// pose is a pure function of the plan and the step (drawn between steps too), a planted lens
// stands still through its shot and pans smoothly; the trackside lenses stand right of the
// piste and off it, and the broadcast through them keeps to one side of his line.
//
// ge:summary Tests for Fall Line's film camera (lenses, pure poses, planting) and the broadcast
// ge:keywords test fall-line camera film replay director lens planted trackside broadcast
// ge:skill camera testing
#include <cmath>
#include <memory>
#include <vector>

#include "check.hpp"
#include "fall_line/camera/film.hpp"
#include "fall_line/camera/world.hpp"
#include "fall_line/sim/bot.hpp"
#include "fall_line/sim/moments.hpp"
#include "fall_line/sim/simulate.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/mapgen/query.hpp"

namespace {

namespace fc = fall_line::camera;
namespace cam = ge::camera;
namespace gr = ge::replay;

std::shared_ptr<const ge::mountain::mapgen::Level> map38() {
  static const auto level = [] {
    ge::mountain::mapgen::GenerateOptions opts;
    opts.version = 1;  // the app's map (fall_line/app/views.hpp kMapVersion)
    return std::make_shared<const ge::mountain::mapgen::Level>(
        ge::mountain::mapgen::generate_level(38, opts));
  }();
  return level;
}

// The bot's first 30 s on seed 38: the player's path, one point a step.
const std::vector<cam::PathPoint>& path38() {
  static const auto path = [] {
    fall_line::sim::Driver run(map38(), 0);
    std::vector<cam::PathPoint> p{fc::path_point(run.state().racers[0].skier)};
    while (run.state().t < 30 && !run.over()) {
      run.step(fall_line::sim::bot_controls(run.state(), 0, run.plan()));
      p.push_back(fc::path_point(run.state().racers[0].skier));
    }
    return p;
  }();
  return path;
}

}  // namespace

TEST(the_shot_rules_lenses_are_the_film_cameras) {
  for (const gr::ShotRule& r : fall_line::sim::shot_rules()) {
    const fc::FilmLens l = fc::film_lens_named(r.lens);
    CHECK(r.lens == fc::film_lens_name(l));
  }
  for (const fc::FilmLens l : {fc::FilmLens::side, fc::FilmLens::front, fc::FilmLens::aerial,
                               fc::FilmLens::impact, fc::FilmLens::chase}) {
    CHECK(fc::film_lens_named(fc::film_lens_name(l)) == l);
  }
  CHECK(fc::film_lens_named("no such lens") == fc::FilmLens::chase);
}

TEST(a_cut_is_filmed_through_planted_lenses_as_a_pure_function_of_the_step) {
  const auto level = map38();
  fall_line::sim::Driver run(level, 0);
  const ge::skier::World sw = run.state().world();
  const cam::World w = fc::camera_world(sw);
  const std::vector<cam::PathPoint>& path = path38();
  const auto length = static_cast<int64_t>(path.size()) - 1;
  CHECK(length > 3000);
  const double hz = fall_line::sim::kHz;
  const std::vector<gr::Mark> marks{{1200, "flight", 1}, {2160, "wipeout", 1}, {3400, "finish", 1}};
  const gr::Cut cut = gr::plan_cut(marks, fall_line::sim::shot_rules(),
                                   fall_line::sim::director_policy(), hz, length);
  CHECK(cut.shots.size() == 3);
  const fc::FilmPlan plan = fc::plan_film(cut, marks, path, w, hz);
  CHECK(plan.shots.size() == cut.shots.size());
  CHECK(plan.shots[0].lens == fc::FilmLens::side);
  CHECK(plan.shots[1].lens == fc::FilmLens::impact);
  CHECK(plan.shots[2].lens == fc::FilmLens::aerial);
  for (size_t k = 0; k < plan.shots.size(); ++k) {
    const fc::FilmShot& s = plan.shots[k];
    // Planted where it sees most of its stretch.
    CHECK(s.plant.candidate >= 0 && s.plant.seen > 0.8);
    // Pure: the same step twice is the same pose; between two steps, between their poses.
    const auto mid = static_cast<double>(s.mark);
    const cam::Pose a = fc::film_pose(plan, w, k, mid), b = fc::film_pose(plan, w, k, mid);
    CHECK(a.yaw == b.yaw && a.pitch == b.pitch && a.fov_y == b.fov_y && a.eye.x == b.eye.x);
    const cam::Pose next = fc::film_pose(plan, w, k, mid + 1);
    const cam::Pose half = fc::film_pose(plan, w, k, mid + 0.5);
    CHECK(std::fabs(ge::core::angle_diff(half.yaw, a.yaw)) <=
          std::fabs(ge::core::angle_diff(next.yaw, a.yaw)) + 1e-12);
    // A planted lens stands still and pans smoothly through its shot, over the snow.
    double most = 0;
    cam::Pose last = fc::film_pose(plan, w, k, static_cast<double>(s.from));
    for (int64_t i = s.from + 1; i <= s.to; ++i) {
      const cam::Pose p = fc::film_pose(plan, w, k, static_cast<double>(i));
      CHECK(p.eye.x == last.eye.x && p.eye.y == last.eye.y && p.eye.z == last.eye.z);
      const double turn = std::fabs(ge::core::angle_diff(p.yaw, last.yaw));
      most = turn > most ? turn : most;
      last = p;
    }
    CHECK(most < 0.02);  // rad a step at 120 Hz
    CHECK(last.eye.y > level->ground_at(last.eye.x, last.eye.z) + 0.5);
  }
}

TEST(the_chase_lens_follows_him_from_behind_over_the_snow) {
  const auto level = map38();
  fall_line::sim::Driver run(level, 0);
  const ge::skier::World sw = run.state().world();
  const cam::World w = fc::camera_world(sw);
  const std::vector<cam::PathPoint>& path = path38();
  gr::Cut cut;
  cut.shots.push_back({600, 1800, 0, "missed", "chase", 1, 1});
  const std::vector<gr::Mark> marks{{1200, "missed", 1}};
  const fc::FilmPlan plan = fc::plan_film(cut, marks, path, w, fall_line::sim::kHz);
  CHECK(plan.shots[0].lens == fc::FilmLens::chase);
  for (int64_t i = 600; i <= 1800; i += 30) {
    const cam::Pose p = fc::film_pose(plan, w, 0, static_cast<double>(i));
    const cam::PathPoint& him = path[static_cast<size_t>(i)];
    const double d = ge::core::hypot(p.eye.x - him.pos.x, p.eye.z - him.pos.z);
    CHECK(d > 4 && d < 9);
    CHECK(p.eye.y > level->ground_at(p.eye.x, p.eye.z) + 0.5);
  }
}

TEST(the_trackside_lenses_stand_right_of_the_piste_and_the_broadcast_keeps_his_line) {
  const auto level = map38();
  fall_line::sim::Driver run(level, 0);
  const ge::skier::World sw = run.state().world();
  const cam::World w = fc::camera_world(sw);
  fc::Broadcast b;
  b.lenses = fc::trackside_lenses(*level, w);
  CHECK(b.lenses.size() >= 10);
  for (const cam::PlantedRow& r : b.lenses) {
    const auto hit = ge::mountain::mapgen::nearest_track_point(level->track, r.eye.x, r.eye.z);
    CHECK(hit.lateral >= level->track.points[static_cast<size_t>(hit.index)].width * 0.5);
  }
  const std::vector<cam::PathPoint>& path = path38();
  for (size_t i = 0; i < path.size(); i += 2) {
    cam::Subject s;
    s.pos = path[i].pos;
    s.vel = path[i].vel;
    fc::broadcast_pose(b, *level, s, w, 2 / fall_line::sim::kHz);
  }
  CHECK(b.state.cuts >= 2);
  // Three cuts in the half minute, at most one across his line (a forced cut, sight lost);
  // over the whole race the camera-director lab counts 2 of 23.
  CHECK(b.state.crossed <= 1);
}
