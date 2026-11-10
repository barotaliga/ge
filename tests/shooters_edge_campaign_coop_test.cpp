// Tests: our campaign mod in coop, the rules a coop game is set by: a teammate's blow neither harms
// nor pushes by default; `sv_friendlypush 1` lets it push without harm (a rocket boost); and
// `sv_friendlyfire 1` lets it harm; a player's own blast always harms himself. Players joining
// spawn on coop starts no one stands on. On the start map an episode after the first stays shut
// until the one before is done (`sv_episodes_done`), and a rune taken in coop sets its bit.
//
// ge:summary Tests: our campaign in coop: friendly fire and push, free coop starts, episodes opened
// ge:keywords test coop co-op friendly-fire sv_friendlyfire sv_friendlypush rocket-boost
// ge:keywords coop-start episode-unlock sv_episodes_done rune
// ge:skill progs shooter testing
#include <filesystem>
#include <string>
#include <vector>

#include "check.hpp"
#include "progs/file.hpp"
#include "progs/layout.hpp"
#include "progs/qc_compile.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/server/campaign.hpp"
#include "shooters_edge/server/mod_match.hpp"

namespace sv = shooters_edge::server;
namespace pg = ge::progs;
namespace sh = ge::shooter;

namespace {

// The arena as a start map: a start, two coop starts, and a way into the second episode over the
// plat's brush.
const char* const kMap = R"({
"classname" "worldspawn"
}
{
"classname" "info_player_start"
"origin" "-416 0 24"
}
{
"classname" "info_player_coop"
"origin" "-416 96 24"
}
{
"classname" "info_player_coop"
"origin" "-416 -96 24"
}
{
"classname" "info_intermission"
"origin" "0 0 128"
"mangle" "10 90 0"
}
{
"classname" "trigger_changelevel"
"model" "*2"
"map" "e2m1"
}
)";

bool load(void* /*ctx*/, const std::string& /*name*/, sv::MapSpec& out, std::string& /*error*/) {
  out.models = sv::arena_models(true);  // with the door's and the plat's brushes
  out.entities = kMap;
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
  int players = 0;
  explicit Game(int n, std::vector<std::string> settings = {}) : players(n) {
    sv::CampaignOptions o;
    o.server.loop.max_edicts = 160;
    o.load = &load;
    o.settings = {"deathmatch 0", "coop 1"};
    for (std::string& line : settings) o.settings.push_back(std::move(line));
    CHECK_MSG(sv::campaign_new_game(c, mod(), o, "start", error), error);
    for (int k = 0; k < n; k++) sv::campaign_join(c, k, "player" + std::to_string(k));
  }
  sv::QcServer& s() { return *sv::qc_of(c.server); }
  pg::Vm& vm() { return s().q.vm; }
  const pg::LayoutFields& f() { return s().q.layout->f; }
  float field(int e, int ofs) { return pg::as_float(*pg::entity_field(vm(), e, ofs)); }
  float global(const char* name) {
    const pg::Def* d = pg::find_global(mod(), name);
    return d ? pg::as_float(vm().globals[d->offset]) : -1;
  }
  bool frame() {
    std::vector<sh::Packet> pk(static_cast<size_t>(players));
    sequence++;
    for (int k = 0; k < players; k++) {
      pk[static_cast<size_t>(k)].client = k;
      pk[static_cast<size_t>(k)].sequence = sequence;
      for (sh::MoveCmd& m : pk[static_cast<size_t>(k)].cmds) {
        m = {};
        m.msec = 14;
      }
    }
    return sv::campaign_frame(c, pk.data(), players, error);
  }
  // `damage(targ, inflictor, attacker, points)` called as the program's own code calls it.
  void blow(int targ, int inflictor, int attacker, float points) {
    const int fn = pg::find_function(mod(), "damage");
    CHECK(fn > 0);
    pg::set_global_int(vm(), pg::parm(0), pg::entity_value(vm(), targ));
    pg::set_global_int(vm(), pg::parm(1), pg::entity_value(vm(), inflictor));
    pg::set_global_int(vm(), pg::parm(2), pg::entity_value(vm(), attacker));
    pg::set_global_float(vm(), pg::parm(3), points);
    CHECK_MSG(pg::call_function(vm(), fn), pg::fault_text(vm()));
  }
  // Player 2 a step from player 1, both still: player 1's blow of 50 on him, then his health and
  // how fast it sent him.
  void blow_on_mate(float& health, float& speed) {
    for (int i = 0; i < 4; i++) CHECK_MSG(frame(), error);
    // In the program's own fields: player 2 64 units along +x from player 1, both still.
    for (int k = 0; k < 3; k++) {
      *pg::entity_field(vm(), 2, f().origin + k) =
          pg::as_word(field(1, f().origin + k) + (k == 0 ? 64.0f : 0.0f));
      *pg::entity_field(vm(), 1, f().velocity + k) = pg::as_word(0.0f);
      *pg::entity_field(vm(), 2, f().velocity + k) = pg::as_word(0.0f);
    }
    blow(2, 1, 1, 50);
    health = field(2, f().health);
    speed = field(2, f().velocity);  // along +x: away from player 1
  }
  uint32_t sequence = 0;
};

}  // namespace

TEST(a_teammates_blow_neither_harms_nor_pushes_by_default) {
  Game g(2);
  float health = 0, speed = 0;
  g.blow_on_mate(health, speed);
  CHECK_MSG(health == 100, std::to_string(health));
  CHECK_MSG(speed == 0, std::to_string(speed));
  CHECK(g.s().q.faults == 0);
}

TEST(friendly_push_lets_a_teammates_blow_push_without_harm) {
  Game g(2, {"sv_friendlypush 1"});
  float health = 0, speed = 0;
  g.blow_on_mate(health, speed);
  CHECK_MSG(health == 100, std::to_string(health));
  CHECK_MSG(speed > 100, std::to_string(speed));  // away from the blow: a rocket boost
}

TEST(friendly_fire_lets_a_teammates_blow_harm) {
  Game g(2, {"sv_friendlyfire 1"});
  float health = 0, speed = 0;
  g.blow_on_mate(health, speed);
  CHECK_MSG(health < 100, std::to_string(health));
  CHECK_MSG(speed > 100, std::to_string(speed));
}

TEST(a_players_own_blast_harms_himself_whatever_the_settings) {
  Game g(1);
  for (int i = 0; i < 4; i++) CHECK_MSG(g.frame(), g.error);
  g.blow(1, 1, 1, 30);
  CHECK(g.field(1, g.f().health) < 100);
}

TEST(players_joining_take_coop_starts_no_one_stands_on) {
  Game g(3);
  for (int i = 0; i < 4; i++) CHECK_MSG(g.frame(), g.error);
  const sh::Vec3f a = g.s().sv.edicts[1].origin, b = g.s().sv.edicts[2].origin,
                  c = g.s().sv.edicts[3].origin;
  const auto apart = [](sh::Vec3f p, sh::Vec3f q) {
    const float dx = p.x - q.x, dy = p.y - q.y;
    return dx * dx + dy * dy > 40 * 40;
  };
  CHECK_MSG(apart(a, b) && apart(a, c) && apart(b, c),
            std::to_string(a.y) + " " + std::to_string(b.y) + " " + std::to_string(c.y));
}

TEST(eight_players_play_a_coop_map) {
  Game g(8);
  for (int i = 0; i < 72; i++) CHECK_MSG(g.frame(), g.error);
  for (int k = 1; k <= 8; k++) CHECK(g.field(k, g.f().health) == 100);
  CHECK(g.s().q.faults == 0);
}

namespace {

// The player stood in the way into the second episode for a second.
void walk_into_episode_two(Game& g) {
  const sh::Edict& t = g.s().sv.edicts[static_cast<size_t>([&] {
    for (int e = 1; e < g.s().sv.num_edicts; e++) {
      const char* cn = pg::vm_string(
          g.vm(), static_cast<int32_t>(*pg::entity_field(g.vm(), e, g.f().classname)));
      if (cn != nullptr && std::string(cn) == "trigger_changelevel") return e;
    }
    return 0;
  }())];
  for (int i = 0; i < 72 && g.c.history.size() == 1; i++) {
    sh::Edict& p = g.s().sv.edicts[1];
    p.origin = {(t.absmin.x + t.absmax.x) / 2, (t.absmin.y + t.absmax.y) / 2,
                (t.absmin.z + t.absmax.z) / 2};
    p.velocity = {};
    CHECK_MSG(g.frame(), g.error);
  }
}

}  // namespace

TEST(the_second_episode_stays_shut_until_the_first_is_done) {
  Game g(1);
  walk_into_episode_two(g);
  CHECK(g.global("intermission_running") == 0);  // the way in refused: still on the start map
  CHECK(g.c.history.size() == 1);
  CHECK(g.s().q.faults == 0);
}

TEST(the_second_episode_opens_once_the_first_is_done) {
  Game g(1, {"sv_episodes_done 1"});
  walk_into_episode_two(g);
  CHECK(g.global("intermission_running") == 1);  // the way in taken: the intermission, then on
  CHECK(g.s().q.faults == 0);
}
