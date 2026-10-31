// Tests: our campaign mod (games/shooters_edge/qc/campaign/) compiled from its sources and played
// on the campaign host. It compiles to the single-player form's layout; a new game starts the
// player on the map's start with a new game's inventory; a secret counts at spawn and is found
// once by touching it; an exit holds the player at the intermission and a button after its wait
// changes the level, the inventory carried in the parms; and a death in single player restarts
// the map with what it was entered with.
//
// ge:summary Tests: our campaign QC mod: the form, the start, secrets, exits, the intermission
// ge:keywords test campaign-mod campaign-variant single-player qc intermission changelevel secret
// ge:keywords spawn-parms restart
// ge:skill progs shooter testing
#include <filesystem>
#include <string>
#include <vector>

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

// The arena with a start, a secret over the door's brush and an exit over the plat's.
const char* const kMap = R"({
"classname" "worldspawn"
}
{
"classname" "info_player_start"
"origin" "-416 0 24"
}
{
"classname" "info_intermission"
"origin" "0 0 128"
"mangle" "10 90 0"
}
{
"classname" "trigger_secret"
"model" "*1"
}
{
"classname" "trigger_changelevel"
"model" "*2"
"map" "second"
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
  Game() {
    sv::CampaignOptions o;
    o.server.loop.max_edicts = 128;
    o.load = &load;
    o.settings = {"deathmatch 0"};  // the host's default is the arena's
    CHECK_MSG(sv::campaign_new_game(c, mod(), o, "first", error), error);
    sv::campaign_join(c, 0, "player");
  }
  sv::QcServer& s() { return *c.server; }
  const pg::LayoutFields& f() { return s().q.layout->f; }
  float field(int e, int ofs) { return pg::as_float(*pg::entity_field(s().q.vm, e, ofs)); }
  float global(const char* name) {
    const pg::Def* d = pg::find_global(mod(), name);
    return d ? pg::as_float(s().q.vm.globals[d->offset]) : -1;
  }
  // The first entity of `classname`.
  int find(const char* classname) {
    for (int e = 1; e < s().sv.num_edicts; e++) {
      const char* cn = pg::vm_string(
          s().q.vm, static_cast<int32_t>(*pg::entity_field(s().q.vm, e, f().classname)));
      if (!s().sv.edicts[static_cast<size_t>(e)].free && cn != nullptr &&
          std::string(cn) == classname)
        return e;
    }
    return 0;
  }
  // The player stood in the middle of entity `e`'s box.
  void stand_in(int e) {
    const sh::Edict& t = s().sv.edicts[static_cast<size_t>(e)];
    sh::Edict& p = s().sv.edicts[1];
    p.origin = {(t.absmin.x + t.absmax.x) / 2, (t.absmin.y + t.absmax.y) / 2,
                (t.absmin.z + t.absmax.z) / 2};
    p.velocity = {};
  }
  bool frame(uint8_t buttons = 0) {
    sh::Packet pk;
    pk.client = 0;
    pk.sequence = ++sequence;
    for (sh::MoveCmd& m : pk.cmds) {
      m = {};
      m.msec = 14;
      m.buttons = buttons;
    }
    return sv::campaign_frame(c, &pk, 1, error);
  }
  uint32_t sequence = 0;
};

}  // namespace

TEST(the_campaign_mod_compiles_to_the_single_player_form) {
  CHECK(mod().crc == pg::kSinglePlayerCrc);
  for (const char* hook : {"SetNewParms", "SetChangeParms", "PutClientInServer", "StartFrame"})
    CHECK_MSG(pg::find_function(mod(), hook) > 0, hook);
}

TEST(a_new_game_starts_on_the_start_with_a_new_inventory) {
  Game g;
  CHECK(g.s().q.faults == 0);
  const sh::Edict& p = g.s().sv.edicts[1];
  CHECK(p.origin.x == -416 && p.origin.y == 0);
  CHECK(g.field(1, g.f().health) == 100);
  CHECK(g.field(1, g.f().ammo_shells) == 25);
}

TEST(a_secret_counts_at_spawn_and_is_found_once) {
  Game g;
  CHECK(g.global("total_secrets") == 1);
  CHECK(g.global("found_secrets") == 0);
  const int secret = g.find("trigger_secret");
  CHECK(secret > 0);
  for (int i = 0; i < 4; i++) {
    g.stand_in(secret);
    CHECK_MSG(g.frame(), g.error);
  }
  CHECK(g.global("found_secrets") == 1);
  CHECK(g.s().q.faults == 0);
}

TEST(an_exit_holds_the_intermission_then_a_button_changes_the_level) {
  Game g;
  *pg::entity_field(g.s().q.vm, 1, g.f().ammo_shells) = pg::as_word(40.0f);
  const int exit = g.find("trigger_changelevel");
  CHECK(exit > 0);
  g.stand_in(exit);
  for (int i = 0; i < 20; i++) CHECK_MSG(g.frame(), g.error);
  CHECK(g.global("intermission_running") == 1);
  CHECK(g.s().sv.edicts[1].origin.z == 128);  // held at the intermission spot
  // A button before the wait is over does nothing; after it, the level changes.
  CHECK_MSG(g.frame(1), g.error);
  CHECK(g.c.history.size() == 1);
  for (int i = 0; i < 72 * 3 && g.c.history.size() == 1; i++) {
    CHECK_MSG(g.frame(static_cast<uint8_t>(i & 1)), g.error);
  }
  CHECK_MSG(g.c.history.size() == 2 && g.c.history.back() == "change second",
            g.c.history.back() + " " + std::to_string(g.c.history.size()));
  CHECK(g.field(1, g.f().ammo_shells) == 40);  // carried in the parms
  CHECK(g.s().q.faults == 0);
}

TEST(a_death_in_single_player_restarts_the_map) {
  Game g;
  *pg::entity_field(g.s().q.vm, 1, g.f().health) = pg::as_word(-10.0f);
  *pg::entity_field(g.s().q.vm, 1, g.f().deadflag) = pg::as_word(2.0f);
  for (int i = 0; i < 72 * 7 && g.c.history.size() == 1; i++) CHECK_MSG(g.frame(), g.error);
  CHECK_MSG(g.c.history.size() == 2 && g.c.history.back().rfind("restart", 0) == 0,
            g.c.history.back() + " " + std::to_string(g.c.history.size()));
  CHECK(g.field(1, g.f().health) == 100);
}
