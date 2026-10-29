// THE PHYSICS FRAME: thinking's time window to the bit (the worked examples: a step of 0.013 s
// at time 10: a think due at 10.010 runs with time 10.010, one at 10.013 runs, one at 10.0131
// waits, one at 9.5 runs with time 10.000; a think that sets 10.012 runs again in the same
// frame, one that sets 10.020 or 10.010 again does not); the toss under gravity landing on a
// floor and stopping, a bounce rebounding at 1.5 and resting below 60 up, fly and the fly-missile
// without gravity, a new missile run 0.05 s ahead once, the step falling and landing with its
// sound, noclip through walls, the water check, the velocity bound; and the frame's order:
// start_frame first, the players' slots skipped, an entity spawned this frame run this frame,
// a forced retouch counted down. A golden holds a toss, a bounce and a step frame by frame.
//
// ge:summary Tests: thinking's window, toss, bounce, fly, missiles, step, noclip, the frame order
// ge:keywords test physics run_think think-window toss bounce fly flymissile new-missile step
// ge:keywords noclip water bound_velocity physics_frame force_retouch golden
// ge:skill shooter testing
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "shooter/physics.hpp"
#include "shooter_loop_bench.hpp"

namespace sh = ge::shooter;
using ge::test::loop::Bench;
using ge::test::loop::box;

namespace {

const sh::Vec3f kZero{0, 0, 0};

constexpr const char* kGolden = "tests/golden/shooter_physics.txt";

bool env_set(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool set = _dupenv_s(&v, &n, name) == 0 && v != nullptr;
  std::free(v);
  return set;
#else
  return std::getenv(name) != nullptr;
#endif
}

// A thinker whose think records the program's time and sets the next think from `next`.
struct Thinker {
  Bench b;
  int e = 0;
  std::vector<float> times;
  std::vector<float> next;  // each think's new nextthink, in turn (0 when they run out)

  explicit Thinker(float first) {
    const sh::Func think = b.add([this](sh::Server& sv) {
      times.push_back(sv.g.time);
      const size_t i = times.size() - 1;
      sv.edicts[static_cast<size_t>(sv.g.self)].nextthink = i < next.size() ? next[i] : 0;
    });
    b.start();
    b.sv.time = 10.0;
    e = sh::spawn_edict(b.sv);
    b.ent(e).think = think;
    b.ent(e).nextthink = first;
  }

  void step() { sh::run_think(b.sv, e, 0.013); }
};

}  // namespace

TEST(a_think_runs_when_due_by_the_end_of_the_step) {
  Thinker a(10.010f);
  a.step();
  CHECK(a.times.size() == 1);
  CHECK_BITS(a.times[0], 10.010f);
  CHECK(a.b.ent(a.e).nextthink == 0);

  Thinker at_end(10.013f);  // float 10.013 is 10.01299..., inside the window
  at_end.step();
  CHECK(at_end.times.size() == 1);

  Thinker after(10.0131f);
  after.step();
  CHECK(after.times.empty());
  CHECK(after.b.ent(after.e).nextthink == 10.0131f);

  Thinker late(9.5f);  // overdue: runs with the server's time, not its own
  late.step();
  CHECK(late.times.size() == 1);
  CHECK_BITS(late.times[0], 10.0f);
}

TEST(a_think_runs_again_in_a_frame_only_when_it_moves_forward_inside_the_window) {
  Thinker again(10.010f);
  again.next = {10.012f};
  again.step();
  CHECK(again.times.size() == 2);
  CHECK_BITS(again.times[1], 10.012f);

  Thinker beyond(10.010f);
  beyond.next = {10.020f};
  beyond.step();
  CHECK(beyond.times.size() == 1);
  CHECK(beyond.b.ent(beyond.e).nextthink == 10.020f);

  Thinker same(10.010f);
  same.next = {10.010f};
  same.step();
  CHECK(same.times.size() == 1);
  CHECK(same.b.ent(same.e).nextthink == 10.010f);  // runs next frame
  same.b.sv.time = 10.013;
  same.step();
  CHECK(same.times.size() == 2);
}

TEST(a_think_that_frees_its_entity_stops_the_entitys_physics) {
  Bench b;
  const sh::Func remove = b.add([](sh::Server& sv) { sh::free_edict(sv, sv.g.self); });
  b.start();
  const int e =
      b.spawn_box({0, 0, 100}, {-4, -4, -4}, {4, 4, 4}, sh::kSolidBbox, sh::kMovetypeToss);
  b.ent(e).velocity = {100, 0, 0};
  b.ent(e).think = remove;
  b.ent(e).nextthink = 1.0f;
  sh::run_entity(b.sv, e, 0.013);
  CHECK(b.ent(e).free);
}

TEST(a_toss_falls_lands_and_stops_on_a_floor) {
  Bench b;
  b.start();
  const int e = b.spawn_box({0, 0, 40}, {-4, -4, -4}, {4, 4, 4}, sh::kSolidBbox, sh::kMovetypeToss);
  b.ent(e).velocity = {50, 0, 0};
  sh::run_entity(b.sv, e, 0.013);
  // The gravity in float x the step in double, rounded once.
  CHECK_BITS(b.ent(e).velocity.z, static_cast<float>(0.0 - 800.0f * 0.013));
  CHECK_BITS(b.ent(e).origin.x, 50 * static_cast<float>(0.013));
  for (int i = 0; i < 200 && (b.ent(e).flags & sh::kFlagOnGround) == 0; i++) {
    b.sv.time += 0.013;
    sh::run_entity(b.sv, e, 0.013);
  }
  CHECK((b.ent(e).flags & sh::kFlagOnGround) != 0);
  CHECK(b.ent(e).groundentity == 0);
  CHECK(b.ent(e).velocity.x == 0 && b.ent(e).velocity.z == 0);
  const float rest = b.ent(e).origin.z;
  CHECK(rest > 4 && rest < 4.1f);
  b.sv.time += 0.013;
  sh::run_entity(b.sv, e, 0.013);
  CHECK(b.ent(e).origin.z == rest);  // on the ground, nothing moves it
}

TEST(a_bounce_rebounds_at_one_and_a_half_and_rests_once_slow) {
  Bench b;
  b.start();
  const int e = b.spawn_box({0, 0, 200}, kZero, kZero, sh::kSolidBbox, sh::kMovetypeBounce);
  int bounces = 0;
  for (int i = 0; i < 2000 && (b.ent(e).flags & sh::kFlagOnGround) == 0; i++) {
    b.sv.time += 0.013;
    const float vz = b.ent(e).velocity.z;
    sh::HullTrace last;
    sh::physics_toss(b.sv, e, 0.013, &last);
    if (last.fraction < 1 && (b.ent(e).flags & sh::kFlagOnGround) == 0) {
      bounces++;
      // Gravity first, then the clip: up at half the speed it came down with.
      const float in = static_cast<float>(static_cast<double>(vz) - 800.0f * 0.013);
      CHECK(std::fabs(b.ent(e).velocity.z + 0.5f * in) < 0.01f);
      CHECK(b.ent(e).velocity.z >= 60);
    }
  }
  CHECK(bounces >= 3);
  CHECK((b.ent(e).flags & sh::kFlagOnGround) != 0);
  CHECK(b.ent(e).velocity.z == 0);
}

TEST(fly_and_the_fly_missile_feel_no_gravity) {
  for (int type : {sh::kMovetypeFly, sh::kMovetypeFlyMissile}) {
    Bench b;
    b.start();
    const int e = b.spawn_box({0, 0, 100}, kZero, kZero, sh::kSolidBbox, type);
    b.ent(e).velocity = {100, 0, 0};
    sh::run_entity(b.sv, e, 0.013);
    CHECK(b.ent(e).velocity.z == 0);
    CHECK(b.ent(e).origin.z == 100);
  }
}

TEST(a_toss_touches_what_it_hits_and_is_touched_back) {
  Bench b;
  std::vector<std::pair<int, int>> seen;
  const sh::Func touch = b.add([&](sh::Server& sv) { seen.push_back({sv.g.self, sv.g.other}); });
  b.start();
  const int target =
      b.spawn_box({100, 0, 50}, {-16, -16, -16}, {16, 16, 16}, sh::kSolidBbox, sh::kMovetypeNone);
  b.ent(target).touch = touch;
  const int rocket = b.spawn_box({0, 0, 50}, kZero, kZero, sh::kSolidBbox, sh::kMovetypeFlyMissile);
  b.ent(rocket).touch = touch;
  b.ent(rocket).velocity = {1000, 0, 0};
  for (int i = 0; i < 20 && seen.empty(); i++) {
    b.sv.time += 0.013;
    sh::run_entity(b.sv, rocket, 0.013);
  }
  CHECK(seen.size() == 2);
  CHECK((seen[0] == std::pair<int, int>{rocket, target}));
  CHECK((seen[1] == std::pair<int, int>{target, rocket}));
  CHECK(std::fabs(b.ent(rocket).origin.x - 84) < 0.05f);
}

TEST(a_new_missile_runs_005_s_ahead_once_in_its_frame) {
  Bench b;
  int missile = 0;
  const sh::Func spawn = b.add([&](sh::Server& sv) {
    if (missile != 0) return;
    missile = sh::spawn_edict(sv);
    sh::Edict& m = sv.edicts[static_cast<size_t>(missile)];
    m.solid = sh::kSolidBbox;
    m.movetype = sh::kMovetypeFlyMissile;
    m.velocity = {1000, 0, 0};
    sh::set_origin(sv, missile, {0, 0, 100});
    sv.g.newmis = missile;
  });
  sh::Program prog;
  prog.start_frame = spawn;
  b.start({box({-1024, -1024, -64}, {1024, 1024, 0})}, {}, {}, prog);
  b.sv.time = 1.013;
  sh::physics_frame(b.sv, 0.013);
  CHECK(b.sv.counts.new_missiles == 1);
  CHECK(b.sv.g.newmis == 0);
  CHECK_BITS(b.ent(missile).origin.x, 1000 * static_cast<float>(0.05));
  b.sv.time += 0.013;
  sh::physics_frame(b.sv, 0.013);
  CHECK_BITS(b.ent(missile).origin.x,
             1000 * static_cast<float>(0.05) + 1000 * static_cast<float>(0.013));
}

TEST(the_step_falls_lands_with_its_sound_and_then_only_thinks) {
  Bench b;
  b.start();
  const int e =
      b.spawn_box({0, 0, 120}, {-8, -8, -8}, {8, 8, 8}, sh::kSolidSlidebox, sh::kMovetypeStep);
  bool landed_sound = false;
  for (int i = 0; i < 200 && (b.ent(e).flags & sh::kFlagOnGround) == 0; i++) {
    b.sv.time += 0.013;
    b.sv.sound_count = 0;
    sh::run_entity(b.sv, e, 0.013);
    for (int s = 0; s < b.sv.sound_count; s++)
      if (std::string(b.sv.sounds[s].sample) == sh::kSoundLand) landed_sound = true;
  }
  CHECK((b.ent(e).flags & sh::kFlagOnGround) != 0);
  CHECK(landed_sound);
  CHECK(std::fabs(b.ent(e).origin.z - 8) < 0.05f);
  // Pushed sideways on the ground it does not move: the step only falls.
  b.ent(e).velocity = {100, 0, 0};
  const float x = b.ent(e).origin.x;
  b.sv.time += 0.013;
  sh::run_entity(b.sv, e, 0.013);
  CHECK(b.ent(e).origin.x == x);
}

TEST(the_fly_move_slides_along_a_wall_and_reports_it) {
  Bench b;
  b.start({box({-1024, -1024, -64}, {1024, 1024, 0}), box({1000, -1024, 0}, {1024, 1024, 256})});
  const int e =
      b.spawn_box({900, 0, 100}, {-8, -8, -8}, {8, 8, 8}, sh::kSolidSlidebox, sh::kMovetypeStep);
  b.ent(e).velocity = {1000, 500, 0};
  const int blocked = sh::fly_move(b.sv, e, 0.2f);
  CHECK(blocked == 2);
  // A 16-wide box meets brushes through the 32-wide hull laid from its mins: its far side
  // stops 16 short of the wall, at 1000 - 8 - 16.
  CHECK(std::fabs(b.ent(e).origin.x - 976) < 0.05f);
  CHECK(b.ent(e).velocity.x == 0);
  CHECK(b.ent(e).velocity.y == 500);
  CHECK(std::fabs(b.ent(e).origin.y - 100) < 0.05f);  // the whole step's slide along it
  // Straight at it: stopped.
  b.ent(e).velocity = {1000, 0, 0};
  sh::fly_move(b.sv, e, 0.1f);
  CHECK(b.ent(e).velocity.x == 0 && b.ent(e).velocity.y == 0);
}

TEST(noclip_moves_through_everything) {
  Bench b;
  b.start();
  const int e =
      b.spawn_box({0, 0, 10}, {-8, -8, -8}, {8, 8, 8}, sh::kSolidNot, sh::kMovetypeNoclip);
  b.ent(e).velocity = {0, 0, -1000};
  b.ent(e).avelocity = {0, 90, 0};
  sh::run_entity(b.sv, e, 0.1);
  CHECK_BITS(b.ent(e).origin.z, 10 + -1000 * static_cast<float>(0.1));
  CHECK_BITS(b.ent(e).angles.y, 90 * static_cast<float>(0.1));
}

TEST(the_water_check_is_silent_first_then_splashes_both_ways) {
  Bench b;
  sh::Brush water = box({-1024, -1024, 0}, {0, 1024, 64});
  water.contents = sh::kContentsWater;
  b.start({box({-1024, -1024, -64}, {1024, 1024, 0}), water});
  const int e = b.spawn_box({100, 0, 32}, kZero, kZero, sh::kSolidBbox, sh::kMovetypeToss);
  sh::check_water_transition(b.sv, e);
  CHECK(b.sv.sound_count == 0);
  CHECK(b.ent(e).watertype == sh::kContentsEmpty);
  b.ent(e).origin = {-100, 0, 32};
  sh::check_water_transition(b.sv, e);
  CHECK(b.sv.sound_count == 1);
  CHECK(b.ent(e).watertype == sh::kContentsWater && b.ent(e).waterlevel == 1);
  b.ent(e).origin = {100, 0, 32};
  sh::check_water_transition(b.sv, e);
  CHECK(b.sv.sound_count == 2);
  CHECK(b.ent(e).watertype == sh::kContentsEmpty);
}

TEST(the_velocity_is_bounded_by_its_length_and_cleared_of_nans) {
  Bench b;
  b.start();
  const int e = b.spawn_box({0, 0, 100}, kZero, kZero, sh::kSolidBbox, sh::kMovetypeToss);
  b.ent(e).velocity = {3000, 0, 4000};
  sh::bound_velocity(b.sv, e);
  CHECK(std::fabs(b.ent(e).velocity.x - 1200) < 0.01f);
  CHECK(std::fabs(b.ent(e).velocity.z - 1600) < 0.01f);
  b.ent(e).velocity = {std::nanf(""), 10, 0};
  sh::bound_velocity(b.sv, e);
  CHECK(b.ent(e).velocity.x == 0 && b.ent(e).velocity.y == 10);
}

TEST(the_frame_starts_the_program_skips_players_and_runs_what_it_spawns) {
  Bench b;
  std::vector<int> order;
  int spawned = 0;
  const sh::Func think = b.add([&](sh::Server& sv) { order.push_back(sv.g.self); });
  const sh::Func start = b.add([&](sh::Server& sv) {
    order.push_back(-1);
    if (spawned == 0) {
      spawned = sh::spawn_edict(sv);
      sv.edicts[static_cast<size_t>(spawned)].think = think;
      sv.edicts[static_cast<size_t>(spawned)].nextthink = static_cast<float>(sv.time);
    }
  });
  sh::Program prog;
  prog.start_frame = start;
  sh::ServerSettings set;
  set.maxclients = 2;
  b.start({box({-1024, -1024, -64}, {1024, 1024, 0})}, {}, set, prog);
  // A thinker in a player's slot: the frame leaves it to its packets.
  b.ent(1).free = false;
  b.ent(1).think = think;
  b.ent(1).nextthink = 1.0f;
  sh::physics_frame(b.sv, 0.1);
  CHECK((order == std::vector<int>{-1, spawned}));
  CHECK(b.sv.counts.physics_frames == 1);
  CHECK_BITS(b.sv.g.frametime, 0.1f);
}

TEST(a_forced_retouch_relinks_everything_with_touches_and_counts_down) {
  Bench b;
  int touches = 0;
  const sh::Func touch = b.add([&](sh::Server&) { touches++; });
  b.start();
  const int trig =
      b.spawn_box({0, 0, 32}, {-32, -32, -32}, {32, 32, 32}, sh::kSolidTrigger, sh::kMovetypeNone);
  b.ent(trig).touch = touch;
  b.spawn_box({0, 0, 32}, {-8, -8, -8}, {8, 8, 8}, sh::kSolidBbox, sh::kMovetypeNone);
  CHECK(touches == 0);  // set_origin links without touches
  b.sv.g.force_retouch = 2;
  sh::physics_frame(b.sv, 0.013);
  CHECK(touches == 1);
  CHECK(b.sv.g.force_retouch == 1);
  b.sv.time += 0.013;
  sh::physics_frame(b.sv, 0.013);
  CHECK(touches == 2);
  CHECK(b.sv.g.force_retouch == 0);
  b.sv.time += 0.013;
  sh::physics_frame(b.sv, 0.013);
  CHECK(touches == 2);
}

// The golden: a toss thrown up and across onto two stairs, a bounce off a wall, a step falling
// onto a stair, every 10th of 300 frames of 0.013 s, each field as its bits.
TEST(the_physics_holds_its_golden) {
  Bench b;
  b.start({box({-1024, -1024, -64}, {1024, 1024, 0}), box({200, -1024, 0}, {1024, 1024, 40}),
           box({300, -1024, 40}, {1024, 1024, 80}), box({-600, -1024, 0}, {-560, 1024, 256})});
  const int toss =
      b.spawn_box({0, 0, 50}, {-4, -4, -4}, {4, 4, 4}, sh::kSolidBbox, sh::kMovetypeToss);
  b.ent(toss).velocity = {320, 45, 270};
  const int bounce = b.spawn_box({0, 100, 50}, kZero, kZero, sh::kSolidBbox, sh::kMovetypeBounce);
  b.ent(bounce).velocity = {-600, -30, 400};
  b.ent(bounce).avelocity = {300, 20, 0};
  const int step = b.spawn_box({250, -100, 300}, {-16, -16, -24}, {16, 16, 40}, sh::kSolidSlidebox,
                               sh::kMovetypeStep);
  b.ent(step).velocity = {80, 0, 0};
  std::vector<std::vector<uint64_t>> got;
  for (int frame = 0; frame < 300; frame++) {
    b.sv.time += 0.013;
    sh::physics_frame(b.sv, 0.013);
    if (frame % 10 != 9) continue;
    for (int e : {toss, bounce, step}) {
      const sh::Edict& ed = b.ent(e);
      std::vector<uint64_t> row = {static_cast<uint64_t>(frame), static_cast<uint64_t>(e)};
      for (float v : {ed.origin.x, ed.origin.y, ed.origin.z, ed.velocity.x, ed.velocity.y,
                      ed.velocity.z, ed.angles.x, ed.angles.y})
        row.push_back(ge::test::bits(v));
      row.push_back(static_cast<uint64_t>(ed.flags));
      got.push_back(row);
    }
  }
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# plugins/shooter/physics.hpp's own record: frame, entity, origin, velocity, "
           "pitch, yaw (double bits), flags (tests/shooter_physics_test.cpp)\n";
    for (const auto& row : got) {
      std::string line;
      for (uint64_t w : row) {
        char word[24];
        std::snprintf(word, sizeof word, "%llx ", static_cast<unsigned long long>(w));
        line += word;
      }
      line.back() = '\n';
      out << line;
    }
    std::printf("wrote %s (%zu rows)\n", kGolden, got.size());
    return;
  }
  const auto rows = ge::test::golden_rows(kGolden);
  CHECK_MSG(rows.size() == got.size(), std::string(kGolden) + " has the wrong row count");
  for (size_t i = 0; i < rows.size() && i < got.size(); i++)
    CHECK_MSG(rows[i] == got[i], "the physics moved off its golden at frame " +
                                     std::to_string(got[i][0]) + ", entity " +
                                     std::to_string(got[i][1]) +
                                     ": if the change is meant, rewrite with GE_GOLDEN_WRITE=1 "
                                     "and say why in the PR");
}
