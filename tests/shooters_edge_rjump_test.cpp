// THE ROCKET JUMP HELPER ON THE WINDOW'S SERVER: +rjump's request (impulse 164 with a jump) fires a
// real rocket straight down as the player jumps, the launcher chosen for it, the player's own view
// never turned, and he climbs far above a plain jump; +rrjump fires behind him and throws him
// forward; a press inside the launcher's refire waits for it; with the server's setting off he is
// told and nothing fires, unless he is granted the helpers (*rjump 1); the sequel's rules do the
// same on a map of brushes.
//
// ge:summary Tests: the rocket jump helpers on the listen server, both rules, gated and granted
// ge:keywords test rocket-jump rjump rrjump sv_rjump sv_rrjump grant listen-server sequel
// ge:skill shooter testing
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "check.hpp"
#include "shooter/rjump_assist.hpp"
#include "shooter/sequel_move.hpp"
#include "shooters_edge/client/controls.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/server/sequel_server.hpp"
#include "vfs/pak.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace br = ge::brush;
namespace sh = ge::shooter;

br::Entity entity(std::initializer_list<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  for (const auto& p : pairs) e.pairs.push_back(p);
  return e;
}

std::vector<uint8_t> pack(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files) {
  std::vector<uint8_t> out;
  std::string error;
  CHECK_MSG(ge::vfs::write_pak(files, &out, &error), error);
  return out;
}

const ge::progs::Progs& baseline() {
  static const ge::progs::Progs p = [] {
    ge::progs::Progs out;
    std::string error;
    CHECK_MSG(cl::baseline_progs(out, error), error);
    return out;
  }();
  return p;
}

bool start_on(cl::Listen& l, const std::string& name, std::vector<uint8_t> bsp) {
  cl::SearchPath files;
  std::string error;
  CHECK(
      cl::add_archive(files, "pak0.pak", pack({{"maps/" + name + ".bsp", std::move(bsp)}}), error));
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, name, error);
  CHECK_MSG(map != nullptr, error);
  if (!map) return false;
  const bool ok =
      cl::listen_start(l, baseline(), std::move(map), shooters_edge::net::listen_options(), error);
  CHECK_MSG(ok, error);
  return ok;
}

// The fixture halls (the classic family): a deathmatch spot facing east over a rocket launcher.
bool start_classic(cl::Listen& l) {
  br::Fixture f = br::sample_fixture();
  f.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "128 192 40"}, {"angle", "0"}}));
  f.entities.push_back(entity({{"classname", "weapon_rocketlauncher"}, {"origin", "128 192 0"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_bsp(f, br::Family::v29, &bytes, &error), error);
  return start_on(l, "halls", bytes);
}

struct Run {
  cl::Listen& l;
  float peak = 0;
  void frames(int n, sh::MoveCmd cmd = {}) {
    for (int i = 0; i < n; i++) {
      cl::listen_frame(l, cmd);
      cmd.impulse = 0;
      cmd.buttons = uint8_t(cmd.buttons & ~sh::kButtonJump);  // a press is one command
      peak = std::max(peak, l.server->world.entities[1].origin.z);
    }
  }
  float z() const { return l.server->world.entities[1].origin.z; }
  sh::Vec3f velocity() const { return l.server->sv.edicts[1].velocity; }
  float rockets() const { return cl::field_named(l, 1, "ammo_rockets"); }
};

sh::MoveCmd ask(sh::Rjump kind) {
  sh::MoveCmd c;
  c.impulse = sh::rjump_impulse(kind);
  c.buttons = sh::kButtonJump;  // the button sends the jump with its request
  return c;
}

bool told(const cl::Listen& l, const std::string& words) {
  for (const std::string& p : l.prints)
    if (p.find(words) != std::string::npos) return true;
  return false;
}

// Spawned on the launcher, picked up, the axe in hand (so the helper must choose the launcher).
bool ready_classic(cl::Listen& l, Run& r) {
  if (!start_classic(l)) return false;
  r.frames(80);  // the spawn, the fall onto the launcher, picked up
  l.impulse = 1;
  r.frames(80);
  CHECK(cl::field_named(l, 1, "weapon") != 32);
  return true;
}

}  // namespace

TEST(rjump_fires_a_rocket_straight_down_as_the_player_jumps_and_he_climbs_far) {
  cl::Listen l;
  Run r{l};
  if (!ready_classic(l, r)) return;
  // A plain jump's height to compare with.
  const float ground = r.z();
  r.peak = ground;
  sh::MoveCmd jump;
  jump.buttons = sh::kButtonJump;
  r.frames(1, jump);
  r.frames(90);
  const float plain = r.peak - ground;
  const float rockets = r.rockets();
  const float health = l.server->sv.edicts[1].health;
  const sh::Vec3f view = l.view;
  r.peak = ground;
  r.frames(1, ask(sh::Rjump::down));
  r.frames(120);
  const float rocket = r.peak - ground;
  CHECK_MSG(r.rockets() == rockets - 1,
            "rockets " + std::to_string(rockets) + " then " + std::to_string(r.rockets()));
  CHECK(cl::field_named(l, 1, "weapon") == 32);   // the launcher chosen for the shot
  CHECK(l.server->sv.edicts[1].health < health);  // a real rocket's blast
  CHECK_MSG(rocket > plain * 3, "a plain jump rose " + std::to_string(plain) +
                                    ", the rocket jump " + std::to_string(rocket));
  CHECK(l.view.x == view.x && l.view.y == view.y);  // his screen never turned
}

TEST(rrjump_fires_behind_and_throws_the_player_forward) {
  cl::Listen l;
  Run r{l};
  if (!ready_classic(l, r)) return;
  l.view = {60, 0, 0};  // looking 60 degrees down, facing east (+x)
  const float ground = r.z();
  const float rockets = r.rockets();
  r.peak = ground;
  r.frames(1, ask(sh::Rjump::back));
  float forward = 0;
  for (int i = 0; i < 40; i++) {
    r.frames(1);
    forward = std::max(forward, r.velocity().x);
  }
  r.frames(80);
  CHECK(r.rockets() == rockets - 1);
  CHECK_MSG(forward > 150, "the push forward was " + std::to_string(forward) + " units/s");
  CHECK_MSG(r.peak - ground > 60, "rose " + std::to_string(r.peak - ground));
}

TEST(a_press_inside_the_refire_waits_for_the_launcher) {
  cl::Listen l;
  Run r{l};
  if (!ready_classic(l, r)) return;
  l.impulse = 7;
  r.frames(80);
  const float ground = r.z();
  const float rockets = r.rockets();
  l.view = {-30, 0, 0};  // a rocket fired up the hall
  sh::MoveCmd fire;
  fire.buttons = sh::kButtonAttack;
  r.frames(1, fire);
  l.view = {0, 0, 0};
  CHECK(r.rockets() == rockets - 1);
  r.peak = ground;
  r.frames(1, ask(sh::Rjump::down));  // at once: the launcher is reloading
  r.frames(5);
  CHECK_MSG(r.peak - ground < 1, "jumped before the shot: " + std::to_string(r.peak - ground));
  r.frames(120);
  CHECK(r.rockets() == rockets - 2);  // fired once the refire ended
  CHECK_MSG(r.peak - ground > 150, "rose " + std::to_string(r.peak - ground));
}

TEST(with_the_setting_off_the_player_is_told_and_nothing_fires_unless_he_is_granted) {
  cl::Listen l;
  Run r{l};
  if (!ready_classic(l, r)) return;
  l.rules->console("sv_rjump 0");
  const float rockets = r.rockets();
  const float ground = r.z();
  r.peak = ground;
  l.prints.clear();
  r.frames(1, ask(sh::Rjump::down));
  r.frames(100);
  CHECK(r.rockets() == rockets);
  CHECK(told(l, "The rocket jump helper is off on this server."));
  CHECK(r.peak - ground < 60);  // his own jump only
  // The reverse one has its own setting, still on.
  r.frames(1, ask(sh::Rjump::back));
  r.frames(100);
  CHECK(r.rockets() == rockets - 1);
  // Granted to him alone: the down one works again.
  l.rules->console("sv_rrjump 0");
  l.rules->user_info(0, "*rjump", "1");
  r.frames(1, ask(sh::Rjump::down));
  r.frames(100);
  CHECK(r.rockets() == rockets - 2);
}

TEST(without_the_launcher_the_player_is_told_why) {
  br::Fixture f = br::sample_fixture();
  f.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "128 192 40"}, {"angle", "0"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_bsp(f, br::Family::v29, &bytes, &error), error);
  cl::Listen l;
  if (!start_on(l, "bare", bytes)) return;
  Run r{l};
  r.frames(80);
  l.prints.clear();
  r.frames(1, ask(sh::Rjump::down));
  r.frames(5);
  CHECK(told(l, "You have no rocket launcher."));
}

TEST(the_buttons_send_the_request_with_a_jump_once_a_press) {
  ge::console::Console con;
  cl::ControlSettings c;
  cl::add_cvars(con, c);
  con.execute("+rrjump");
  cl::Moves m = cl::button_moves(c);
  CHECK(m.impulse == 156 && m.jump);
  m = cl::button_moves(c);  // still held: asked once
  CHECK(m.impulse == 0 && !m.jump);
  con.execute("-rrjump");
  con.execute("+rjump");
  m = cl::button_moves(c);
  CHECK(m.impulse == 164 && m.jump);
}

// The sequel's rules on a map of brushes: the same request, the launcher picked up and raised.
TEST(rjump_works_in_the_sequel_s_rules_too) {
  br::BrushWorld world;
  world.models.resize(1);
  const auto box = [&](float x0, float y0, float z0, float x1, float y1, float z1) {
    const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
    world.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/wall"));
  };
  box(-512, -512, -16, 512, 512, 0);
  box(-528, -512, -16, -512, 512, 1024);
  box(512, -512, -16, 528, 512, 1024);
  box(-528, 512, -16, 528, 528, 1024);
  box(-528, -528, -16, 528, -512, 1024);
  box(-528, -528, 1024, 528, 528, 1040);
  world.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "0 0 24"}, {"angle", "90"}}));
  world.entities.push_back(entity({{"classname", "weapon_rocketlauncher"}, {"origin", "0 0 16"}}));
  std::vector<uint8_t> bsp;
  std::string error;
  CHECK_MSG(br::write_brush_world(world, &bsp, &error), error);
  cl::Listen l;
  if (!start_on(l, "fixture", bsp)) return;
  CHECK(shooters_edge::server::sequel_of(*l.rules) != nullptr);
  Run r{l};
  r.frames(200);  // spawned, the launcher picked up and raised
  const float ground = r.z();
  sh::MoveCmd jump;
  jump.buttons = sh::kButtonJump;
  r.peak = ground;
  r.frames(1, jump);
  r.frames(120);
  const float plain = r.peak - ground;
  CHECK_MSG(plain > 20, "the jump key does not jump in the sequel: rose " + std::to_string(plain));
  r.peak = ground;
  r.frames(1, ask(sh::Rjump::down));
  r.frames(200);
  const float rocket = r.peak - ground;
  CHECK_MSG(rocket > plain * 2, "a plain jump rose " + std::to_string(plain) +
                                    ", the rocket jump " + std::to_string(rocket));
}
