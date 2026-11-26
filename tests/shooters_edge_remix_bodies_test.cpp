// Armour and health remixed by each game's own rules (classic remix's `cr_items`, M18u13e): the
// things a remix lays on a map are taken by the rules of the game they belong to, never the played
// game's numbers under another's models. Quake II's armour adds onto the one worn up to its most
// and gives its own counts; Quake's replaces the worn one when better. Quake II's boxes give its
// own points on Quake's rules, Quake's give Quake's on Quake II's rules.
//
// ge:summary Tests armour and health taken by their own game's rules under classic remix
// ge:keywords test remix cr_items armour health q1-body q2-body take-rule salvage real-items
// ge:skill shooter testing
#include <bit>
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classic/q1_body.hpp"
#include "classic/q2_body.hpp"
#include "progs/layout.hpp"
#include "progs/vm.hpp"
#include "shooter/hull.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/remix.hpp"
#include "shooters_edge/net/qw_game.hpp"
#include "shooters_edge/profiles/arsenal.hpp"
#include "shooters_edge/profiles/remix_arsenal.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_quake_arms.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "vfs/pak.hpp"

namespace cl = shooters_edge::client;
namespace pf = shooters_edge::profiles;
namespace sv = shooters_edge::server;
namespace gc = ge::classic;
namespace br = ge::brush;
namespace sh = ge::shooter;

namespace {

// A floor walled round (a Quake II map, as the remix reads it), the start with the jacket, the
// combat armour, a medium health and a box of shells on it.
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
  world.entities.push_back(entity({{"classname", "item_armor_jacket"}, {"origin", "0 0 16"}}));
  world.entities.push_back(entity({{"classname", "item_armor_combat"}, {"origin", "0 0 16"}}));
  world.entities.push_back(entity({{"classname", "item_health"}, {"origin", "0 0 16"}}));
  world.entities.push_back(entity({{"classname", "ammo_shells"}, {"origin", "0 0 16"}}));
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

const ge::progs::Progs& baseline() {
  static const ge::progs::Progs progs = [] {
    ge::progs::Progs out;
    std::string why;
    CHECK_MSG(cl::baseline_progs(out, why), why);
    return out;
  }();
  return progs;
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
  cl::ListenOptions opt = shooters_edge::net::listen_options();
  opt.arsenal = pf::arsenal_lines(worn->arsenal);
  CHECK_MSG(cl::listen_start(l, baseline(), std::move(map), opt, error), error);
  return l.server != nullptr && sv::qc_of(*l.rules) != nullptr;
}

// Entity `e`'s field `name` (the program's own, by name; 1 the player, 0 the world).
float& field(cl::Listen& l, const char* name, int e = 1) {
  sv::QcServer* qc = sv::qc_of(*l.rules);
  const ge::progs::Def* d = ge::progs::find_field(baseline(), name);
  CHECK_MSG(d != nullptr, name);
  return *reinterpret_cast<float*>(
      ge::progs::entity_field(qc->q.vm, e, d != nullptr ? d->offset : 0));
}

}  // namespace

// Quake II's take rule, both ways round: a better armour takes the worn one's count over at the
// shares' ratio; a worse one adds at that ratio up to the worn one's most; a full one is left.
TEST(q2_armour_adds_onto_the_worn_one_by_its_share) {
  CHECK(gc::q2_take_armour(0, 0, 1).count == 25);
  const gc::Q2Armour body = gc::q2_take_armour(1, 40, 3);  // 100 + 40 x 0.3 / 0.8
  CHECK(body.taken && body.tier == 3 && body.count == 115);
  const gc::Q2Armour jacket = gc::q2_take_armour(3, 100, 1);  // 100 + 25 x 0.3 / 0.8
  CHECK(jacket.taken && jacket.tier == 3 && jacket.count == 109);
  CHECK(!gc::q2_take_armour(1, 50, 1).taken);  // the jacket at its most
  const gc::Q2Armour shard = gc::q2_take_armour(0, 0, 0);
  CHECK(shard.taken && shard.tier == 1 && shard.count == 2);
  CHECK(gc::q2_armour_tier(0.6f) == 2 && gc::q2_armour_tier(0) == 0);
}

// Quake's rule: the better armour (share times value) replaces the worn one, the worse is left.
TEST(q1_armour_replaces_the_worn_one_when_better) {
  CHECK(gc::q1_takes_armour(0, 0, 1));
  CHECK(gc::q1_takes_armour(1, 100, 2));
  CHECK(!gc::q1_takes_armour(3, 200, 2));
  CHECK(gc::q1_takes_armour(3, 20, 1));  // a red worn down to 20 is worth less than a green
}

// Quake II's rules wearing Quake's things: the jacket is Quake's green (100), the combat Quake's
// yellow (150, replacing it), a jacket then left lying; the boxes give Quake's 15, 25 and 100 up
// to 250, and every blow is shared at the normal share (Quake's armour knows no energy).
TEST(q2_rules_take_quakes_armour_and_health_by_quakes_rules) {
  sv::SqBody b = sv::sq_new_body();
  sv::sq_quake_body(b);
  CHECK(sv::sq_pick_up(sv::sq_item("item_armor_jacket"), b, true, 0, 0));
  CHECK(b.armour == sv::SqArmour::jacket && b.armour_count == 100);
  CHECK(sv::sq_pick_up(sv::sq_item("item_armor_combat"), b, true, 0, 0));
  CHECK(b.armour == sv::SqArmour::combat && b.armour_count == 150);
  CHECK(!sv::sq_pick_up(sv::sq_item("item_armor_jacket"), b, true, 0, 0));
  CHECK(sv::sq_armour_save(b, 10, true) == 6);  // 60% of an energy blow too
  b.health = 50;
  CHECK(sv::sq_pick_up(sv::sq_item("item_health_small"), b, true, 0, 0));
  CHECK(b.health == 65);
  CHECK(sv::sq_pick_up(sv::sq_item("item_health_large"), b, true, 0, 0));
  CHECK(b.health == 90);
  b.health = 200;
  CHECK(sv::sq_pick_up(sv::sq_item("item_health_mega"), b, true, 0, 0));
  CHECK(b.health == gc::kQ1MegaLimit);
  // Quake II's own: the stimpack 2 past the limit, its armour added.
  sv::SqBody own = sv::sq_new_body();
  CHECK(sv::sq_pick_up(sv::sq_item("item_armor_jacket"), own, true, 0, 0));
  CHECK(own.armour_count == 25);
  CHECK(sv::sq_pick_up(sv::sq_item("item_health_small"), own, true, 0, 0));
  CHECK(own.health == 102);
}

// The classic rules wearing Quake II's things: the rows carry Quake II's counts, most and points;
// on the server the jacket and the combat armour at the start join as Quake II's do (62 combat,
// whichever is touched first), the box of shells holds Quake II's 10, and the medium box (Quake's
// small one) gives Quake II's 10.
TEST(classic_rules_take_q2s_armour_and_health_by_q2s_rules) {
  const pf::RemixArsenal* worn = pf::remix_arsenal("q1", "q2");
  CHECK(worn != nullptr);
  if (worn == nullptr) return;
  CHECK(worn->arsenal.armour[0].amount == 25 && worn->arsenal.armour[0].most == 50);
  CHECK(worn->arsenal.armour[2].amount == 100 && worn->arsenal.armour[2].most == 200);
  const std::vector<std::string> lines = pf::arsenal_lines(worn->arsenal);
  bool medium = false;
  for (const std::string& line : lines)
    medium = medium || line ==
                           "set arsenal_health1 \"models/items/healing/medium/tris.md2 "
                           "items/n_health.wav 15 10\"";
  CHECK(medium);

  cl::Listen l;
  if (!start(l)) return;
  for (int i = 0; i < 120; i++) cl::listen_frame(l, {});  // spawned on the armour: taken
  const float shells = field(l, "ammo_shells");           // the spawn's 25 and Quake II's box of 10
  CHECK_MSG(shells == 35, std::to_string(shells));
  CHECK_MSG(field(l, "armorvalue") == 62, std::to_string(field(l, "armorvalue")));
  CHECK(field(l, "armortype") > 0.59f && field(l, "armortype") < 0.61f);
  l.server->sv.edicts[1].health = 50;  // the engine's own copy, which the program reads
  for (int i = 0; i < 40; i++) cl::listen_frame(l, {});
  CHECK_MSG(l.server->sv.edicts[1].health == 60, std::to_string(l.server->sv.edicts[1].health));
}

// Quake II's carry limits on the classic rules wearing its weapons: each weapon row ends with its
// ammunition's limit (shells 100, bullets 200, rockets 50, slugs 50), kept in the world's
// ammunition fields, and a player is filled no further than that.
TEST(classic_rules_carry_q2s_ammunition_to_q2s_limits) {
  const pf::RemixArsenal* worn = pf::remix_arsenal("q1", "q2");
  CHECK(worn != nullptr);
  if (worn == nullptr) return;
  CHECK(worn->arsenal.weapons[6].ammo_most == gc::kQ2MostRockets);
  CHECK(worn->arsenal.weapons[7].ammo_most == gc::kQ2MostSlugs);
  CHECK(pf::weapon_text(worn->arsenal.weapons[6]).ends_with(" 5 50"));
  CHECK(pf::weapon_text(pf::kQ1Arsenal.weapons[6]).ends_with(" 5"));  // Quake's own: no limit word

  cl::Listen l;
  if (!start(l)) return;
  for (int i = 0; i < 10; i++) cl::listen_frame(l, {});
  CHECK_MSG(field(l, "ammo_rockets", 0) == 50, std::to_string(field(l, "ammo_rockets", 0)));
  CHECK_MSG(field(l, "ammo_cells", 0) == 50, std::to_string(field(l, "ammo_cells", 0)));
  CHECK_MSG(field(l, "ammo_shells", 0) == 100, std::to_string(field(l, "ammo_shells", 0)));
  CHECK_MSG(field(l, "ammo_nails", 0) == 200, std::to_string(field(l, "ammo_nails", 0)));
}
