// MIDAIR ON THE SEQUEL'S RULES (`k_midair 1`, server/sequel_fight.cpp over plugins/shooter/
// midair.hpp): a spawn's kit, the items gone from the map, a rocket on a target below the least
// height wearing armour only, one above it killing and scoring by the rise with the line everyone
// reads, rockets never used up, the dead back by themselves after 2 s, and the level's tally.
//
// ge:summary Tests: midair on the sequel's runner: kit, items, counted hits, frags, respawn
// ge:keywords test midair k_midair sequel fight_midair_kit counted-hit rise respawn tally
// ge:skill shooter testing
#include <memory>
#include <string>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "core/libm/libm.hpp"
#include "shooter/midair.hpp"
#include "shooters_edge/server/sequel_fight.hpp"
#include "shooters_edge/server/sequel_server.hpp"

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;

namespace {

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

std::shared_ptr<const br::Map> hall() {
  br::BrushWorld w;
  w.models.resize(1);
  const float lo[3] = {-1024, -512, -16}, hi[3] = {1024, 512, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  w.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "0 0 24"}, {"angle", "0"}}));
  w.entities.push_back(entity({{"classname", "item_health_large"}, {"origin", "300 200 16"}}));
  w.entities.push_back(entity({{"classname", "weapon_railgun"}, {"origin", "-300 200 16"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  auto map = std::make_shared<br::Map>();
  CHECK(br::write_brush_world(w, &bytes, &error) && br::read_bsp(bytes, map.get(), &error));
  return map;
}

struct Stage {
  sv::RulesRunner r;
  uint32_t sequence = 0;
  sv::SequelServer& s() const { return *sv::sequel_of(r); }
  sv::SequelPlayer& p(int i) const { return s().players[i]; }
};

void tick(Stage& d, uint8_t buttons, float pitch) {
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

void put(sv::SequelPlayer& p, float x, float z) {
  p.move.origin[0] = static_cast<int16_t>(x * 8);
  p.move.origin[1] = 0;
  p.move.origin[2] = static_cast<int16_t>(z * 8);
  for (int16_t& v : p.move.velocity) v = 0;
  for (int16_t& v : p.move.delta_angles) v = 0;
  p.forced = false;
}

std::unique_ptr<Stage> stage(int level = 1) {
  auto d = std::make_unique<Stage>();
  d->r = sv::sequel_runner(sv::make_sequel_server({}));
  d->r.console("deathmatch 1");
  d->r.console("k_midair 1");
  d->r.console("k_midair_minheight " + std::to_string(level));
  sv::MapSpec spec;
  spec.name = "hall";
  spec.brushes = hall();
  std::string error;
  CHECK_MSG(d->r.start(spec, error), error);
  d->r.connect(0, "Ana", false, false);
  d->r.connect(1, "Ben", false, false);
  for (int i = 0; i < 160; i++) tick(*d, 0, 0);
  return d;
}

// One rocket from the floor (origin z 24) at Ben held `height` above the floor 200 ahead; ticks
// on until the blow lands or a second passes.
void fire_at(Stage& d, float height) {
  const float pitch =
      -static_cast<float>(ge::core::libm::atan2(height + 4 - 22.0, 200.0) * 180.0 / 3.14159265);
  for (int i = 0; i < 80; i++) {
    put(d.p(0), 0, 24);
    put(d.p(1), 200, 24 + height);
    tick(d, 0, pitch);
  }
  tick(d, sh::kButtonAttack, pitch);
  for (int i = 0; i < 80 && d.p(1).alive; i++) {
    put(d.p(1), 200, 24 + height);
    tick(d, 0, pitch);
  }
}

}  // namespace

TEST(a_midair_spawn_holds_the_launcher_and_the_map_holds_nothing) {
  auto d = stage();
  const sv::SequelPlayer& p = d->p(1);
  CHECK(p.health == sh::kMidairHealth);
  CHECK(p.body.armour == sv::SqArmour::body && p.body.armour_count == 200);
  CHECK(p.hand.weapon == int(sv::SqWeapon::rocket_launcher));
  CHECK(p.body.ammo[static_cast<int>(sv::SqAmmo::rockets)] == 255);
  for (const sv::SequelEntity& e : d->s().entities)
    CHECK_MSG(e.item < 0 || e.gone, e.classname + " lies on the map");
}

TEST(a_rocket_below_the_least_height_wears_armour_and_spares_health) {
  auto d = stage();
  fire_at(*d, 60);
  CHECK(d->p(1).alive);
  CHECK(d->p(1).health == sh::kMidairHealth);
  CHECK(d->p(1).body.armour_count < 200);
  CHECK(d->p(0).frags == 0);
  // Rockets never run out.
  CHECK(d->p(0).body.ammo[static_cast<int>(sv::SqAmmo::rockets)] == 255);
}

TEST(a_rocket_high_enough_kills_and_scores_by_the_rise) {
  auto d = stage();
  fire_at(*d, 600);
  CHECK(!d->p(1).alive);
  CHECK(d->p(0).frags == 4);  // gold
  CHECK(!d->s().obituaries.empty());
  CHECK_MSG(d->s().obituaries.back().rfind("Gold midair by Ana on Ben (", 0) == 0,
            d->s().obituaries.back());
  CHECK(d->p(0).midair.hits[static_cast<int>(sh::MidairBand::gold)] == 1);
  // Back by himself after 2 s, armed again.
  for (int i = 0; i < 200 && !d->p(1).alive; i++) tick(*d, 0, 0);
  CHECK(d->p(1).alive);
  CHECK(d->p(1).health == sh::kMidairHealth);
  const std::vector<std::string> tally = sv::fight_midair_report(d->s());
  CHECK(tally.size() == 1);
  CHECK_MSG(tally[0].rfind("Ana: 1 midair (bronze 0, silver 0, gold 1, platinum 0)", 0) == 0,
            tally[0]);
}

TEST(the_least_height_follows_its_level) {
  auto d = stage(3);  // 512: gold
  fire_at(*d, 300);
  CHECK(d->p(1).alive && d->p(0).frags == 0);
}
