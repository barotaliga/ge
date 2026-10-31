// Tests: the monsters of our campaign mod (games/shooters_edge/qc/campaign/ai.qc, monsters.qc)
// on the campaign host, in the arena's west room. Each counts at spawn; a rifleman facing the
// player notices him and shoots; one facing away notices only a player who made noise; a death
// counts, is told and fires the monster's targets; a hound bites a player beside it; a hound on a
// path walks it corner to corner.
//
// ge:summary Tests: our campaign mod's monsters: counting, noticing, shooting, biting, paths
// ge:keywords test campaign-mod monsters monster-ai rifleman hound path-corner killed-monsters
// ge:keywords notice show-hostile
// ge:skill progs shooter testing
#include <algorithm>
#include <filesystem>
#include <string>

#include "check.hpp"
#include "progs/layout.hpp"
#include "progs/qc_compile.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/server/campaign.hpp"
#include "shooters_edge/server/mod_match.hpp"

namespace sv = shooters_edge::server;
namespace pg = ge::progs;
namespace sh = ge::shooter;

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
  const sh::Edict& ed(int e) { return s().sv.edicts[static_cast<size_t>(e)]; }
  // One host frame with the player looking along `yaw` and pressing `buttons`.
  bool frame(float yaw = 0, uint8_t buttons = 0) {
    sh::Packet pk;
    pk.client = 0;
    pk.sequence = ++sequence;
    for (sh::MoveCmd& m : pk.cmds) {
      m = {};
      m.msec = 14;
      m.angles = {0, yaw, 0};
      m.buttons = buttons;
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
    CHECK_MSG(g.frame(180, 1), g.error);
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
