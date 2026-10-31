// Tests: a single-player client's own data each frame (games/shooters_edge/server/client_data.hpp)
// and what the window does with it: the ideal pitch traced off the floor ahead on hand-built
// worlds (level ground, a ramp down and up, a single step, the air, a drop with no floor); the
// blow the program reported taken and cleared once, with its source, reaching the listen client;
// a multiplayer program's damage fields left alone; the punch the program sets carried to the loop
// and decayed there; and lookspring drifting the pitch only while the player moves on the ground
// with the mouse still.
//
// ge:summary Tests: the ideal pitch, the blow taken and cleared, the punch, lookspring
// ge:keywords test client-data ideal-pitch slope ramp dmg_take dmg_save dmg_inflictor blow
// ge:keywords punchangle lookspring spring_view take_client_data
// ge:skill shooter testing
#include <cmath>
#include <memory>
#include <string>

#include "check.hpp"
#include "progs/vm.hpp"
#include "shooter_loop_bench.hpp"
#include "shooters_edge/client/controls.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/server/client_data.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"

namespace {

namespace sh = ge::shooter;
namespace cl = shooters_edge::client;
namespace sv = shooters_edge::server;
namespace pg = ge::progs;
using ge::test::loop::Bench;
using ge::test::loop::box;

bool near(float a, float b, float tol = 0.01f) { return std::fabs(a - b) <= tol; }

// A ramp falling `drop` units a unit of x from x = `from` on, down to z = -600.
sh::Brush ramp(float from, float drop) {
  sh::Brush b = box({from, -1024, -600}, {1024, 1024, 0});
  const float len = std::sqrt(1 + drop * drop);
  b.planes[4] = {{drop / len, 0, 1 / len}, drop * from / len, 3};
  return b;
}

// A player standing at `at` (his origin; the box's bottom 24 under it), facing `yaw` degrees.
int player(Bench& b, sh::Vec3f at, float yaw, bool grounded = true) {
  const int e =
      b.spawn_box(at, {-16, -16, -24}, {16, 16, 32}, sh::kSolidSlidebox, sh::kMovetypeWalk);
  sh::Edict& ed = b.ent(e);
  ed.view_ofs = {0, 0, 22};
  ed.angles = {0, yaw, 0};
  if (grounded) ed.flags |= sh::kFlagOnGround;
  return e;
}

std::unique_ptr<cl::LoadedMap> stub_map(const std::string& name) {
  auto m = std::make_unique<cl::LoadedMap>();
  m->name = name;
  m->spec.name = name;
  m->spec.models = sv::arena_models();
  m->spec.entities = sv::kStubEntities;
  return m;
}

float field_f(const sv::QcServer& s, int e, int f) {
  return pg::as_float(*pg::entity_field(s.q.vm, e, f));
}
void set_f(sv::QcServer& s, int e, int f, float v) {
  *pg::entity_field(s.q.vm, e, f) = pg::as_word(v);
}

}  // namespace

TEST(level_ground_sets_the_ideal_pitch_to_zero) {
  Bench b;
  b.start();
  const int e = player(b, {0, 0, 24}, 0);
  CHECK(sv::ideal_pitch(b.sv, e, 7, sv::kIdealPitchScale) == 0);
}

TEST(a_ramp_tilts_the_ideal_pitch_down_going_down_and_up_going_up) {
  Bench b;
  b.start({box({-1024, -1024, -64}, {30, 1024, 0}), ramp(30, 0.25f)});
  // Down: the floor falls 3 units every 12 ahead, so minus -3 times 0.8.
  const int down = player(b, {0, 0, 24}, 0);
  const float d = sv::ideal_pitch(b.sv, down, 0, sv::kIdealPitchScale);
  CHECK_MSG(near(d, 2.4f), std::to_string(d));
  // Up: on the ramp facing back toward the top, every point ahead still on the ramp.
  const int up = player(b, {300, 0, 24 - 67.5f}, 180);
  const float u = sv::ideal_pitch(b.sv, up, 0, sv::kIdealPitchScale);
  CHECK_MSG(near(u, -2.4f), std::to_string(u));
  CHECK(near(sv::ideal_pitch(b.sv, down, 0, 0.5f), 1.5f));  // the scale is the cvar's
}

TEST(a_floor_that_is_not_one_slope_leaves_the_ideal_as_it_was) {
  Bench b;
  // One step down of 8 units at x = 70: a single step is no slope.
  b.start({box({-1024, -1024, -64}, {70, 1024, 0}), box({70, -1024, -64}, {1024, 1024, -8})});
  const int e = player(b, {0, 0, 24}, 0);
  CHECK(sv::ideal_pitch(b.sv, e, 3, sv::kIdealPitchScale) == 3);
  // In the air nothing is traced.
  const int flying = player(b, {0, 200, 60}, 0, false);
  CHECK(sv::ideal_pitch(b.sv, flying, 5, sv::kIdealPitchScale) == 5);
}

TEST(a_drop_with_no_floor_in_reach_leaves_the_ideal_as_it_was) {
  Bench b;
  b.start({box({-1024, -1024, -64}, {60, 1024, 0}), box({60, -1024, -900}, {1024, 1024, -400})});
  const int e = player(b, {0, 0, 24}, 0);
  CHECK(sv::ideal_pitch(b.sv, e, -2, sv::kIdealPitchScale) == -2);
}

TEST(a_blow_is_taken_once_with_its_source_and_reaches_the_listen_client) {
  pg::Progs stub;
  std::string error;
  CHECK_MSG(sv::build_campaign_stub(stub, error), error);
  cl::Listen l;
  cl::ListenOptions opt;
  opt.console = {"deathmatch 0"};
  CHECK_MSG(cl::listen_start(l, stub, stub_map("first"), opt, error), error);
  sv::QcServer& s = *l.server;
  const pg::LayoutFields& f = s.q.layout->f;
  CHECK(f.idealpitch >= 0);
  // A blow from the world (its box's centre) of 12 blood and 300 armour (a byte: 255).
  set_f(s, 1, f.dmg_take, 12);
  set_f(s, 1, f.dmg_save, 300);
  cl::listen_frame(l, {});
  CHECK(l.blows.size() == 1);
  CHECK(l.blows[0].blood == 12 && l.blows[0].armor == 255);
  const sh::Edict& world = s.sv.edicts[0];
  CHECK(l.blows[0].from.x == (world.mins.x + world.maxs.x) * 0.5f);
  CHECK(field_f(s, 1, f.dmg_take) == 0 && field_f(s, 1, f.dmg_save) == 0);
  CHECK(s.clients[0].blow.hit);
  cl::listen_frame(l, {});
  CHECK(l.blows.size() == 1);  // cleared: no second blow
  CHECK(!s.clients[0].blow.hit);
  // The stub's arena floor is level, so a player standing on it has an ideal of 0.
  if ((s.sv.edicts[1].flags & sh::kFlagOnGround) != 0) CHECK(cl::player_state(l).ideal_pitch == 0);
}

TEST(a_multiplayer_programs_damage_fields_are_left_alone) {
  pg::Progs mod;
  std::string error;
  CHECK_MSG(sv::build_test_mod(mod, error), error);
  cl::Listen l;
  CHECK_MSG(cl::listen_start(l, mod, stub_map("arena"), {}, error), error);
  sv::QcServer& s = *l.server;
  const int take = s.q.layout->f.dmg_take;
  CHECK(s.q.layout->f.idealpitch < 0);
  set_f(s, 1, take, 9);
  cl::listen_frame(l, {});
  CHECK(field_f(s, 1, take) == 9);
  CHECK(l.blows.empty());
}

TEST(the_punch_reaches_the_program_and_decays_on_the_loop) {
  pg::Progs stub;
  std::string error;
  CHECK_MSG(sv::build_campaign_stub(stub, error), error);
  cl::Listen l;
  cl::ListenOptions opt;
  opt.console = {"deathmatch 0"};
  CHECK_MSG(cl::listen_start(l, stub, stub_map("first"), opt, error), error);
  sv::QcServer& s = *l.server;
  s.sv.edicts[1].movetype = sh::kMovetypeWalk;  // the stub leaves its player standing still
  s.sv.edicts[1].punchangle = {-2, 0, 0};       // what the program's fire leaves after its call
  cl::listen_frame(l, {});
  const float after = s.sv.edicts[1].punchangle.x;
  CHECK_MSG(after > -2 && after < 0, std::to_string(after));
  // 10 degrees a second over the campaign's frame.
  CHECK_MSG(near(after, static_cast<float>(-2 + 10 * sv::kCampaignStep), 1e-4f),
            std::to_string(after));
  // The program sees the decayed kick too, and the window's view reads it.
  const float seen = pg::as_float(pg::entity_field(s.q.vm, 1, s.q.layout->f.punchangle)[0]);
  CHECK_MSG(near(seen, after, 1e-5f), std::to_string(seen));
  CHECK(cl::player_state(l).punch.x == after);
  // The program's view offset is the loop's eye (the original move's water level reads it).
  CHECK(s.sv.edicts[1].view_ofs.z ==
        pg::as_float(pg::entity_field(s.q.vm, 1, s.q.layout->f.view_ofs)[2]));
}

TEST(lookspring_drifts_the_pitch_only_while_moving_on_the_ground_with_the_mouse_still) {
  cl::ControlSettings c;
  ge::input::InputState in;
  sh::MoveCmd walk;
  walk.forward = 400;
  sh::Vec3f view{20, 90, 0};
  cl::spring_view(in, c, true, walk, 0, 0.1, view);
  CHECK(view.x == 20);  // off by default
  c.lookspring = true;
  cl::spring_view(in, c, true, walk, 0, 0.1, view);
  CHECK(near(view.x, 20 - static_cast<float>(cl::kSpringSpeed * 0.1)));
  cl::spring_view(in, c, true, walk, 4.8f, 1, view);
  CHECK(near(view.x, 4.8f));  // reaches the ideal and holds it
  cl::spring_view(in, c, false, walk, 0, 1, view);
  CHECK(near(view.x, 4.8f));  // in the air
  cl::spring_view(in, c, true, sh::MoveCmd{}, 0, 1, view);
  CHECK(near(view.x, 4.8f));  // standing still
  in.mouse.dy = 3;
  cl::spring_view(in, c, true, walk, 0, 1, view);
  CHECK(near(view.x, 4.8f));  // the mouse is pitching the view
}

TEST(a_blow_kicks_the_view_away_from_its_source) {
  pg::Progs stub;
  std::string error;
  CHECK_MSG(sv::build_campaign_stub(stub, error), error);
  const auto look = [&](bool hit) {
    cl::Listen l;
    cl::ListenOptions opt;
    opt.console = {"deathmatch 0"};
    CHECK_MSG(cl::listen_start(l, stub, stub_map("first"), opt, error), error);
    cl::listen_frame(l, {});
    cl::ViewState v;
    cl::frame_view(l, v, 1, 0.01, 0.01);
    if (hit) {  // from straight ahead and to the right of where the player looks
      const sh::Edict& me = l.server->sv.edicts[1];
      const double yaw = static_cast<double>(l.view.y) * 3.14159265358979323846 / 180;
      const auto c = static_cast<float>(std::cos(yaw)), sn = static_cast<float>(std::sin(yaw));
      l.blows.push_back(
          {20,
           20,
           {me.origin.x + 100 * c + 100 * sn, me.origin.y + 100 * sn - 100 * c, me.origin.z}});
    }
    cl::frame_view(l, v, 1, 0.02, 0.01);
    return v.pose;
  };
  const ge::camera::Pose before = look(false), after = look(true);
  // A blow from the front pitches the view up; one from the right rolls it.
  CHECK_MSG(after.pitch > before.pitch + 0.01, std::to_string(after.pitch - before.pitch));
  CHECK_MSG(std::fabs(after.roll - before.roll) > 0.01, std::to_string(after.roll - before.roll));
}
