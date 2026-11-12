// The sequel's fight lab: our own clean-room rules (server/sequel_combat.hpp, sequel_fight.cpp)
// measured on the runner over a hall built here (no game files). Each weapon is held on a still
// player 200 units away for ten seconds: the shots, the ammo, the damage dealt and its rate, and a
// chart of the damage over time; then a rocket fired at the floor (the rocket jump): the
// shooter's height and health through the second after. Every number is deterministic.
//
// ge:summary Lab: the sequel's weapons on the runner: damage over time, periods, the rocket jump
// ge:keywords lab sequel fight weapons damage-rate period rocket-jump se-sequel-fight
// ge:skill shooter labs
// ge:test tests/shooters_edge_sequel_fight_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "shooter/sequel_move.hpp"
#include "shooters_edge/server/sequel_server.hpp"

namespace {

namespace br = ge::brush;
namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;

constexpr float kHealth = 100000;  // the target's: never killed, so every blow counts

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

std::shared_ptr<const br::Map> hall(std::string& error) {
  br::BrushWorld w;
  w.models.resize(1);
  const float floor_lo[3] = {-1024, -256, -16}, floor_hi[3] = {1024, 256, 0};
  w.models[0].push_back(br::box_brush(floor_lo, floor_hi, sh::kSqSolid, "world/floor"));
  w.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "-300 0 24"}, {"angle", "0"}}));
  std::vector<uint8_t> bytes;
  auto map = std::make_shared<br::Map>();
  if (!br::write_brush_world(w, &bytes, &error) || !br::read_bsp(bytes, map.get(), &error))
    return nullptr;
  return map;
}

struct Duel {
  sv::RulesRunner r;
  uint32_t sequence = 0;
  sv::SequelPlayer& p(int i) const { return sv::sequel_of(r)->players[i]; }
};

void tick(Duel& d, uint8_t buttons, float pitch) {
  sh::Packet packets[2];
  for (int c = 0; c < 2; c++) {
    packets[c].client = c;
    packets[c].sequence = ++d.sequence;
    sh::MoveCmd cmd;
    cmd.msec = 13;
    if (c == 0) {
      cmd.buttons = buttons;
      cmd.angles = {pitch, 0, 0};
    }
    packets[c].cmds[0] = packets[c].cmds[1] = packets[c].cmds[2] = cmd;
  }
  d.r.frame(packets, 2);
}

void place(Duel& d, int i, float x) {
  d.p(i).move.origin[0] = static_cast<int16_t>(x * 8);
  d.p(i).move.origin[1] = 0;
  for (int16_t& v : d.p(i).move.velocity) v = 0;
}

std::unique_ptr<Duel> duel(const std::shared_ptr<const br::Map>& map, std::string& error) {
  auto d = std::make_unique<Duel>();
  d->r = sv::sequel_runner(sv::make_sequel_server({}));
  d->r.console("deathmatch 1");
  sv::MapSpec spec;
  spec.name = "hall";
  spec.brushes = map;
  if (!d->r.start(spec, error)) return nullptr;
  d->r.connect(0, "Ana", false, false);
  d->r.connect(1, "Ben", false, false);
  for (int i = 0; i < 80; i++) tick(*d, 0, 0);
  place(*d, 0, -100);
  place(*d, 1, 100);
  for (int i = 0; i < 20; i++) tick(*d, 0, 0);
  return d;
}

std::string fixed(double v, int places) { return ge::core::num::to_fixed(v, places); }

void run(gl::Lab& lab) {
  std::string error;
  const std::shared_ptr<const br::Map> map = hall(error);
  if (map == nullptr) {
    lab.fail("the hall did not build: " + error, "Read the reason above. Skill: brush.");
    return;
  }
  const double seconds = lab.arg_num("seconds");
  const int ticks = static_cast<int>(seconds / 0.013);
  gl::LabTable table{"weapons held 200 units from a still player",
                     {"weapon", "shots", "ammo used", "damage", "damage/s", "first hit (s)"},
                     {}};
  // Two charts, so each line has its own colour: the weapons that fire a shot a period, and those
  // that keep firing while attack is held.
  gl::LineChart single, held;
  single.spikes = held.spikes = false;
  single.frame = {"DAMAGE DEALT, A SHOT A PERIOD, 200 UNITS AWAY",
                  "deathmatch's numbers; our own rules (server/sequel_combat.hpp)", "t (s)",
                  "damage dealt"};
  held.frame = single.frame;
  held.frame.title = "DAMAGE DEALT, FIRING WHILE HELD, 200 UNITS AWAY";
  for (int w = 0; w < sv::kSqWeaponCount; w++) {
    std::unique_ptr<Duel> d = duel(map, error);
    if (d == nullptr) {
      lab.fail(error, "Read the reason above. Skill: shooter.");
      return;
    }
    const sv::SqWeaponRow& row = sv::kSqWeapons[w];
    sv::SqBody& body = d->p(0).body;
    if (row.item != nullptr) body.counts[sv::sq_item(row.item)] = 1;
    if (row.ammo != sv::SqAmmo::none) body.ammo[static_cast<int>(row.ammo)] = 1000;
    if (w != 0) d->r.client_command(0, std::string("use ") + row.name);
    for (int i = 0; i < 120; i++) tick(*d, 0, 0);  // raised
    d->p(1).health = kHealth;
    const int had = row.ammo == sv::SqAmmo::none ? 0 : body.ammo[static_cast<int>(row.ammo)];
    gl::Trace trace{row.name, {}, {}};
    double first = -1;
    for (int i = 0; i < ticks; i++) {
      tick(*d, sh::kButtonAttack, 0);
      const double dealt = kHealth - d->p(1).health;
      if (first < 0 && dealt > 0) first = (i + 1) * 0.013;
      if (i % 8 == 0) {
        trace.x.push_back((i + 1) * 0.013);
        trace.y.push_back(dealt);
      }
    }
    const double dealt = kHealth - d->p(1).health;
    const int used = row.ammo == sv::SqAmmo::none ? 0 : had - body.ammo[static_cast<int>(row.ammo)];
    const int shots = row.per_shot == 0 ? 0 : used / row.per_shot;
    const std::string key = std::string("weapon.") + std::to_string(w + 1);
    lab.metric(key + ".ammo-used", used, "count");
    lab.metric(key + ".damage", dealt, "points");
    lab.metric(key + ".first-hit", first, "s");
    table.rows.push_back({row.name, row.per_shot == 0 ? "-" : std::to_string(shots),
                          std::to_string(used), fixed(dealt, 0), fixed(dealt / seconds, 1),
                          first < 0 ? "none" : fixed(first, 2)});
    const bool keeps = w == int(sv::SqWeapon::machinegun) || w == int(sv::SqWeapon::chaingun) ||
                       w == int(sv::SqWeapon::hyperblaster);
    (keeps ? held : single).traces.push_back(std::move(trace));
  }
  lab.table(table);
  lab.shot({"damage-single",
            single.frame.title,
            "Each weapon held on a still player 200 units away, from the step that presses attack: "
            "the damage dealt so far.",
            "a step at each weapon's period; pellets lose some to their spread; the bolt and the "
            "missiles land a little after the shot; the BFG's lasers before its ball",
            {{"seconds", fixed(seconds, 0)}}},
           gl::to_picture(gl::line_chart(single).plot));
  lab.shot({"damage-held",
            held.frame.title,
            "The weapons that keep firing while attack is held, on a still player 200 units away: "
            "the damage dealt so far.",
            "the chaingun climbing faster as it winds up; the hyperblaster's bolts landing a beat "
            "after the shots",
            {{"seconds", fixed(seconds, 0)}}},
           gl::to_picture(gl::line_chart(held).plot));

  // The rocket jump: a rocket at the feet, the shooter's height and health after.
  std::unique_ptr<Duel> d = duel(map, error);
  if (d == nullptr) return;
  place(*d, 1, 900);
  const sv::SqWeaponRow& rl = sv::kSqWeapons[int(sv::SqWeapon::rocket_launcher)];
  d->p(0).body.counts[sv::sq_item(rl.item)] = 1;
  d->p(0).body.ammo[int(sv::SqAmmo::rockets)] = 5;
  d->r.client_command(0, "use Rocket Launcher");
  for (int i = 0; i < 120; i++) tick(*d, 0, 89);
  const float floor_z = float(d->p(0).move.origin[2]) / 8;
  gl::LineChart jump;
  jump.spikes = false;
  jump.frame = {"A ROCKET AT THE FEET: THE ROCKET JUMP",
                "the shooter's own blast pushes 3.2 times "
                "as hard and hurts half as much",
                "t (s)", "units"};
  gl::Trace height{"height above the floor", {}, {}}, health{"health", {}, {}};
  float top = 0;
  for (int i = 0; i < 100; i++) {
    tick(*d, i < 4 ? sh::kButtonAttack : 0, 89);
    const float z = float(d->p(0).move.origin[2]) / 8 - floor_z;
    top = std::max(top, z);
    height.x.push_back((i + 1) * 0.013);
    height.y.push_back(z);
    health.x.push_back((i + 1) * 0.013);
    health.y.push_back(d->p(0).health);
  }
  lab.metric("rocket-jump.height", top, "units");
  lab.metric("rocket-jump.health", d->p(0).health, "points");
  if (top < 40)
    lab.fail("the rocket jump rose only " + fixed(top, 1) + " units",
             "Read sq_knockback and blast in server/sequel_fight.cpp. Skill: shooter.");
  jump.traces = {height, health};
  lab.shot({"rocket-jump",
            jump.frame.title,
            "A rocket fired straight down: the shooter's height above the floor and his health "
            "through the next 1.3 s.",
            "up well over a step's height, and health down by about half the blast",
            {{"top", fixed(top, 0)}}},
           gl::to_picture(gl::line_chart(jump).plot));
}

const gl::LabSpec kSpec{
    "se-sequel-fight",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "the sequel's weapons and damage on its runner (server/sequel_combat.hpp, sequel_fight.cpp)",
    "shooter",
    "Does each of the sequel's weapons fire at its period and deal its damage on the runner, and "
    "does a rocket at the feet lift its shooter?",
    {{"seconds", "10", "seconds each weapon is held"}},
    "--seconds=3",
    run,
};
GE_LAB(kSpec);

}  // namespace
