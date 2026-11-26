// Quake II's weapons in the classic rules (classic remix's `cr_items q2`, M18u13b): a slot whose
// row fires `q2_*` is Quake II's weapon whole. The rows name Quake II's weapons in Quake's slots;
// the program's strings hold every sample, effect and obituary word they need; and on the server
// the shotgun fires once in 1.2 s (never at Quake's 0.5 s), its frames are Quake II's, its pump
// and its puffs are Quake II's, and the rail fires once in 1.6 s.
//
// ge:summary Tests Quake II's weapons fired whole by the classic rules under classic remix
// ge:keywords test remix cr_items q2-arms real-weapons shotgun-period railgun q2-arm-strings
// ge:skill shooter testing
#include <bit>
#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "brush/write.hpp"
#include "check.hpp"
#include "classic/q2_arms.hpp"
#include "progs/layout.hpp"
#include "progs/vm.hpp"
#include "shooter/hull.hpp"
#include "shooter/pmove.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/q2_arms_kick.hpp"
#include "shooters_edge/client/remix.hpp"
#include "shooters_edge/net/qw_game.hpp"
#include "shooters_edge/profiles/arsenal.hpp"
#include "shooters_edge/profiles/remix_arsenal.hpp"
#include "shooters_edge/rules/q2_arm_strings.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "vfs/pak.hpp"

namespace cl = shooters_edge::client;
namespace pf = shooters_edge::profiles;
namespace gc = ge::classic;
namespace br = ge::brush;
namespace sh = ge::shooter;

namespace {

// A floor walled round, the start facing +y, Quake II's rail lying at the start.
std::vector<uint8_t> hall() {
  br::BrushWorld world;
  world.models.resize(1);
  const auto box = [&](float x0, float y0, float z0, float x1, float y1, float z1) {
    const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
    world.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/wall"));
  };
  box(-512, -512, -16, 512, 512, 0);
  box(-528, -512, -16, -512, 512, 256);
  box(512, -512, -16, 528, 512, 256);
  box(-528, 512, -16, 528, 528, 256);
  box(-528, -528, -16, 528, -512, 256);
  const auto entity = [](std::initializer_list<std::pair<std::string, std::string>> pairs) {
    br::Entity e;
    for (const auto& p : pairs) e.pairs.push_back(p);
    return e;
  };
  world.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "0 0 24"}, {"angle", "90"}}));
  world.entities.push_back(entity({{"classname", "weapon_railgun"}, {"origin", "0 0 16"}}));
  std::vector<uint8_t> bsp;
  std::string error;
  CHECK_MSG(br::write_brush_world(world, &bsp, &error), error);
  return bsp;
}

std::vector<uint8_t> pack(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files) {
  std::vector<uint8_t> out;
  std::string error;
  CHECK_MSG(ge::vfs::write_pak(files, &out, &error), error);
  return out;
}

bool listed(const std::vector<std::string>& names, std::string_view name) {
  for (const std::string& n : names)
    if (n == name) return true;
  return false;
}

// The classic rules on the hall wearing Quake II's things, started into `l`.
bool start(cl::Listen& l) {
  cl::RemixState state;
  state.on = true;
  state.other = "q2";
  const pf::RemixArsenal* worn = pf::remix_arsenal("q1", "q2");
  CHECK(worn != nullptr);
  if (worn == nullptr) return false;
  cl::SearchPath mine;
  cl::RemixFiles remix;
  remix.played = "q1";
  remix.state = state;
  std::string error;
  CHECK(cl::add_archive(remix.files, "pak0.pak", pack({{"maps/hall.bsp", hall()}}), error));
  std::unique_ptr<cl::LoadedMap> map = cl::load_remixed_map(mine, "hall", &remix, error);
  CHECK_MSG(map != nullptr, error);
  if (!map) return false;
  static const ge::progs::Progs progs = [] {
    ge::progs::Progs out;
    std::string why;
    CHECK_MSG(cl::baseline_progs(out, why), why);
    return out;
  }();
  cl::ListenOptions opt = shooters_edge::net::listen_options();
  opt.arsenal = pf::arsenal_lines(worn->arsenal);
  CHECK_MSG(cl::listen_start(l, progs, std::move(map), opt, error), error);
  return l.server != nullptr && shooters_edge::server::qc_of(*l.rules) != nullptr;
}

float field(cl::Listen& l, int f) {
  shooters_edge::server::QcServer* qc = shooters_edge::server::qc_of(*l.rules);
  return std::bit_cast<float>(*ge::progs::entity_field(qc->q.vm, 1, f));
}

// Holds attack with impulse `pick` for `seconds`: the shots counted by the ammunition `ammo`
// (a field) going down, each shot's frames seen.
struct Fired {
  int shots = 0;
  float first = -1, last = -1;  // the times of the first and last shot, s
  int frame_least = 1 << 20, frame_most = -1;
};
Fired hold_attack(cl::Listen& l, int pick, int ammo, float per_shot, float seconds) {
  sh::MoveCmd pick_cmd;
  pick_cmd.angles = {0, 90, 0};
  l.impulse = pick;
  for (int i = 0; i < 120; i++) cl::listen_frame(l, pick_cmd);  // taken up and raised
  shooters_edge::server::QcServer* qc = shooters_edge::server::qc_of(*l.rules);
  sh::MoveCmd fire;
  fire.angles = {0, 90, 0};
  fire.buttons = sh::kButtonAttack;
  Fired out;
  float had = field(l, ammo);
  const double began = l.server->sv.time;
  while (l.server->sv.time - began < seconds) {
    cl::listen_frame(l, fire);
    const float now = field(l, ammo);
    const float t = static_cast<float>(l.server->sv.time - began);
    if (now < had) {
      out.shots += static_cast<int>(std::lround((had - now) / per_shot));
      if (out.first < 0) out.first = t;
      out.last = t;
    }
    had = now;
    const int frame = static_cast<int>(field(l, qc->world_fields.weaponframe));
    out.frame_least = frame < out.frame_least ? frame : out.frame_least;
    out.frame_most = frame > out.frame_most ? frame : out.frame_most;
  }
  return out;
}

}  // namespace

// Each of Quake's slots holds Quake II's weapon whole: its fire word Quake II's, its cost Quake
// II's, its period its frames' (the shotgun's 1.2 s, the rail's 1.6 s).
TEST(the_classic_slots_hold_q2_weapons_whole) {
  const pf::RemixArsenal* worn = pf::remix_arsenal("q1", "q2");
  CHECK(worn != nullptr);
  if (worn == nullptr) return;
  CHECK(worn->arsenal.weapons.size() == std::size(pf::kQ1Weapons));
  for (const pf::WeaponRow& w : worn->arsenal.weapons) CHECK(pf::fire_is_q2(w.fire));
  for (int a = 0; a < 10; a++)
    CHECK(pf::kFireWords[static_cast<int>(pf::kFireQ2First) + a] == gc::kQ2ArmFireWords[a]);
  const pf::WeaponRow& shotgun = worn->arsenal.weapons[1];
  CHECK(shotgun.fire == pf::Fire::q2_shotgun && shotgun.cost == 1);
  CHECK_MSG(shotgun.refire > 1.19f && shotgun.refire < 1.21f, std::to_string(shotgun.refire));
  const pf::WeaponRow& rail = worn->arsenal.weapons[7];
  CHECK(rail.fire == pf::Fire::q2_railgun && rail.ammo == pf::Ammo::cells);
  CHECK_MSG(rail.refire > 1.59f && rail.refire < 1.61f, std::to_string(rail.refire));
}

// The rules' strings hold every sample, effect and obituary word, once each.
TEST(the_rules_strings_hold_q2_weapons_names_once) {
  namespace ru = shooters_edge::server::rules;
  CHECK(ru::kQ2ArmStringCount > 20 && ru::kQ2ArmStringCount <= ru::q2s::kMost);
  for (const char* s : {"weapons/shotgf1b.wav", "weapons/shotgr1b.wav", "weapons/railgf1a.wav",
                        "weapons/machgf3b.wav", "weapons/noammo.wav"})
    CHECK_MSG(ru::q2_arm_string(s) >= 0, s);
  CHECK(ru::q2_arm_string(gc::kQ2RailLine) >= 0 && ru::q2_arm_string(gc::kQ2RocketBlast) >= 0);
  CHECK(ru::q2_arm_string("progs/player.mdl") == -1);
  for (int i = 0; i < ru::kQ2ArmStringCount; i++)
    for (int k = i + 1; k < ru::kQ2ArmStringCount; k++)
      CHECK(std::string_view(ru::kQ2ArmStrings[static_cast<size_t>(i)]) !=
            ru::kQ2ArmStrings[static_cast<size_t>(k)]);
}

// On the server, the shotgun is Quake II's: a shot each 1.2 s (five in 5.5 s held, never Quake's
// eleven), the view model through Quake II's fire frames, its pump among the sounds loaded.
TEST(q2_shotgun_fires_at_its_own_period_on_the_classic_rules) {
  cl::Listen l;
  if (!start(l)) return;
  for (int i = 0; i < 120; i++) cl::listen_frame(l, {});  // spawned
  shooters_edge::server::QcServer* qc = shooters_edge::server::qc_of(*l.rules);
  const Fired f = hold_attack(l, 2, qc->world_fields.ammo[0], 1, 5.5f);
  CHECK_MSG(f.shots == 5, "shots " + std::to_string(f.shots) + " from " + std::to_string(f.first) +
                              " to " + std::to_string(f.last));
  if (f.shots >= 2) {
    const float period = (f.last - f.first) / static_cast<float>(f.shots - 1);
    CHECK_MSG(period > 1.15f && period < 1.25f, "period " + std::to_string(period));
  }
  const gc::Q2ArmFrames& runs = gc::q2_arm(gc::Q2ArmId::shotgun).frames;
  CHECK_MSG(f.frame_most > runs.fire_from && f.frame_most <= runs.last_idle,
            "frames to " + std::to_string(f.frame_most));
  CHECK(listed(l.server->sounds, "weapons/shotgf1b.wav"));
  CHECK(listed(l.server->sounds, "weapons/shotgr1b.wav"));  // the pump after the shot
  CHECK(listed(qc->effect_names, gc::kQ2HitPellet));        // its pellets' puffs by Quake II's name
}

// The rail lying at the start is taken as the beam slot, and fires as Quake II's: once in 1.6 s.
TEST(q2_railgun_fires_at_its_own_period_on_the_classic_rules) {
  cl::Listen l;
  if (!start(l)) return;
  for (int i = 0; i < 120; i++) cl::listen_frame(l, {});  // spawned on the rail: taken
  shooters_edge::server::QcServer* qc = shooters_edge::server::qc_of(*l.rules);
  const Fired f = hold_attack(l, 8, qc->world_fields.ammo[3], 1, 5);
  CHECK_MSG(f.shots == 3 || f.shots == 4, "shots " + std::to_string(f.shots));
  if (f.shots >= 2) {
    const float period = (f.last - f.first) / static_cast<float>(f.shots - 1);
    CHECK_MSG(period > 1.55f && period < 1.65f, "period " + std::to_string(period));
  }
  CHECK(listed(qc->effect_names, gc::kQ2RailLine));
}


// The window's kick for Quake II's weapons on the classic rules (client/q2_arms_kick.hpp): the
// rocket launcher's shot read off its frames tips the view 1 degree up and the eye 2 units back,
// in over a tenth and out over the next; Quake's own view model kicks nothing.
TEST(q2_weapons_on_the_classic_rules_kick_the_view_as_quake_ii) {
  cl::ArmsKick k;
  const float ahead[3] = {0, 1, 0};
  constexpr std::string_view rocket = "models/weapons/v_rocket/tris.md2";
  cl::arms_kick_step(k, rocket, 13, 0.9, ahead, true, 1);  // idle
  cl::arms_kick_step(k, rocket, 6, 1.0, ahead, true, 1);   // fired
  const gc::Q2Kick top = cl::arms_kick_step(k, rocket, 7, 1.1, ahead, true, 1);
  CHECK(top.angles[0] > -1.01f && top.angles[0] < -0.99f);
  CHECK(top.origin[1] > -2.01f && top.origin[1] < -1.99f);
  const gc::Q2Kick gone = cl::arms_kick_step(k, rocket, 8, 1.2, ahead, true, 1);
  CHECK(gone.angles[0] == 0 && gone.origin[1] == 0);
  cl::ArmsKick quake;
  cl::arms_kick_step(quake, "progs/v_rock2.mdl", 0, 0.9, ahead, true, 1);
  const gc::Q2Kick none = cl::arms_kick_step(quake, "progs/v_rock2.mdl", 1, 1.0, ahead, true, 1);
  CHECK(none.angles[0] == 0);
}
