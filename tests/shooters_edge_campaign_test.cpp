// Tests: the campaign host (games/shooters_edge/server/campaign.hpp) on the campaign stub, a
// single-player program compiled from QC (test_mod.hpp): a map's start (the rules written, the
// settle, the clock), a new game's parms, a level change carrying the parms and serverflags, a
// restart, a new game clearing both, and the single-player clock's one think a frame.
//
// ge:summary Tests: the campaign host: map start, parms carried, changelevel, restart, the clock
// ge:keywords test campaign single-player host changelevel restart spawn-parms serverflags skill
// ge:keywords registered coop settle single-player-clock statistics
// ge:skill progs shooter testing
#include <string>
#include <vector>

#include "check.hpp"
#include "progs/layout.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/server/campaign.hpp"
#include "shooters_edge/server/commands.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"

namespace sv = shooters_edge::server;
namespace pg = ge::progs;
namespace sh = ge::shooter;

namespace {

const pg::LayoutFields& kF = pg::kSinglePlayerLayout.f;

struct Maps {
  std::vector<std::string> asked;
};

bool load(void* ctx, const std::string& name, sv::MapSpec& out, std::string& /*error*/) {
  static_cast<Maps*>(ctx)->asked.push_back(name);
  out.models = sv::arena_models();
  out.entities = sv::kStubEntities;
  return true;
}

pg::Progs campaign_mod() {
  pg::Progs p;
  std::string error;
  CHECK_MSG(sv::build_campaign_stub(p, error), error);
  return p;
}

struct Fixture {
  pg::Progs mod = campaign_mod();
  Maps maps;
  sv::Campaign c;
  std::string error;

  bool start(const std::string& map, std::vector<std::string> settings = {},
             bool registered = false) {
    sv::CampaignOptions o;
    o.server.loop.max_edicts = 64;
    o.load = &load;
    o.load_ctx = &maps;
    o.registered = registered;
    o.settings = std::move(settings);
    return sv::campaign_new_game(c, mod, o, map, error);
  }
  sv::QcServer& s() { return *c.server; }
  float f(int e, int ofs) { return pg::as_float(*pg::entity_field(s().q.vm, e, ofs)); }
  // Frames until the map changes (at most `most`).
  int run_until_change(int most) {
    const size_t before = c.history.size();
    for (int i = 0; i < most; i++) {
      CHECK_MSG(sv::campaign_frame(c, nullptr, 0, error), error);
      if (c.history.size() != before) return i + 1;
    }
    return -1;
  }
};

}  // namespace

TEST(a_maps_start_writes_the_rules_and_settles) {
  Fixture fx;
  CHECK_MSG(fx.start("second", {"skill 2.6", "coop 1", "deathmatch 1"}, true), fx.error);
  sv::QcServer& s = fx.s();
  CHECK(s.q.layout == &pg::kSinglePlayerLayout);
  CHECK(s.cvars.find("skill")->value == "3");  // rounded and written back
  CHECK(fx.f(0, kF.max_health) == 1);          // registered
  CHECK(fx.f(0, kF.frags) == 3);
  CHECK(fx.f(0, kF.armorvalue) == 0);  // coop forces deathmatch 0
  CHECK(fx.f(0, kF.ammo_shells) == 1);
  CHECK(fx.f(0, kF.items) == 0);
  // The clock: 1.0, two settling frames of 0.1, then a step of 1/72 after each frame.
  CHECK(s.sv.time == 1.0 + 0.1 + 0.1);
  CHECK(s.sv.set.single_player_clock);
  const double settled = s.sv.time;
  CHECK(sv::campaign_frame(fx.c, nullptr, 0, fx.error));
  CHECK(fx.s().sv.time == settled + sv::kCampaignStep);
  const sv::CampaignStats st = sv::campaign_stats(fx.s());
  CHECK(st.total_monsters == 3 && st.killed_monsters == 1 && st.total_secrets == 2);
  CHECK(st.found_secrets == 0);
  CHECK(fx.c.history.size() == 1 && fx.c.history[0] == "new second");
}

TEST(skill_is_clamped_and_registered_follows_the_content) {
  Fixture fx;
  CHECK_MSG(fx.start("second", {"skill -4"}), fx.error);
  CHECK(fx.s().cvars.find("skill")->value == "0");
  CHECK(fx.f(0, kF.max_health) == 0);  // not registered
  CHECK(fx.f(0, kF.armorvalue) == 1);  // deathmatch as set (the server's default)
}

TEST(a_level_change_carries_the_parms_and_the_flags) {
  Fixture fx;
  CHECK_MSG(fx.start("first", {"skill 1"}), fx.error);
  sv::campaign_join(fx.c, 0, "player");
  CHECK(fx.f(1, kF.health) == 100 && fx.f(1, kF.frags) == 0);  // SetNewParms
  const int frames = fx.run_until_change(200);
  CHECK_MSG(frames > 0, std::to_string(frames));
  CHECK(fx.maps.asked.size() == 2 && fx.maps.asked[1] == "second");  // the second call ignored
  CHECK(fx.c.history.back() == "change second");
  // The player re-entered with SetChangeParms' parms: 75 health, one map played.
  CHECK(fx.f(1, kF.health) == 75 && fx.f(1, kF.frags) == 1);
  CHECK(fx.c.serverflags == 1);
  CHECK(fx.f(0, kF.items) == 1);                    // the next map's worldspawn saw the runes
  CHECK(fx.s().cvars.find("skill")->value == "1");  // the settings carried
}

TEST(a_restart_keeps_the_parms_the_map_was_entered_with) {
  Fixture fx;
  CHECK_MSG(fx.start("first"), fx.error);
  sv::campaign_join(fx.c, 0, "player");
  CHECK(fx.run_until_change(200) > 0);
  sv::QcServer& s = fx.s();
  pg::entity_field(s.q.vm, 1, kF.health)[0] = pg::as_word(10.0f);  // hurt on this map
  sv::server_command(s, "restart");
  CHECK(sv::campaign_frame(fx.c, nullptr, 0, fx.error));
  CHECK(fx.c.history.back() == "restart second");
  CHECK(fx.f(1, kF.health) == 75 && fx.f(1, kF.frags) == 1);
  CHECK(fx.c.serverflags == 1);
}

TEST(a_new_game_clears_the_parms_and_the_flags) {
  Fixture fx;
  CHECK_MSG(fx.start("first"), fx.error);
  sv::campaign_join(fx.c, 0, "player");
  CHECK(fx.run_until_change(200) > 0);
  sv::server_command(fx.s(), "map first");
  CHECK(sv::campaign_frame(fx.c, nullptr, 0, fx.error));
  CHECK(fx.c.history.back() == "new first");
  CHECK(fx.c.serverflags == 0);
  CHECK(fx.f(1, kF.health) == 100 && fx.f(1, kF.frags) == 0);
  CHECK(fx.f(0, kF.items) == 0);
}

TEST(the_single_player_clock_thinks_once_a_frame) {
  // The counter asks to think again 0.001 s on: the multiplayer clock would run it a dozen times
  // in a frame of 1/72 s, the single-player clock once.
  Fixture fx;
  CHECK_MSG(fx.start("second"), fx.error);
  int counter = -1;
  for (int e = 1; e < fx.s().sv.num_edicts; e++) {
    const char* cls = pg::vm_string(
        fx.s().q.vm, static_cast<int32_t>(*pg::entity_field(fx.s().q.vm, e, kF.classname)));
    if (cls != nullptr && std::string(cls) == "counter") counter = e;
  }
  CHECK(counter > 0);
  const float settled = fx.f(counter, kF.frags);
  CHECK(settled == 2);  // one think in each settling frame
  for (int i = 0; i < 10; i++) CHECK(sv::campaign_frame(fx.c, nullptr, 0, fx.error));
  CHECK(fx.f(counter, kF.frags) == settled + 10);
}
