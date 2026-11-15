// Tests: the campaign in C++ (games/shooters_edge/rules/campaign*.cpp) plays the co-op runs the
// compiled QC campaign played before it was deleted: the coop hall (its door, button, items and
// monsters) with companions fighting beside a scripted leader. The numbers were the QC's, taken
// while the two ran side by side and agreed frame by frame (every frame's digest of the loop's
// state, every player's origin, view and health; the kills and who made them, the door, the
// button, the exit); a change that moves one is a change to the rules.
//
// ge:summary Tests: the campaign's rules in C++ play the QC campaign's co-op runs to the bit,
// ge:summary pinned
// ge:keywords test campaign native-campaign parity coop-run campaign_mod loop-digest pinned
// ge:skill progs shooter testing
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

#include "check.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/bots/coop_run.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/campaign.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/sim/qc_program.hpp"

namespace sb = shooters_edge::bots;
namespace sv = shooters_edge::server;
namespace pg = ge::progs;
namespace sh = ge::shooter;

namespace {

const pg::Progs& native() {
  static const pg::Progs p = [] {
    pg::Progs out;
    std::string error;
    if (!sv::campaign_mod(out, error)) out = {};
    return out;
  }();
  return p;
}

struct Pin {
  size_t frames, kills;
  int monsters, button_ms, door_ms, leader_deaths;
  bool exited;
  uint64_t hash;  // FNV-1a over every frame's digest, then each kill's monster and companion
};

uint64_t fold(const sb::CoopReport& r) {
  uint64_t h = 1469598103934665603ull;
  const auto mix = [&h](uint64_t w) {
    for (int i = 0; i < 8; i++) {
      h ^= (w >> (8 * i)) & 0xffu;
      h *= 1099511628211ull;
    }
  };
  for (uint64_t d : r.digests) mix(d);
  for (const sb::CoopKill& k : r.kills) {
    mix(static_cast<uint64_t>(static_cast<uint32_t>(k.ent)));
    mix(static_cast<uint64_t>(static_cast<uint32_t>(k.by)));
  }
  return h;
}

int ms(double s) { return s < 0 ? -1 : static_cast<int>(std::lround(s * 1000.0)); }

// `o` by reference: a copy of its one-element `skills` trips GCC 13's -O3 array-bounds false
// positive.
void same_run(sb::CoopOptions& o, const Pin& qc) {
  o.root = GE_SOURCE_DIR;
  o.progs = &native();
  const sb::CoopReport n = sb::run_coop(o);
  CHECK_MSG(n.ok, n.error);
  CHECK(n.faults == 0);
  const Pin got = {n.digests.size(),   n.kills.size(),  n.monsters_total, ms(n.button_pressed_at),
                   ms(n.door_open_at), n.leader_deaths, n.exited,         fold(n)};
  const std::string seen =
      "{" + std::to_string(got.frames) + ", " + std::to_string(got.kills) + ", " +
      std::to_string(got.monsters) + ", " + std::to_string(got.button_ms) + ", " +
      std::to_string(got.door_ms) + ", " + std::to_string(got.leader_deaths) + ", " +
      (got.exited ? "true" : "false") + ", " + std::to_string(got.hash) + "ull}";
  CHECK_MSG(got.frames == qc.frames && got.kills == qc.kills && got.monsters == qc.monsters &&
                got.button_ms == qc.button_ms && got.door_ms == qc.door_ms &&
                got.leader_deaths == qc.leader_deaths && got.exited == qc.exited &&
                got.hash == qc.hash,
            "the campaign's run moved from the QC's: now " + seen);
}

}  // namespace

TEST(the_campaign_builds_as_a_native_program) {
  CHECK(native().natives != nullptr);
  CHECK(native().statements.size() <= 1);  // no bytecode: every function is C++
}

TEST(the_campaign_in_cpp_plays_the_qc_coop_hall) {
  sb::CoopOptions o;
  o.seconds = 45;
  same_run(o, {3240, 3, 3, 13208, 13514, 0, false, 15427148075464556034ull});
}

TEST(the_campaign_in_cpp_plays_the_qc_coop_hall_with_two_companions_on_hard) {
  sb::CoopOptions o;
  o.skills = {sh::AimSkill::expert, sh::AimSkill::novice};
  o.skill = 3;
  o.seed = 7;
  o.seconds = 45;
  same_run(o, {3240, 3, 3, 13056, 13361, 0, false, 9622315132312202921ull});
}

TEST(the_campaign_in_cpp_plays_the_qc_coop_hall_to_its_exit) {
  sb::CoopOptions o;
  o.exit = true;
  o.seed = 3;
  o.seconds = 45;
  same_run(o, {3240, 3, 3, 13056, 13361, 0, false, 12686590009719983557ull});
}

// Classic remix's `cr_monsters 2`: the sequel's monsters spawn in the classic campaign with the
// roster's health, play their frames and hurt the player who stands before them.
namespace {

bool sequel_hall(void* /*ctx*/, const std::string& /*name*/, sv::MapSpec& out,
                 std::string& /*error*/) {
  out.models = sv::arena_models();
  out.entities = R"({
"classname" "worldspawn"
}
{
"classname" "info_player_start"
"origin" "-416 0 24"
"angle" "0"
}
{
"classname" "monster_soldier"
"origin" "-32 -192 24"
"angle" "180"
}
{
"classname" "monster_gunner"
"origin" "-32 -64 24"
"angle" "180"
}
{
"classname" "monster_tank"
"origin" "-32 96 40"
"angle" "180"
}
{
"classname" "monster_flyer"
"origin" "-160 200 96"
"angle" "180"
}
)";
  return true;
}

}  // namespace

TEST(the_sequels_monsters_run_in_the_classic_campaign) {
  sv::Campaign c;
  sv::CampaignOptions co;
  co.server.loop.max_edicts = 256;
  co.load = &sequel_hall;
  co.settings = {"deathmatch 0", "coop 0", "skill 1"};
  std::string error;
  CHECK_MSG(sv::campaign_new_game(c, native(), co, "hall", error), error);
  sv::campaign_join(c, 0, "player");
  CHECK(sv::campaign_stats(*sv::qc_of(c.server)).total_monsters == 4);
  const auto& ents = c.server->world.entities;
  float healths[4] = {};
  int n = 0;
  for (size_t e = 2; e < ents.size() && n < 4; e++)
    if (!ents[e].free && ents[e].health > 0) healths[n++] = ents[e].health;
  CHECK(n == 4 && healths[0] == 30 && healths[1] == 175 && healths[2] == 750 && healths[3] == 50);
  float lowest = 100;  // the player's health at its lowest (a death restarts the map)
  bool posed = false;  // a monster on a frame of its own runs
  for (int i = 0; i < 72 * 8; i++) {
    CHECK_MSG(sv::campaign_frame(c, nullptr, 0, error), error);
    const auto& now = c.server->world.entities;
    lowest = std::min(lowest, now[1].health);
    for (size_t e = 2; e < now.size(); e++) posed = posed || (!now[e].free && now[e].frame > 0);
  }
  CHECK_MSG(lowest < 50, "the player's lowest health " + std::to_string(lowest));
  CHECK(posed);
}

// The sequel's medic raises a dead guard while it hunts, and the brain's screen takes a third of a
// blow, in the classic campaign.
namespace {

bool medic_hall(void* /*ctx*/, const std::string& /*name*/, sv::MapSpec& out,
                std::string& /*error*/) {
  out.models = sv::arena_models();
  out.entities = R"({
"classname" "worldspawn"
}
{
"classname" "info_player_start"
"origin" "-416 0 24"
"angle" "0"
}
{
"classname" "monster_medic"
"origin" "-160 0 24"
"angle" "180"
}
{
"classname" "monster_soldier"
"origin" "-160 120 24"
"angle" "180"
}
{
"classname" "monster_brain"
"origin" "-40 -160 24"
"angle" "180"
}
)";
  return true;
}

int monster_with(const sv::Campaign& c, float health, int skip = 0) {
  const auto& ents = c.server->world.entities;
  for (size_t e = 2; e < ents.size(); e++)
    if (!ents[e].free && ents[e].health == health && skip-- == 0) return static_cast<int>(e);
  return -1;
}

}  // namespace

TEST(the_sequels_medic_raises_the_dead_and_the_brains_screen_saves_a_third) {
  sv::Campaign c;
  sv::CampaignOptions co;
  co.server.loop.max_edicts = 256;
  co.load = &medic_hall;
  co.settings = {"deathmatch 0", "coop 0", "skill 0"};
  std::string error;
  CHECK_MSG(sv::campaign_new_game(c, native(), co, "hall", error), error);
  sv::campaign_join(c, 0, "player");
  sv::QcServer& s = *sv::qc_of(c.server);
  const int guard = monster_with(c, 30), brain = monster_with(c, 300, 1);  // after the medic
  CHECK(guard > 0 && brain > 0);
  if (guard < 0 || brain < 0) return;
  const auto blow = [&](int targ, float points) {
    const int fn = pg::find_function(native(), "damage");
    pg::set_global_int(s.q.vm, pg::parm(0), pg::entity_value(s.q.vm, targ));
    pg::set_global_int(s.q.vm, pg::parm(1), pg::entity_value(s.q.vm, 1));
    pg::set_global_int(s.q.vm, pg::parm(2), pg::entity_value(s.q.vm, 1));
    pg::set_global_float(s.q.vm, pg::parm(3), points);
    shooters_edge::sim::enter_qc(s.q, s.sv);  // as the server's own calls run it
    CHECK_MSG(shooters_edge::sim::run_qc(s.q, fn), s.q.last_fault);
    shooters_edge::sim::leave_qc(s.q);
  };
  const auto health = [&](int e) {
    return pg::as_float(*pg::entity_field(s.q.vm, e, s.q.layout->f.health));
  };
  for (int i = 0; i < 8; i++) CHECK_MSG(sv::campaign_frame(c, nullptr, 0, error), error);
  const float before = health(brain);
  blow(brain, 30);
  CHECK_MSG(before - health(brain) == 20, std::to_string(health(brain)));  // 10 on the screen
  blow(guard, 40);  // dead, not torn apart (its gib health is -30)
  CHECK(health(guard) <= 0);
  bool raised = false;
  for (int i = 0; i < 72 * 10 && !raised; i++) {
    CHECK_MSG(sv::campaign_frame(c, nullptr, 0, error), error);
    raised = health(guard) == 30;
  }
  CHECK(raised);
  CHECK(sv::campaign_stats(s).total_monsters == 4);  // counted again
}
