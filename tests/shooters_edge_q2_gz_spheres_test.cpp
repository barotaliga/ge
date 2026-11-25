// Tests: Ground Zero's spheres (games/shooters_edge/server/q2_gz_spheres.cpp): one out at a time
// and following its owner, the defender's half and its bolts, the vengeance's strike, the hunter's
// chase and the camera its owner's view rides, a doppleganger's sphere, and their ends.
//
// ge:summary Tests: Ground Zero's defender, hunter and vengeance spheres and the hunter's camera
// ge:keywords test q2 ground-zero rogue sphere defender-sphere hunter-sphere vengeance-sphere
// ge:keywords huntercam doppleganger obituary
// ge:skill shooter testing
// ge:test tests/shooters_edge_q2_gz_spheres_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_fight_kit.hpp"
#include "shooters_edge/server/q2_gz.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// A long hall floored at 0 and roofed at 256, walled at x ±1024.
std::shared_ptr<const br::Map> hall() {
  br::BrushWorld w;
  w.models.resize(1);
  const float floor_lo[3] = {-1024, -256, -16}, floor_hi[3] = {1024, 256, 0};
  const float roof_lo[3] = {-1024, -256, 256}, roof_hi[3] = {1024, 256, 272};
  const float east_lo[3] = {1024, -256, 0}, east_hi[3] = {1040, 256, 256};
  const float west_lo[3] = {-1040, -256, 0}, west_hi[3] = {-1024, 256, 256};
  w.models[0].push_back(br::box_brush(floor_lo, floor_hi, sh::kSqSolid, "world/floor"));
  w.models[0].push_back(br::box_brush(roof_lo, roof_hi, sh::kSqSolid, "world/roof"));
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

// Host ticks of 13 ms, both standing.
void run(Arena& a, int ticks) {
  for (int i = 0; i < ticks; i++) {
    sh::Packet packets[2];
    for (int c = 0; c < 2; c++) {
      packets[c].client = c;
      packets[c].sequence = ++a.sequence;
      sh::MoveCmd cmd;
      cmd.msec = 13;
      packets[c].cmds[0] = packets[c].cmds[1] = packets[c].cmds[2] = cmd;
    }
    a.r.frame(packets, 2);
  }
}

constexpr int kSecond = 77;  // host ticks of 13 ms in a second

void place(Arena& a, int i, float x) {
  int32_t* o = a.p(i).move.origin;
  o[0] = static_cast<int32_t>(x * 8);
  o[1] = 0;
  for (int32_t& v : a.p(i).move.velocity) v = 0;
}

sh::Vec3f at(const Arena& a, int i) {
  const int32_t* o = a.p(i).move.origin;
  return {float(o[0]) * 0.125f, float(o[1]) * 0.125f, float(o[2]) * 0.125f};
}

// Ground Zero's rules on the hall, in deathmatch: Ana at x −300, Ben at x `ben`.
std::unique_ptr<Arena> arena(float ben) {
  auto a = std::make_unique<Arena>();
  sv::Q2ServerOptions o;
  o.pack = sv::Q2Pack::ground_zero;
  a->r = sv::q2_runner(sv::make_q2_server(o));
  a->r.console("deathmatch 1");
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

int item(const char* classname) { return sv::sq_item(classname, sv::Q2Pack::ground_zero); }

// Ana uses sphere `classname`, given to her first.
void launch(Arena& a, const char* classname, const char* name) {
  a.p(0).body.counts[item(classname)] = 1;
  CHECK(a.r.client_command(0, std::string("use ") + name) == sv::CommandResult::mod);
  run(a, 2);
}

const sv::Q2Missile* sphere(const Arena& a, int owner = 0) {
  for (const sv::Q2Missile& m : a.s().missiles)
    if (m.live && sv::gz_sphere_kind(m.kind) && m.owner == owner) return &m;
  return nullptr;
}

bool said(const Arena& a, const std::string& line) {
  for (const std::string& o : a.s().obituaries)
    if (o == line) return true;
  return false;
}

bool printed(const Arena& a, const std::string& text) {
  for (const sv::NetEvent& e : a.s().out.events)
    if (e.kind == sv::EventKind::print && e.text == text) return true;
  return false;
}

}  // namespace

TEST(a_sphere_follows_above_its_owner_one_at_a_time_and_runs_out_after_30_seconds) {
  auto a = arena(600);
  launch(*a, "item_sphere_defender", "defender sphere");
  const sv::Q2Missile* m = sphere(*a);
  CHECK(m != nullptr && m->kind == sv::SqShot::defender_sphere);
  CHECK(a->p(0).body.counts[item("item_sphere_defender")] == 0);
  CHECK(a->p(0).defender_until > a->s().level_time + 29);
  run(*a, 2 * kSecond);
  const sh::Vec3f top = at(*a, 0) + sh::Vec3f{0, 0, a->p(0).maxs.z + 4};
  CHECK_MSG(sh::length(m->origin - top) < 2,
            std::to_string(m->origin.z) + " " + std::to_string(top.z));
  // A second: kept, with the words; and none may be picked up meanwhile.
  a->p(0).body.counts[item("item_sphere_hunter")] = 1;
  CHECK(a->r.client_command(0, "use hunter sphere") == sv::CommandResult::mod);
  CHECK(printed(*a, "Only one sphere at a time!\n"));
  CHECK(a->p(0).body.counts[item("item_sphere_hunter")] == 1);
  CHECK(sv::gz_sphere_refused(a->s(), 0, item("item_sphere_vengeance")));
  CHECK(!sv::gz_sphere_refused(a->s(), 0, item("item_double")));
  run(*a, 29 * kSecond);
  CHECK(sphere(*a) == nullptr);
  CHECK(a->p(0).sphere == -1 && a->p(0).defender_until == 0);
  CHECK(!sv::gz_sphere_refused(a->s(), 0, item("item_sphere_vengeance")));
}

TEST(the_defender_halves_its_owners_blows_and_shoots_back_at_who_hurt_him) {
  auto a = arena(200);
  launch(*a, "item_sphere_defender", "defender sphere");
  a->p(0).body.armour_count = 0;
  const float before = a->p(0).health;
  sv::fight_hurt(a->s(), 0, 1, {1, 0, 0}, 40, 0, int(sv::SqWeapon::shotgun));
  CHECK_MSG(before - a->p(0).health == 20, std::to_string(before - a->p(0).health));
  CHECK(a->p(0).hurt_by == 1);
  const float ben = a->p(1).health;
  bool bolt = false;
  for (int i = 0; i < kSecond && a->p(1).health == ben; i++) {
    run(*a, 1);
    for (const sv::Q2Missile& m : a->s().missiles)
      bolt |= m.live && m.kind == sv::SqShot::green_bolt && m.owner == 0;
  }
  CHECK_MSG(bolt, "the defender never fired");
  CHECK_MSG(a->p(1).health < ben, "its bolt never reached Ben");
  // Killed, its owner's defender bursts with him.
  sv::fight_hurt(a->s(), 0, 1, {1, 0, 0}, 1000, 0, int(sv::SqWeapon::railgun), 3);
  CHECK(!a->p(0).alive);
  CHECK(sphere(*a) == nullptr);
}

TEST(the_vengeance_sphere_purges_who_brings_its_owner_under_25) {
  auto a = arena(300);
  launch(*a, "item_sphere_vengeance", "vengeance sphere");
  a->p(0).body.armour_count = 0;
  a->p(0).health = 30;
  sv::fight_hurt(a->s(), 0, 1, {1, 0, 0}, 10, 0, int(sv::SqWeapon::shotgun));
  CHECK(a->p(0).alive && a->p(0).health < 25);
  const sv::Q2Missile* m = sphere(*a);
  CHECK(m != nullptr && m->prey == 1);
  for (int i = 0; i < 3 * kSecond && a->p(1).alive; i++) run(*a, 1);
  CHECK_MSG(!a->p(1).alive, "Ben lived");
  CHECK(said(*a, "Ben was purged by Ana's vengeance sphere"));
  CHECK(a->p(0).alive && sphere(*a) == nullptr);
}

TEST(the_hunter_chases_its_owners_killer_and_his_view_rides_it) {
  auto a = arena(600);
  launch(*a, "item_sphere_hunter", "hunter sphere");
  sv::fight_hurt(a->s(), 0, 1, {1, 0, 0}, 1000, 0, int(sv::SqWeapon::railgun), 3);
  CHECK(!a->p(0).alive);
  const sv::Q2Missile* m = sphere(*a);
  CHECK(m != nullptr && m->prey == 1);
  CHECK(m->dies_at >= a->s().level_time + 15);
  CHECK(sv::gz_sphere_view(a->s(), 0) && a->p(0).gibbed);
  run(*a, kSecond / 2);
  CHECK_MSG(sh::length(at(*a, 0) - m->origin) < 1, "his view is not at the sphere");
  for (int i = 0; i < 4 * kSecond && a->p(1).alive; i++) run(*a, 1);
  CHECK_MSG(!a->p(1).alive, "Ben lived");
  CHECK(said(*a, "Ben was killed like a dog by Ana's hunter sphere"));
  CHECK(sphere(*a) == nullptr);
  CHECK(sv::gz_sphere_view(a->s(), 0));  // stopped where the sphere was
}

TEST(a_hunter_with_no_camera_hunts_on_and_an_idle_vengeance_bursts_with_its_owner) {
  auto a = arena(600);
  a->r.console("dmflags 1024");  // forced respawn: no camera
  launch(*a, "item_sphere_hunter", "hunter sphere");
  sv::fight_hurt(a->s(), 0, 1, {1, 0, 0}, 1000, 0, int(sv::SqWeapon::railgun), 3);
  CHECK(sphere(*a) != nullptr && !sv::gz_sphere_view(a->s(), 0));
  auto b = arena(600);
  launch(*b, "item_sphere_vengeance", "vengeance sphere");
  sv::fight_hurt(b->s(), 0, 1, {1, 0, 0}, 1000, 0, int(sv::SqWeapon::railgun), 3);
  CHECK(sphere(*b) == nullptr);
}

TEST(a_dopplegangers_sphere_hunts_at_once_names_the_doppleganger_and_spares_its_owner) {
  auto a = arena(400);
  const float ana = a->p(0).health;
  sv::gz_release_sphere(a->s(), 0, 1, at(*a, 0) + sh::Vec3f{0, 0, 32}, true);
  const sv::Q2Missile* m = sphere(*a);
  CHECK(m != nullptr && m->prey == 1 && a->p(0).sphere == -1);
  for (int i = 0; i < 3 * kSecond && a->p(1).alive; i++) run(*a, 1);
  CHECK(!a->p(1).alive);
  CHECK(said(*a, "Ben was hunted down by Ana's doppleganger"));
  CHECK(a->p(0).health == ana);
}
