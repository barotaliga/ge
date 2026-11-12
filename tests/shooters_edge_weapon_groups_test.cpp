// WEAPON GROUPS IN SHOOTER'S EDGE: the cvars and the game's own groups (empty: the profile's, "0":
// none); the HUD's status read as the plugin's weapons; the look laid on the status; the HUD's
// group column (open, folding, the second weapon's mark); and the listen server on the fixture
// halls with a launcher picked up: under cl_weapongroups 1 key 1 takes the launcher, key 4 the
// shotgun back, key 7 (no group) passes as the launcher's own key.
//
// ge:summary Tests: Shooter's Edge's weapon groups: cvars, the status read, the HUD column, keys
// ge:summary on the listen server
// ge:keywords test weapon-groups cl_weapongroups cl_weapongroup group_moves group_status wgroup
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
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/weapon_groups.hpp"
#include "shooters_edge/hud/layout.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
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

// One frame of the window's: the number key made into a command through the weapon groups.
void frame(cl::Listen& l, cl::WeaponGroups& g, double& t, int key = 0, bool attack = false) {
  cl::Moves m;
  m.attack = attack;
  m.impulse = key;
  cl::group_moves(g, cl::status_of(l, t), t, m);
  sh::MoveCmd cmd;
  cmd.msec = 13;
  cmd.angles = {l.view.x, l.view.y, 0};
  if (m.attack) cmd.buttons |= sh::kButtonAttack;
  l.impulse = m.impulse;
  cl::listen_frame(l, cmd);
  t += kTic;
}

const hud::GroupShow& group_column(hud::Hud& h, hud::Status& s, cl::WeaponGroups& g, double t) {
  cl::group_status(g, s, t);
  static const hud::HudArt art = hud::free_art();
  hud::fill_hud(h, s, art);
  return s.group;
}

}  // namespace

TEST(the_cvars_take_the_games_groups_until_set) {
  ge::console::Console con;
  cl::ControlSettings c;
  cl::add_cvars(con, c);
  CHECK(!c.groups.on);
  c.groups.game = shooters_edge::profiles::classic_game().weapon_groups;
  const sh::WeaponGroups& d = cl::group_settings(c.groups);
  CHECK(d[0] == (std::vector<int>{7, 6}));
  CHECK(d[1] == std::vector<int>{8});
  CHECK(d[2] == (std::vector<int>{5, 4}));
  CHECK(d[3] == (std::vector<int>{3, 2}));
  CHECK(d[4] == std::vector<int>{1});
  CHECK(d[5].empty());
  con.execute("cl_weapongroups 1; cl_weapongroup1 \"6 7\"; cl_weapongroup2 0; cl_weapongroup6 8");
  const sh::WeaponGroups& s = cl::group_settings(c.groups);
  CHECK(c.groups.on);
  CHECK(s[0] == (std::vector<int>{6, 7}));
  CHECK(s[1].empty());  // "0": key 2 picks its own weapon
  CHECK(s[5] == std::vector<int>{8});
  // The sequel's own groups are its weapon keys.
  const auto* sequel = shooters_edge::profiles::find_game("sequel");
  CHECK(sequel != nullptr && sequel->weapon_groups.size() == 6);
}

TEST(the_status_reads_as_carried_and_ready_weapons) {
  hud::Status s;
  s.owned = 0x31;  // the shotgun and the two launchers
  s.ammo = {5, 0, 0, 0};
  s.weapon = hud::Weapon::shotgun;
  const sh::GroupArmed a = cl::group_armed(s);
  CHECK((a.carried >> 7 & 1u) != 0 && (a.carried >> 6 & 1u) != 0);
  CHECK((a.ready >> 7 & 1u) == 0);  // no rockets
  CHECK((a.carried >> 1 & 1u) != 0 && (a.ready >> 2 & 1u) != 0);
  CHECK(a.in_hand == 2);
}

TEST(the_hud_column_opens_folds_and_marks_the_second_weapon) {
  cl::WeaponGroups g;
  g.on = true;
  g.game = shooters_edge::profiles::classic_game().weapon_groups;
  hud::Hud h = hud::make_hud();
  hud::Status s;
  s.owned = 0x31;  // the shotgun and both launchers (bit k - 1 for gun k)
  s.ammo = {20, 0, 10, 0};
  s.weapon = hud::Weapon::shotgun;
  // Folded: the shotgun alone, no mark (no double shotgun carried).
  const hud::GroupShow* c = &group_column(h, s, g, 0);
  CHECK(c->on && c->count == 1 && !c->more && c->open == 0);
  // Key 1: the launchers, the rocket launcher asked for and lit.
  cl::Moves m;
  m.impulse = 1;
  cl::group_moves(g, s, 1.0, m);
  CHECK(m.impulse == 7);
  s.weapon = hud::Weapon::rocket_launcher;
  c = &group_column(h, s, g, 1.1);
  CHECK(c->count == 2 && c->lit == 0 && c->open == 1);
  CHECK(c->weapons[1] == hud::Weapon::grenade_launcher);
  // Again: the grenade launcher.
  m = {};
  m.impulse = 1;
  cl::group_moves(g, s, 1.3, m);
  CHECK(m.impulse == 6);
  // The attack folds it into the weapon asked for, marked: the rocket launcher is carried too.
  m = {};
  m.attack = true;
  cl::group_moves(g, s, 1.5, m);
  s.weapon = hud::Weapon::grenade_launcher;
  c = &group_column(h, s, g, 1.5 + sh::kCloseS + 0.01);
  CHECK(c->count == 1 && c->more && c->weapons[0] == hud::Weapon::grenade_launcher);
  // Off: no column.
  g.on = false;
  CHECK(!group_column(h, s, g, 2).on);
}

TEST(number_keys_pick_from_their_groups_on_the_listen_server) {
  cl::Listen l;
  if (!start(l)) return;
  cl::WeaponGroups g;
  g.on = true;
  g.game = shooters_edge::profiles::classic_game().weapon_groups;
  double t = 0;
  for (int i = 0; i < 80; i++) frame(l, g, t);  // the spawn, the launcher picked up
  frame(l, g, t, 4);                            // the shotguns: the shotgun
  for (int i = 0; i < 80; i++) frame(l, g, t);
  hud::Status s = cl::status_of(l, t);
  CHECK(s.weapon == hud::Weapon::shotgun && s.has(hud::Weapon::rocket_launcher));
  frame(l, g, t, 1);  // the launchers: the rocket launcher
  for (int i = 0; i < 80; i++) frame(l, g, t);
  CHECK(cl::status_of(l, t).weapon == hud::Weapon::rocket_launcher);
  frame(l, g, t, 4);
  for (int i = 0; i < 80; i++) frame(l, g, t);
  CHECK(cl::status_of(l, t).weapon == hud::Weapon::shotgun);
  frame(l, g, t, 7);  // no group: the launcher's own key
  for (int i = 0; i < 80; i++) frame(l, g, t);
  CHECK(cl::status_of(l, t).weapon == hud::Weapon::rocket_launcher);
}
