// Quake's weapons in Quake II's rules (classic remix's `cr_items q1`, M18u13d): with `sv_wear q1`
// the rules hold Quake's eight weapons whole, never Quake II's dressed as Quake's. A spawn has the
// axe, the shotgun and 25 shells; the shotgun fires each 0.5 s (eleven shots in 5.5 s, never
// Quake II's five), the nailgun each 0.1 s as spikes flying at 1000; a rocket from 8 forward
// kills a player 200 ahead in Quake's words; the items give Quake's weapons and amounts; the
// view model plays Quake's fire run from frame 1 and stands on frame 0.
//
// ge:summary Tests Quake's weapons fired whole by Quake II's rules under classic remix
// ge:keywords test remix cr_items sv_wear q1-arms quake-weapons shotgun-period spikes rocket
// ge:skill shooter testing
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classic/q1_arms.hpp"
#include "shooters_edge/server/q2_combat.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_quake_arms.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace sv = shooters_edge::server;
namespace br = ge::brush;
namespace sh = ge::shooter;

namespace {

// A wide floor, one start facing +x.
std::shared_ptr<const br::Map> hall() {
  br::BrushWorld w;
  w.models.resize(1);
  const float lo[3] = {-1024, -1024, -16}, hi[3] = {1024, 1024, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  br::Entity start;
  start.pairs = {{"classname", "info_player_deathmatch"}, {"origin", "0 0 24"}, {"angle", "0"}};
  w.entities.push_back(start);
  std::vector<uint8_t> bytes;
  std::string error;
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::write_brush_world(w, &bytes, &error) && br::read_bsp(bytes, map.get(), &error),
            error);
  return map;
}

struct Stage {
  sv::RulesRunner r;
  uint32_t sequence = 0;
  sv::Q2Server& s() const { return *sv::q2_of(r); }
  sv::Q2Player& p(int i) const { return s().players[i]; }
};

void tick(Stage& d, uint8_t buttons, float pitch = 0, int impulse = 0) {
  sh::Packet packets[2];
  ++d.sequence;
  for (int c = 0; c < 2; c++) {
    packets[c].client = c;
    packets[c].sequence = d.sequence;
    sh::MoveCmd cmd;
    cmd.msec = 13;
    if (c == 0) {
      cmd.buttons = buttons;
      cmd.angles = {pitch, 0, 0};
      cmd.impulse = static_cast<uint8_t>(impulse);
    }
    packets[c].cmds[0] = packets[c].cmds[1] = packets[c].cmds[2] = cmd;
  }
  d.r.frame(packets, 2);
}

void put(sv::Q2Player& p, float x, float z) {
  p.move.origin[0] = static_cast<int16_t>(x * 8);
  p.move.origin[1] = 0;
  p.move.origin[2] = static_cast<int16_t>(z * 8);
  for (int32_t& v : p.move.velocity) v = 0;
  for (int16_t& v : p.move.delta_angles) v = 0;
  p.forced = false;
}

std::unique_ptr<Stage> stage() {
  auto d = std::make_unique<Stage>();
  d->r = sv::q2_runner(sv::make_q2_server({}));
  d->r.console("deathmatch 1");
  d->r.console("set sv_wear q1");
  sv::MapSpec spec;
  spec.name = "hall";
  spec.brushes = hall();
  std::string error;
  CHECK_MSG(d->r.start(spec, error), error);
  d->r.connect(0, "Ana", false, false);
  tick(*d, 0);
  put(d->p(0), -600, 24);
  d->r.connect(1, "Ben", false, false);
  for (int i = 0; i < 160; i++) {
    put(d->p(0), -600, 24);
    put(d->p(1), 600, 24);
    tick(*d, 0);
  }
  return d;
}

bool listed(const std::vector<std::string>& names, std::string_view name) {
  for (const std::string& n : names)
    if (n == name) return true;
  return false;
}

int ammo(const sv::Q2Player& p, sv::SqAmmo a) { return p.body.ammo[static_cast<int>(a)]; }

// Holds attack for `seconds` of the server's time with Ana kept still; the ammunition spent.
int hold(Stage& d, sv::SqAmmo a, double seconds, float pitch = -60) {
  const int had = ammo(d.p(0), a);
  const double began = d.s().level_time;
  while (d.s().level_time - began < seconds - 1e-6) {
    put(d.p(0), -600, 24);
    put(d.p(1), 600, 24);
    tick(d, sh::kButtonAttack, pitch);
  }
  for (int i = 0; i < 40; i++) {  // let go: the hand comes to rest
    put(d.p(0), -600, 24);
    put(d.p(1), 600, 24);
    tick(d, 0, pitch);
  }
  return had - ammo(d.p(0), a);
}

}  // namespace

// The spawn's kit is Quake's: the axe and the shotgun, 25 shells, the shotgun in hand; Quake's view
// models and missiles are registered and Quake II's weapons are not.
TEST(a_spawn_holds_quakes_axe_and_shotgun) {
  auto d = stage();
  const sv::Q2Player& p = d->p(0);
  CHECK(d->s().quake_arms && p.body.quake);
  CHECK(p.hand.weapon == int(sv::SqWeapon::q1_shotgun));
  CHECK(ammo(p, sv::SqAmmo::shells) == 25);
  CHECK(sv::sq_holds(p.body, int(sv::SqWeapon::q1_axe)));
  CHECK(!sv::sq_holds(p.body, int(sv::SqWeapon::blaster)));
  CHECK(!sv::sq_holds(p.body, int(sv::SqWeapon::q1_rocket_launcher)));
  const sv::RulesWorld& w = d->s().world;
  for (const char* name :
       {"progs/v_shot.mdl", "progs/v_axe.mdl", "progs/spike.mdl", "progs/missile.mdl"})
    CHECK_MSG(listed(w.models, name), name);
  CHECK(!listed(w.models, "models/weapons/v_blast/tris.md2"));
  CHECK(listed(w.sounds, "weapons/guncock.wav") && !listed(w.sounds, "weapons/blastf1a.wav"));
}

// Quake's shotgun fires each 0.5 s: eleven shells in 5.5 s held, where Quake II's fires five. The
// view model shows Quake's fire run from frame 1, then stands on frame 0.
TEST(quakes_shotgun_fires_each_half_second) {
  auto d = stage();
  const int spent = hold(*d, sv::SqAmmo::shells, 5.5);
  CHECK_MSG(spent == 11, "shells " + std::to_string(spent));
  // One more shot: frame 1 on the frame it fires, 0 once its run is over.
  put(d->p(0), -600, 24);
  tick(*d, sh::kButtonAttack, -60);
  while (d->s().players[0].q1_last_shot != d->s().frame_number) tick(*d, sh::kButtonAttack, -60);
  CHECK_MSG(sv::quake_view_frame(d->s(), d->p(0)) == 1,
            std::to_string(sv::quake_view_frame(d->s(), d->p(0))));
  for (int i = 0; i < 80; i++) tick(*d, 0, -60);
  CHECK(sv::quake_view_frame(d->s(), d->p(0)) == 0);
}

// The nailgun (the machinegun's item): a nail each 0.1 s while held, each a spike flying at 1000.
TEST(quakes_nailgun_fires_spikes_each_tenth) {
  auto d = stage();
  sv::Q2Player& p = d->p(0);
  CHECK(sv::sq_pick_up(sv::sq_item("weapon_machinegun"), p.body, true, 0, 0));
  CHECK(ammo(p, sv::SqAmmo::bullets) == 30);  // Quake's nailgun comes with 30 nails
  p.body.ammo[int(sv::SqAmmo::bullets)] = 100;
  for (int i = 0; i < 60; i++) tick(*d, 0, -60, 4);  // impulse 4: Quake's nailgun
  CHECK(p.hand.weapon == int(sv::SqWeapon::q1_nailgun));
  // Held: the first spikes in flight are Quake's, at Quake's speed.
  bool spiked = false;
  for (int i = 0; i < 20 && !spiked; i++) {
    put(p, -600, 24);
    tick(*d, sh::kButtonAttack, -60);
    for (const sv::Q2Missile& m : d->s().missiles)
      if (m.live && m.kind == sv::SqShot::q1_spike) {
        spiked = true;
        CHECK(sh::length(m.velocity) > 999 && sh::length(m.velocity) < 1001);
        CHECK(m.damage == 9);
      }
  }
  CHECK(spiked);
  const int spent = hold(*d, sv::SqAmmo::bullets, 2.0);
  CHECK_MSG(spent >= 19 && spent <= 21, "nails " + std::to_string(spent));
}

// A rocket at Ben 200 ahead: Quake's rocket (100 to 120 on him, its blast) kills him, and the
// obituary is in Quake's words.
TEST(quakes_rocket_kills_in_quakes_words) {
  auto d = stage();
  sv::Q2Player& p = d->p(0);
  CHECK(sv::sq_pick_up(sv::sq_item("weapon_rocketlauncher"), p.body, true, 0, 0));
  for (int i = 0; i < 60; i++) tick(*d, 0, 0, 7);
  CHECK(p.hand.weapon == int(sv::SqWeapon::q1_rocket_launcher));
  for (int i = 0; i < 20; i++) {
    put(p, -200, 24);
    put(d->p(1), 0, 24);
    tick(*d, 0);
  }
  tick(*d, sh::kButtonAttack);
  bool flew = false;
  for (const sv::Q2Missile& m : d->s().missiles)
    flew = flew || (m.live && m.kind == sv::SqShot::q1_rocket && sh::length(m.velocity) > 999 &&
                    sh::length(m.velocity) < 1001);
  CHECK(flew);
  for (int i = 0; i < 80 && d->p(1).alive; i++) {
    put(d->p(1), 0, 24);
    tick(*d, 0);
  }
  CHECK(!d->p(1).alive);
  CHECK(sv::sq_obituary("Ben", "Ana", false, int(sv::SqWeapon::q1_rocket_launcher), 0, "male") ==
        "Ben rides Ana's rocket");
}

// The items give Quake's weapon of their role and Quake's amounts.
TEST(quake_ii_items_give_quakes_weapons_and_amounts) {
  sv::SqBody b = sv::sq_new_body();
  sv::sq_quake_body(b);
  CHECK(sv::sq_pick_up(sv::sq_item("weapon_shotgun"), b, true, 0, 0));
  CHECK(sv::sq_holds(b, int(sv::SqWeapon::q1_super_shotgun)));
  CHECK(b.ammo[int(sv::SqAmmo::shells)] == 30);  // 25 and the gun's 5
  CHECK(sv::sq_pick_up(sv::sq_item("weapon_bfg"), b, true, 0, 0));
  CHECK(sv::sq_holds(b, int(sv::SqWeapon::q1_lightning)));
  CHECK(b.ammo[int(sv::SqAmmo::cells)] == 15);
  CHECK(sv::sq_pick_up(sv::sq_item("ammo_grenades"), b, true, 0, 0));
  CHECK(b.ammo[int(sv::SqAmmo::rockets)] == 5);
  CHECK(sv::sq_pick_up(sv::sq_item("ammo_slugs"), b, true, 0, 0));
  CHECK(b.ammo[int(sv::SqAmmo::cells)] == 21);
  CHECK(sv::sq_best_weapon(b) == int(sv::SqWeapon::q1_lightning));
  // Quake II's own rules never hold Quake's weapons.
  const sv::SqBody own = sv::sq_new_body();
  CHECK(!sv::sq_holds(own, int(sv::SqWeapon::q1_shotgun)));
  CHECK(sv::sq_best_weapon(own) == int(sv::SqWeapon::blaster));
}

// The view model's frames: a single shot's run once, a held weapon's round.
TEST(quakes_view_frames_run_from_one_and_rest_on_zero) {
  namespace gc = ge::classic;
  CHECK(gc::q1_view_frame(gc::Q1ArmId::shotgun, 0) == 1);
  CHECK(gc::q1_view_frame(gc::Q1ArmId::shotgun, 5) == 6);
  CHECK(gc::q1_view_frame(gc::Q1ArmId::shotgun, 6) == 0);
  CHECK(gc::q1_view_frame(gc::Q1ArmId::nailgun, 8) == 1);
  CHECK(gc::q1_view_frame(gc::Q1ArmId::lightning, 5) == 2);
  CHECK(std::string(gc::q1_look_name("q1_spike")) == "spike");
  CHECK(gc::q1_look_name("te_spike") == nullptr);
}
