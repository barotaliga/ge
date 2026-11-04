// Tests: the monsters of our campaign mod (games/shooters_edge/qc/campaign/ai.qc, monsters.qc)
// on the campaign host, in the arena's west room. Each counts at spawn; a rifleman facing the
// player notices him and shoots; one facing away notices only a player who made noise; a death
// counts, is told and fires the monster's targets; a hound bites a player beside it; a hound on a
// path walks it corner to corner. The kinds that fight up close and in the air: a swordsman and a
// brute hurt a player near them, a ghoul shrugs off a small hit and is knocked down by a big one
// and gets up again whole, and only its whole health in one blow ends it; a flyer keeps its height
// and spits at the player. The big and late kinds: each counts at spawn and hurts a player in front
// of it, the giant takes half of a blast, and a spawn bursts when it dies.
//
// ge:summary Tests: our campaign mod's monsters: counting, noticing, attacking, paths, each kind
// ge:keywords test campaign-mod monsters monster-ai rifleman hound path-corner killed-monsters
// ge:keywords notice show-hostile swordsman brute ghoul flyer eel knockdown leaper giant caster
// ge:keywords trooper spawn heavy-swordsman blast-share
// ge:skill progs shooter testing
#include <algorithm>
#include <filesystem>
#include <initializer_list>
#include <string>

#include "check.hpp"
#include "progs/layout.hpp"
#include "progs/qc_compile.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/server/campaign.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/sim/qc_program.hpp"

namespace sv = shooters_edge::server;
namespace pg = ge::progs;
namespace sh = ge::shooter;
namespace se = shooters_edge::sim;

namespace {

std::string g_things;  // the map's entities after the world and the start

bool load(void* /*ctx*/, const std::string& /*name*/, sv::MapSpec& out, std::string& /*error*/) {
  out.models = sv::arena_models(true);
  out.entities = R"({
"classname" "worldspawn"
}
{
"classname" "info_player_start"
"origin" "-416 0 24"
}
)" + g_things;
  return true;
}

const pg::Progs& mod() {
  static const pg::Progs p = [] {
    const pg::QcProject built = pg::compile_project(std::filesystem::path(GE_SOURCE_DIR) /
                                                    "games/shooters_edge/qc/campaign/progs.src");
    for (const pg::QcDiagnostic& d : built.result.diagnostics)
      CHECK_MSG(!d.error, pg::diagnostic_text(d));
    return built.result.progs;
  }();
  return p;
}

struct Game {
  sv::Campaign c;
  std::string error;
  uint32_t sequence = 0;
  explicit Game(const std::string& things) {
    g_things = things;
    sv::CampaignOptions o;
    o.server.loop.max_edicts = 128;
    o.load = &load;
    o.settings = {"deathmatch 0"};
    CHECK_MSG(sv::campaign_new_game(c, mod(), o, "first", error), error);
    sv::campaign_join(c, 0, "player");
  }
  sv::QcServer& s() { return *c.server; }
  const pg::LayoutFields& f() { return s().q.layout->f; }
  float field(int e, int ofs) { return pg::as_float(*pg::entity_field(s().q.vm, e, ofs)); }
  void set(int e, int ofs, float v) { *pg::entity_field(s().q.vm, e, ofs) = pg::as_word(v); }
  int entity_of(int e, int ofs) {
    return pg::entity_number(s().q.vm, static_cast<int32_t>(*pg::entity_field(s().q.vm, e, ofs)));
  }
  float global(const char* name) {
    const pg::Def* d = pg::find_global(mod(), name);
    return d ? pg::as_float(s().q.vm.globals[d->offset]) : -1;
  }
  int find(const char* classname, int after = 0) {
    for (int e = after + 1; e < s().sv.num_edicts; e++) {
      const char* cn = pg::vm_string(
          s().q.vm, static_cast<int32_t>(*pg::entity_field(s().q.vm, e, f().classname)));
      if (!s().sv.edicts[static_cast<size_t>(e)].free && cn != nullptr &&
          std::string(cn) == classname)
        return e;
    }
    return 0;
  }
  // A function of the program called with these parameter words, the way the host calls a hook
  // with arguments.
  bool call(const char* name, std::initializer_list<uint32_t> args) {
    const int fn = pg::find_function(mod(), name);
    if (fn < 0) return false;
    pg::Vm& vm = s().q.vm;
    se::enter_qc(s().q, s().sv);
    int i = 0;
    for (uint32_t a : args) vm.globals[static_cast<size_t>(pg::parm(i++))] = a;
    const bool ok = se::run_qc(s().q, fn);
    se::leave_qc(s().q);
    return ok;
  }
  uint32_t ent(int e) { return static_cast<uint32_t>(pg::entity_value(s().q.vm, e)); }
  // One blow of `points` on `target` from `attacker`, through the program's own `damage`.
  bool hit(int target, int attacker, float points) {
    return call("damage", {ent(target), ent(attacker), ent(attacker), pg::as_word(points)});
  }
  // A blast of `points` centred on `at`'s origin, through the program's own `radius_damage`.
  bool blast(int at, int attacker, float points) {
    return call("radius_damage", {ent(at), ent(attacker), pg::as_word(points), ent(0), 0});
  }
  const sh::Edict& ed(int e) { return s().sv.edicts[static_cast<size_t>(e)]; }
  // One host frame with the player looking along `yaw` and pressing `buttons`.
  bool frame(float yaw = 0, uint8_t buttons = 0, uint8_t impulse = 0) {
    sh::Packet pk;
    pk.client = 0;
    pk.sequence = ++sequence;
    for (sh::MoveCmd& m : pk.cmds) {
      m = {};
      m.msec = 14;
      m.angles = {0, yaw, 0};
      m.buttons = buttons;
      m.impulse = impulse;
    }
    return sv::campaign_frame(c, &pk, 1, error);
  }
};

constexpr int kFrames = 72;  // host frames a second

}  // namespace

TEST(monsters_count_at_spawn) {
  Game g(R"({
"classname" "monster_army"
"origin" "-100 200 24"
}
{
"classname" "monster_dog"
"origin" "-100 -200 24"
}
)");
  CHECK(g.s().q.faults == 0);
  CHECK(g.global("total_monsters") == 2);
  const int army = g.find("monster_army"), dog = g.find("monster_dog");
  CHECK(army > 0 && dog > 0);
  CHECK(g.field(army, g.f().health) == 30);
  CHECK(g.field(dog, g.f().health) == 25);
  CHECK(g.ed(army).origin.z > 23.9f && g.ed(army).origin.z < 24.1f);  // dropped to the floor
}

TEST(a_rifleman_facing_the_player_notices_and_shoots_him) {
  Game g(R"({
"classname" "monster_army"
"origin" "-100 0 24"
"angle" "180"
}
)");
  const int army = g.find("monster_army");
  for (int i = 0; i < kFrames * 6 && g.field(1, g.f().health) == 100; i++)
    CHECK_MSG(g.frame(180), g.error);
  CHECK(g.entity_of(army, g.f().enemy) == 1);
  CHECK(g.field(1, g.f().health) < 100);
  CHECK(g.s().q.faults == 0);
}

TEST(a_rifleman_facing_away_notices_only_a_noisy_player) {
  Game g(R"({
"classname" "monster_army"
"origin" "-100 0 24"
"angle" "0"
}
)");
  const int army = g.find("monster_army");
  for (int i = 0; i < kFrames * 3; i++) CHECK_MSG(g.frame(), g.error);
  CHECK(g.entity_of(army, g.f().enemy) == 0);  // near, behind it and quiet: unseen
  // The player fires (away from it, into the west wall): the noise gives him away.
  for (int i = 0; i < kFrames && g.entity_of(army, g.f().enemy) == 0; i++)
    CHECK_MSG(g.frame(0, 1), g.error);
  CHECK(g.entity_of(army, g.f().enemy) == 1);
  CHECK(g.s().q.faults == 0);
}

TEST(a_death_counts_and_fires_the_monsters_targets) {
  Game g(R"({
"classname" "monster_army"
"origin" "-100 0 24"
"angle" "0"
"killtarget" "gone"
}
{
"classname" "info_notnull"
"origin" "0 300 24"
"targetname" "gone"
}
)");
  const int army = g.find("monster_army");
  CHECK(g.find("info_notnull") > 0);
  g.set(army, g.f().health, 1);
  for (int i = 0; i < kFrames && g.global("killed_monsters") == 0; i++)
    CHECK_MSG(g.frame(0, 1), g.error);  // the scattergun, at it
  CHECK(g.global("killed_monsters") == 1);
  CHECK(g.field(army, g.f().takedamage) == 0);
  CHECK(g.find("info_notnull") == 0);  // its killtarget removed
  CHECK(g.s().q.faults == 0);
}

TEST(a_hound_beside_the_player_bites_him) {
  Game g(R"({
"classname" "monster_dog"
"origin" "-300 0 24"
"angle" "180"
}
)");
  for (int i = 0; i < kFrames * 4 && g.field(1, g.f().health) == 100; i++)
    CHECK_MSG(g.frame(), g.error);
  CHECK(g.field(1, g.f().health) < 100);
  CHECK(g.s().q.faults == 0);
}

TEST(a_hound_on_a_path_walks_it_corner_to_corner) {
  Game g(R"({
"classname" "monster_dog"
"origin" "-100 -300 24"
"target" "p1"
}
{
"classname" "path_corner"
"origin" "-100 200 24"
"targetname" "p1"
"target" "p2"
}
{
"classname" "path_corner"
"origin" "-100 -300 24"
"targetname" "p2"
"target" "p1"
}
)");
  // The player, in, hides from monsters (no-target) so the hound only walks.
  CHECK_MSG(g.frame(), g.error);
  g.set(1, g.f().flags, g.field(1, g.f().flags) + 128);
  const int dog = g.find("monster_dog");
  float furthest = -1000;
  int goal = g.entity_of(dog, g.f().goalentity), turns = 0;
  for (int i = 0; i < kFrames * 12; i++) {
    CHECK_MSG(g.frame(), g.error);
    furthest = std::max(furthest, g.ed(dog).origin.y);
    if (g.entity_of(dog, g.f().goalentity) != goal) {
      goal = g.entity_of(dog, g.f().goalentity);
      turns++;
    }
  }
  CHECK_MSG(furthest > 150, std::to_string(furthest));  // reached the far corner
  CHECK_MSG(turns >= 2, std::to_string(turns));         // and turned back at each end
  CHECK(g.entity_of(dog, g.f().enemy) == 0);
  CHECK(g.s().q.faults == 0);
}

TEST(the_close_and_air_kinds_count_at_spawn) {
  Game g(R"({
"classname" "monster_knight"
"origin" "-100 200 24"
}
{
"classname" "monster_ogre"
"origin" "-100 -200 24"
}
{
"classname" "monster_zombie"
"origin" "0 300 24"
}
{
"classname" "monster_wizard"
"origin" "0 -300 120"
}
{
"classname" "monster_fish"
"origin" "-300 300 60"
}
)");
  CHECK(g.s().q.faults == 0);
  CHECK(g.global("total_monsters") == 5);
  CHECK(g.field(g.find("monster_knight"), g.f().health) == 75);
  CHECK(g.field(g.find("monster_ogre"), g.f().health) == 200);
  CHECK(g.field(g.find("monster_zombie"), g.f().health) == 60);
  CHECK(g.field(g.find("monster_wizard"), g.f().health) == 80);
  CHECK(g.field(g.find("monster_fish"), g.f().health) == 25);
  CHECK(g.ed(g.find("monster_wizard")).origin.z == 120);  // a flyer stays where it is placed
}

TEST(a_swordsman_and_a_brute_hurt_a_player_near_them) {
  for (const char* kind : {"monster_knight", "monster_ogre"}) {
    Game g(std::string(R"({
"classname" ")") +
           kind + R"("
"origin" "-200 0 24"
"angle" "180"
}
)");
    for (int i = 0; i < kFrames * 8 && g.field(1, g.f().health) == 100; i++)
      CHECK_MSG(g.frame(180), g.error);
    CHECK_MSG(g.field(1, g.f().health) < 100, kind);
    CHECK_MSG(g.s().q.faults == 0, kind);
  }
}

TEST(a_ghoul_shrugs_off_small_hits_and_is_knocked_down_by_big_ones) {
  Game g(R"({
"classname" "monster_zombie"
"origin" "-200 0 24"
"angle" "180"
}
)");
  const int ghoul = g.find("monster_zombie");
  // The scattergun's pellets, each a small hit: it keeps its health, and turns on the player.
  CHECK_MSG(g.frame(0, 1), g.error);
  for (int i = 0; i < 10; i++) CHECK_MSG(g.frame(0), g.error);
  CHECK(g.field(ghoul, g.f().health) == 60);
  CHECK(g.field(ghoul, g.f().takedamage) != 0);
  CHECK(g.entity_of(ghoul, g.f().enemy) == 1);
  // A blow of 40 at once: down, not dead.
  CHECK(g.hit(ghoul, 1, 40));
  CHECK(g.field(ghoul, g.f().takedamage) == 0);
  CHECK(g.global("killed_monsters") == 0);
  // It gets up again whole.
  for (int i = 0; i < kFrames * 7 && g.field(ghoul, g.f().takedamage) == 0; i++)
    CHECK_MSG(g.frame(180), g.error);
  CHECK(g.field(ghoul, g.f().takedamage) != 0);
  CHECK(g.field(ghoul, g.f().health) == 60);
  // A blow of its whole health at once: in pieces.
  CHECK(g.hit(ghoul, 1, 60));
  CHECK(g.global("killed_monsters") == 1);
  CHECK(g.s().q.faults == 0);
}

TEST(a_flyer_keeps_its_height_and_spits_at_the_player) {
  Game g(R"({
"classname" "monster_wizard"
"origin" "-150 0 100"
"angle" "180"
}
)");
  const int wizard = g.find("monster_wizard");
  float lowest = 1000;
  for (int i = 0; i < kFrames * 8 && g.field(1, g.f().health) == 100; i++) {
    CHECK_MSG(g.frame(180), g.error);
    lowest = std::min(lowest, g.ed(wizard).origin.z);
  }
  CHECK(g.field(1, g.f().health) < 100);
  CHECK_MSG(lowest > 40, std::to_string(lowest));  // flying, not fallen to the floor
  CHECK(g.s().q.faults == 0);
}

TEST(the_big_and_late_kinds_count_at_spawn) {
  Game g(R"({
"classname" "monster_demon1"
"origin" "-100 200 24"
}
{
"classname" "monster_shambler"
"origin" "-100 -200 24"
}
{
"classname" "monster_shalrath"
"origin" "100 0 24"
}
{
"classname" "monster_enforcer"
"origin" "0 300 24"
}
{
"classname" "monster_tarbaby"
"origin" "0 -300 24"
}
{
"classname" "monster_hell_knight"
"origin" "-300 300 24"
}
)");
  CHECK(g.s().q.faults == 0);
  CHECK(g.global("total_monsters") == 6);
  CHECK(g.field(g.find("monster_demon1"), g.f().health) == 300);
  CHECK(g.field(g.find("monster_shambler"), g.f().health) == 600);
  CHECK(g.field(g.find("monster_shalrath"), g.f().health) == 400);
  CHECK(g.field(g.find("monster_enforcer"), g.f().health) == 80);
  CHECK(g.field(g.find("monster_tarbaby"), g.f().health) == 80);
  CHECK(g.field(g.find("monster_hell_knight"), g.f().health) == 250);
}

TEST(each_big_and_late_kind_hurts_a_player_in_front_of_it) {
  for (const char* kind : {"monster_demon1", "monster_shambler", "monster_shalrath",
                           "monster_enforcer", "monster_tarbaby", "monster_hell_knight"}) {
    Game g(std::string(R"({
"classname" ")") +
           kind + R"("
"origin" "-200 0 24"
"angle" "180"
}
)");
    for (int i = 0; i < kFrames * 10 && g.field(1, g.f().health) == 100; i++)
      CHECK_MSG(g.frame(180), g.error);
    CHECK_MSG(g.field(1, g.f().health) < 100, kind);
    CHECK_MSG(g.s().q.faults == 0, kind);
  }
}

TEST(the_giant_takes_half_of_a_blast) {
  Game g(R"({
"classname" "monster_shambler"
"origin" "-100 0 24"
}
{
"classname" "monster_hell_knight"
"origin" "-100 200 24"
}
)");
  const int giant = g.find("monster_shambler"), knight = g.find("monster_hell_knight");
  // A blast of 110 on each one's own origin: the giant's centre is 20 units up (100, half of
  // it taken), the heavy swordsman's 8 (106, all of it).
  CHECK(g.blast(giant, 1, 110));
  CHECK(g.blast(knight, 1, 110));
  CHECK_MSG(g.field(giant, g.f().health) == 550, std::to_string(g.field(giant, g.f().health)));
  CHECK_MSG(g.field(knight, g.f().health) == 144, std::to_string(g.field(knight, g.f().health)));
  CHECK(g.s().q.faults == 0);
}

TEST(a_spawn_bursts_when_it_dies) {
  Game g(R"({
"classname" "monster_tarbaby"
"origin" "-340 0 24"
"spawnflags" "1"
}
)");
  CHECK_MSG(g.frame(180), g.error);
  const int spawn = g.find("monster_tarbaby");
  CHECK(spawn > 0);
  CHECK(g.hit(spawn, 1, 100));
  CHECK(g.global("killed_monsters") == 1);
  CHECK(g.find("monster_tarbaby") == 0);  // nothing left of it
  CHECK(g.field(1, g.f().health) < 100);  // the player beside it caught the burst
  CHECK(g.s().q.faults == 0);
}
