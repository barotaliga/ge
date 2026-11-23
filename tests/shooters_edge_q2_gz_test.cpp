// Tests: Ground Zero's rules (games/shooters_edge/server/q2_gz.hpp): the chainfist, the ETF rifle,
// the plasma beam, the disruptor, the prox mine, the tesla and the A-M bomb on a hall, double
// damage's multiplier, the obituaries' words and the ammo a dry body turns to.
//
// ge:summary Tests: Ground Zero's weapons, mines and bomb, double damage, obituaries
// ge:keywords test q2 ground-zero rogue chainfist etf-rifle plasma-beam disruptor prox-mine tesla
// ge:keywords am-bomb double-damage obituary
// ge:skill shooter testing
// ge:test tests/shooters_edge_q2_gz_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_fight_kit.hpp"
#include "shooters_edge/server/q2_gz.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
using sv::SqWeapon;

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// A long hall with a floor at 0 and walls at x ±1024.
std::shared_ptr<const br::Map> hall() {
  br::BrushWorld w;
  w.models.resize(1);
  const float floor_lo[3] = {-1024, -256, -16}, floor_hi[3] = {1024, 256, 0};
  const float east_lo[3] = {1024, -256, 0}, east_hi[3] = {1040, 256, 256};
  const float west_lo[3] = {-1040, -256, 0}, west_hi[3] = {-1024, 256, 256};
  w.models[0].push_back(br::box_brush(floor_lo, floor_hi, sh::kSqSolid, "world/floor"));
  w.models[0].push_back(br::box_brush(east_lo, east_hi, sh::kSqSolid, "world/wall"));
  w.models[0].push_back(br::box_brush(west_lo, west_hi, sh::kSqSolid, "world/wall"));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-900 0 24"}, {"angle", "0"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

struct Arena {
  sv::RulesRunner r;
  uint32_t sequence = 0;
  sv::Q2Server& s() const { return *sv::q2_of(r); }
  sv::Q2Player& p(int i) const { return s().players[i]; }
};

// Host ticks of 13 ms: Ana with `buttons` and `pitch` (degrees down), Ben standing still.
void run(Arena& a, int ticks, uint8_t buttons = 0, float pitch = 0) {
  for (int i = 0; i < ticks; i++) {
    sh::Packet packets[2];
    for (int c = 0; c < 2; c++) {
      packets[c].client = c;
      packets[c].sequence = ++a.sequence;
      sh::MoveCmd cmd;
      cmd.msec = 13;
      if (c == 0) {
        cmd.buttons = buttons;
        cmd.angles = {pitch, 0, 0};
      }
      packets[c].cmds[0] = packets[c].cmds[1] = packets[c].cmds[2] = cmd;
    }
    a.r.frame(packets, 2);
  }
}

void place(Arena& a, int i, float x) {
  int32_t* o = a.p(i).move.origin;
  o[0] = static_cast<int32_t>(x * 8);
  o[1] = 0;
  for (int32_t& v : a.p(i).move.velocity) v = 0;
}

// Ground Zero's rules on the hall: Ana at x −300 looking down it at Ben at x `ben`.
std::unique_ptr<Arena> facing(float ben, const char* deathmatch = "1") {
  auto a = std::make_unique<Arena>();
  sv::Q2ServerOptions o;
  o.pack = sv::Q2Pack::ground_zero;
  a->r = sv::q2_runner(sv::make_q2_server(o));
  a->r.console(std::string("deathmatch ") + deathmatch);
  a->r.console("sv_fps 10");
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall();
  std::string error;
  CHECK_MSG(a->r.start(map, error), error);
  a->r.connect(0, "Ana", false, false);
  a->p(0).move.origin[0] = static_cast<int32_t>(a->p(0).move.origin[0] - 64 * 8);
  a->r.connect(1, "Ben", false, false);
  run(*a, 80);
  place(*a, 0, -300);
  place(*a, 1, ben);
  run(*a, 20);
  return a;
}

void give(Arena& a, SqWeapon w, int ammo) {
  const sv::SqWeaponRow& r = sv::kSqWeapons[int(w)];
  a.p(0).body.counts[sv::sq_item(r.item, sv::Q2Pack::ground_zero)] = 1;
  if (r.ammo != sv::SqAmmo::none) a.p(0).body.ammo[int(r.ammo)] = ammo;
  CHECK(a.r.client_command(0, std::string("use ") + r.name) == sv::CommandResult::mod);
  run(a, 100);  // the blaster dropped, the new one raised
  CHECK_MSG(a.p(0).hand.weapon == int(w) && a.p(0).hand.state == sv::SqHandState::ready, r.name);
}

int events_named(const Arena& a, const char* name) {
  int n = 0;
  for (const sv::NetEvent& e : a.s().out.events) n += e.text == name ? 1 : 0;
  return n;
}

TEST(the_plasma_beam_burns_15_a_frame_and_draws_its_beam) {
  auto a = facing(200);
  give(*a, SqWeapon::plasma_beam, 100);
  const float before = a->p(1).health;
  bool beam = false;
  for (int i = 0; i < 8; i++) {  // a tenth: one frame's 15
    run(*a, 1, 1);
    beam |= events_named(*a, "te_heatbeam") > 0;
  }
  CHECK(beam);
  CHECK(a->p(1).health < before);
  CHECK(int(before - a->p(1).health) % 15 == 0);
  CHECK(a->p(0).body.ammo[int(sv::SqAmmo::cells)] < 100);
}

TEST(the_etf_rifle_s_flechettes_pass_armour) {
  auto a = facing(0);
  a->p(1).body.armour = sv::SqArmour::body;
  a->p(1).body.armour_count = 100;
  give(*a, SqWeapon::etf_rifle, 50);
  const float before = a->p(1).health;
  run(*a, 60, 1);
  CHECK(a->p(1).health < before || !a->p(1).alive);
  CHECK(a->p(1).body.armour_count == 100);  // flechettes skip the armour
  CHECK(a->p(0).body.ammo[int(sv::SqAmmo::flechettes)] < 50);
}

TEST(the_chainfist_hits_within_64_and_pulls_its_wielder) {
  auto a = facing(-250);  // 50 apart
  give(*a, SqWeapon::chainfist, 0);
  const float before = a->p(1).health;
  run(*a, 120, 1);  // a swing or two
  CHECK(a->p(1).health < before);
  auto far = facing(0);
  give(*far, SqWeapon::chainfist, 0);
  const float full = far->p(1).health;
  run(*far, 120, 1);
  CHECK(far->p(1).health == full);  // 300 away: nothing
}

TEST(the_disruptor_follows_and_hurts_over_half_a_second) {
  auto a = facing(300);
  give(*a, SqWeapon::disruptor, 15);
  const float before = a->p(1).health;
  run(*a, 120, 1);
  CHECK(a->p(1).health <= before - 30 || !a->p(1).alive);
  CHECK(a->p(0).body.ammo[int(sv::SqAmmo::rounds)] < 15);
}

TEST(a_prox_mine_sticks_arms_and_goes_off_when_a_body_comes_near) {
  auto a = facing(900);
  give(*a, SqWeapon::prox_launcher, 5);
  run(*a, 2, 1);  // one mine, down the hall onto the floor
  run(*a, 200);   // landed, opened, armed
  const sv::Q2Missile* mine = nullptr;
  for (const sv::Q2Missile& m : a->s().missiles)
    if (m.live && m.kind == sv::SqShot::prox) mine = &m;
  CHECK(mine != nullptr);
  if (mine == nullptr) return;
  CHECK(mine->stage == 2);  // stuck and armed
  const float before = a->p(1).health;
  place(*a, 1, mine->origin.x + 60);  // into its field: 0.5 s later it goes off
  run(*a, 80);
  bool live = false;
  for (const sv::Q2Missile& m : a->s().missiles) live |= m.live && m.kind == sv::SqShot::prox;
  CHECK(!live);
  CHECK(a->p(1).health < before || !a->p(1).alive);
}

TEST(double_damage_doubles_and_stacks_with_the_quad_in_deathmatch) {
  auto a = facing(200);
  sv::Q2Player& p = a->p(0);
  const double now = a->s().level_time;
  CHECK(sv::fight_multiplier(a->s(), p) == 1);
  p.double_until = now + 30;
  CHECK(sv::fight_multiplier(a->s(), p) == 2);
  p.quad_until = now + 30;
  CHECK(sv::fight_multiplier(a->s(), p) == 8);
  a->r.console("dmflags 262144");  // no stacking
  CHECK(sv::fight_multiplier(a->s(), p) == 4);
  CHECK(std::string(sv::fight_multiplier_sound(2)) == "misc/ddamage3.wav");
  CHECK(std::string(sv::fight_multiplier_sound(8)) == "items/damage3.wav");
  CHECK(sv::fight_multiplier_sound(1) == nullptr);
}

TEST(the_am_bomb_kills_within_its_radius) {
  auto a = facing(200);
  sv::Q2Player& p = a->p(0);
  const int bomb = sv::sq_item("ammo_nuke", sv::Q2Pack::ground_zero);
  p.body.counts[bomb] = 1;
  CHECK(sv::fight_use(a->s(), 0, bomb));
  run(*a, 800);  // 10 s on (and some)
  CHECK(!a->p(1).alive);
  CHECK(a->p(1).frags == 0);
}

TEST(ground_zero_s_obituaries) {
  const auto line = [](SqWeapon w) {
    return sv::sq_obituary("Ben", "Ana", false, int(w), sv::kSqHowDirect, "male");
  };
  CHECK(line(SqWeapon::chainfist) == "Ben was shredded by Ana's ripsaw");
  CHECK(line(SqWeapon::etf_rifle) == "Ben was perforated by Ana");
  CHECK(line(SqWeapon::plasma_beam) == "Ben was scorched by Ana's plasma beam");
  CHECK(line(SqWeapon::tesla) == "Ben was enlightened by Ana's tesla mine");
  CHECK(line(SqWeapon::prox_launcher) == "Ben got too close to Ana's proximity mine");
  CHECK(line(SqWeapon::disruptor) == "Ben was annihilated by Ana's disruptor");
  CHECK(sv::sq_obituary("Ben", "Ana", false, sv::kSqByAmBomb, sv::kSqHowBlast, "male") ==
        "Ben was nuked by Ana's antimatter bomb");
}

TEST(a_dry_body_turns_to_ground_zero_s_order) {
  sv::SqBody b = sv::sq_new_body(sv::Q2Pack::ground_zero);
  b.counts[sv::sq_item("weapon_hyperblaster")] = 1;
  b.ammo[int(sv::SqAmmo::cells)] = 50;
  CHECK(sv::sq_best_weapon(b) == int(SqWeapon::blaster));  // the hyperblaster is never chosen
  b.counts[sv::sq_item("weapon_etf_rifle", sv::Q2Pack::ground_zero)] = 1;
  b.ammo[int(sv::SqAmmo::flechettes)] = 10;
  CHECK(sv::sq_best_weapon(b) == int(SqWeapon::etf_rifle));
  b.counts[sv::sq_item("weapon_plasmabeam", sv::Q2Pack::ground_zero)] = 1;
  CHECK(sv::sq_best_weapon(b) == int(SqWeapon::plasma_beam));
  b.ammo[int(sv::SqAmmo::cells)] = 1;  // not enough for a frame
  CHECK(sv::sq_best_weapon(b) == int(SqWeapon::etf_rifle));
}

}  // namespace
