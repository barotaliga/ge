// THE QUICK WEAPON IN SHOOTER'S EDGE: the cvars set from the console and their lists read; the
// HUD's status read as the plugin's weapons (the double guns ready only with two rounds); and the
// listen server on the fixture halls, the player on his shotgun with a launcher carried: a tap of
// +attack under `cl_quickweapon 1` fires ONE rocket and puts the shotgun back in hand, +attack2
// fires the shotgun's list, and the hand shows the launcher's see-through ghost at rest.
//
// ge:summary Tests: Shooter's Edge's quick weapon: cvars, the status read, a tap on the listen
// ge:summary server
// ge:keywords test quick-weapon quick_fire cl_quickweapon attack2 ghost listen-server
// ge:skill shooter testing
#include <memory>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "check.hpp"
#include "console/console.hpp"
#include "shooters_edge/client/controls.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/quick_fire.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "vfs/pak.hpp"

namespace cl = shooters_edge::client;
namespace hud = shooters_edge::hud;
namespace br = ge::brush;
namespace sh = ge::shooter;

namespace {

constexpr double kTic = 0.013;

// The fixture halls, a deathmatch spot facing east with a rocket launcher under it.
std::vector<uint8_t> halls_pack() {
  br::Fixture f = br::sample_fixture();
  br::Entity spot, launcher;
  spot.pairs = {{"classname", "info_player_deathmatch"}, {"origin", "128 192 40"}, {"angle", "0"}};
  launcher.pairs = {{"classname", "weapon_rocketlauncher"}, {"origin", "128 192 0"}};
  f.entities.push_back(spot);
  f.entities.push_back(launcher);
  std::vector<uint8_t> bsp, out;
  std::string error;
  CHECK_MSG(br::write_bsp(f, br::Family::v29, &bsp, &error), error);
  CHECK_MSG(ge::vfs::write_pak({{"maps/halls.bsp", bsp}}, &out, &error), error);
  return out;
}

bool start(cl::Listen& l) {
  static ge::progs::Progs progs;
  std::string error;
  if (progs.statements.empty()) CHECK_MSG(cl::baseline_progs(progs, error), error);
  cl::SearchPath files;
  CHECK(cl::add_archive(files, "pak0.pak", halls_pack(), error));
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, "halls", error);
  CHECK_MSG(map != nullptr, error);
  if (!map) return false;
  const bool ok =
      cl::listen_start(l, progs, std::move(map), shooters_edge::net::listen_options(), error);
  CHECK_MSG(ok, error);
  return ok;
}

// One frame of the window's: the buttons made into a command through the quick weapon.
void frame(cl::Listen& l, cl::QuickFire& q, double& t, bool attack, bool attack2 = false) {
  cl::Moves m;
  m.attack = attack;
  m.attack2 = attack2;
  m.impulse = l.impulse;
  l.impulse = 0;
  cl::quick_moves(q, cl::status_of(l, t), t, m);
  sh::MoveCmd cmd;
  cmd.msec = 13;
  cmd.angles = {l.view.x, l.view.y, 0};
  if (m.attack) cmd.buttons |= sh::kButtonAttack;
  l.impulse = m.impulse;
  cl::listen_frame(l, cmd);
  t += kTic;
}

}  // namespace

TEST(the_cvars_set_the_lists_and_switches) {
  ge::console::Console con;
  cl::ControlSettings c;
  cl::add_cvars(con, c);
  const sh::QuickSettings& d = cl::quick_settings(c.quick);
  CHECK(!d.on[0] && d.on[1]);  // +attack plain until asked; +attack2 always quick
  CHECK(d.prio[0] == (std::vector<int>{7, 8, 5, 6, 3, 2, 4, 1}));
  CHECK(d.prio[1] == (std::vector<int>{8, 3, 2}));
  CHECK(d.fallback == (std::vector<int>{2, 1}));
  CHECK(d.preview && !d.hide);
  con.execute(
      "cl_quickweapon 1; cl_quickweapon_prio \"6 7\"; cl_quickweapon_default 1; "
      "cl_quickweapon_preview 0; cl_quickweapon_hide 1");
  const sh::QuickSettings& s = cl::quick_settings(c.quick);
  CHECK(s.on[0] && s.prio[0] == (std::vector<int>{6, 7}) && s.fallback == std::vector<int>{1});
  CHECK(!s.preview && s.hide);
  CHECK(con.cvars().find("cl_quickweapon_prio2") != nullptr);
}

TEST(the_status_reads_as_weapons_ready_with_a_shots_ammunition) {
  hud::Status s;
  s.owned = 0x7F;  // every gun
  s.ammo = {1, 1, 0, 5};
  s.weapon = hud::Weapon::shotgun;
  const sh::QuickArmed a = cl::quick_armed(s);
  CHECK((a.ready >> 1 & 1u) != 0);  // the melee weapon, always
  CHECK((a.ready >> 2 & 1u) != 0);  // the shotgun: one shell
  CHECK((a.ready >> 3 & 1u) == 0);  // the double shotgun needs two
  CHECK((a.ready >> 5 & 1u) == 0);  // the heavy nailgun needs two nails
  CHECK((a.ready >> 7 & 1u) == 0);  // no rockets
  CHECK((a.ready >> 8 & 1u) != 0);
  CHECK(a.in_hand == 2 && a.ammo[2] == 1 && a.ammo[1] == -1 && a.alive);
  s.health = 0;
  CHECK(!cl::quick_armed(s).alive);
}

TEST(a_tap_fires_one_rocket_and_puts_the_shotgun_back_on_the_listen_server) {
  cl::Listen l;
  if (!start(l)) return;
  cl::QuickFire q;
  q.on = true;
  double t = 0;
  for (int i = 0; i < 80; i++) frame(l, q, t, false);  // the spawn, the launcher picked up
  l.impulse = 2;
  for (int i = 0; i < 80; i++) frame(l, q, t, false);  // the shotgun in hand
  hud::Status s = cl::status_of(l, t);
  CHECK(s.weapon == hud::Weapon::shotgun && s.has(hud::Weapon::rocket_launcher));
  const int rockets = s.ammo[2], shells = s.ammo[0];
  // At rest: the hand shows the launcher +attack would fire, see-through.
  shooters_edge::view::HandFrame hand;
  cl::quick_hand(q, s, t, hand);
  CHECK(hand.weapon == shooters_edge::view::Weapon::rocket_launcher && hand.opacity < 1);
  frame(l, q, t, true);  // the tap: one frame
  for (int i = 0; i < 200; i++) frame(l, q, t, false);
  s = cl::status_of(l, t);
  CHECK(s.ammo[2] == rockets - 1);  // one rocket
  CHECK(s.ammo[0] == shells);       // no shell spent by mistake
  CHECK(s.weapon == hud::Weapon::shotgun);
  // +attack2: the shotgun's list (no beam gun, no double shotgun carried): the shotgun in hand.
  frame(l, q, t, false, true);
  for (int i = 0; i < 100; i++) frame(l, q, t, false);
  s = cl::status_of(l, t);
  CHECK(s.ammo[0] == shells - 1);
  CHECK(s.weapon == hud::Weapon::shotgun);
}
