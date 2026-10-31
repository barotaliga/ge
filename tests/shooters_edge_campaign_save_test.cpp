// Tests: the campaign saved and loaded (games/shooters_edge/server/campaign_save.hpp): a game
// saved mid-map, loaded and run on, has at every later frame the digest of the game never saved;
// the program's strings (the engine's, the zone's) and the carried parms come back; the save is
// refused when it cannot be made (a dead player, coop, a match's program) and a file damaged, of
// another program or of another game is refused on load with the reason.
//
// ge:summary Tests: a campaign saved, loaded and run on bit for bit; the refusals
// ge:keywords test campaign-save campaign-load continue digest refuse damaged strings zone
// ge:skill shooter save testing
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/server/campaign_save.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"

namespace {

namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace pg = ge::progs;
namespace cl = shooters_edge::client;

bool load(void* /*ctx*/, const std::string& /*name*/, sv::MapSpec& out, std::string& /*error*/) {
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

sv::CampaignOptions options(std::vector<std::string> settings) {
  sv::CampaignOptions o;
  o.server.loop.max_edicts = 64;
  o.load = &load;
  o.settings = std::move(settings);
  return o;
}

// A game on `first` with the player in, walking: his command each frame turns and moves him.
struct Game {
  pg::Progs mod = campaign_mod();
  sv::Campaign c;
  std::string error;
  uint32_t sequence = 0;

  explicit Game(std::vector<std::string> settings = {}) {
    settings.insert(settings.begin(), "deathmatch 0");
    CHECK_MSG(sv::campaign_new_game(c, mod, options(std::move(settings)), "first", error), error);
    sv::campaign_join(c, 0, "player");
    c.server->sv.edicts[1].movetype = sh::kMovetypeWalk;  // the stub leaves its player standing
  }
  void frame(int k) {
    sh::Packet p;
    p.client = 0;
    p.sequence = ++sequence;
    sh::MoveCmd m;
    m.msec = 14;
    m.forward = static_cast<int16_t>(k % 40 < 20 ? 320 : -200);
    m.side = static_cast<int16_t>(k % 30 < 10 ? 160 : 0);
    m.angles = {0, static_cast<float>((k * 3) % 360), 0};
    m.buttons = static_cast<uint8_t>(k % 50 == 0 ? 2 : 0);
    for (sh::MoveCmd& slot : p.cmds) slot = m;
    CHECK_MSG(sv::campaign_frame(c, &p, 1, error), error);
  }
};

}  // namespace

TEST(a_loaded_game_runs_on_as_the_one_never_saved) {
  Game a;
  for (int k = 0; k < 150; k++) a.frame(k);
  std::vector<uint8_t> file;
  std::string error;
  CHECK_MSG(sv::campaign_save(a.c, file, error, 1234), error);
  CHECK(!file.empty());

  Game b;  // another game, its own progress, loaded over
  for (int k = 0; k < 20; k++) b.frame(k * 7);
  CHECK_MSG(sv::campaign_load(b.c, b.mod, options({}), file, "quick", error), error);
  b.sequence = a.sequence;
  CHECK(sv::campaign_digest(*b.c.server) == sv::campaign_digest(*a.c.server));
  CHECK(b.c.server->sv.time == a.c.server->sv.time);
  CHECK_MSG(b.c.history.back() == "load " + a.c.map.name, b.c.history.back());
  CHECK(b.c.history.size() == a.c.history.size() + 1);
  for (int k = 150; k < 400; k++) {
    a.frame(k);
    b.frame(k);
    if (sv::campaign_digest(*a.c.server) != sv::campaign_digest(*b.c.server)) {
      CHECK_MSG(false, "the loaded game parted at frame " + std::to_string(k));
      break;
    }
  }
  // Saved again from the loaded one: the same bytes as a save of the game never stopped.
  // (The history is the one difference: the loaded game's says it was loaded.)
  b.c.history.pop_back();
  std::vector<uint8_t> again_a, again_b;
  CHECK(sv::campaign_save(a.c, again_a, error, 1));
  CHECK(sv::campaign_save(b.c, again_b, error, 1));
  CHECK(again_a == again_b);
}

TEST(strings_parms_and_settings_come_back) {
  Game a({"skill 3"});
  for (int k = 0; k < 10; k++) a.frame(k);
  sv::QcServer& s = *a.c.server;
  pg::Vm& vm = s.q.vm;
  // A string the engine made and a zone string, held in the player's fields.
  const int32_t made = pg::engine_string(vm, "a string the engine made");
  const pg::LayoutFields& f = s.q.layout->f;
  *pg::entity_field(vm, 1, f.netname) = static_cast<uint32_t>(made);
  a.c.players[0].parms[3] = 42;
  a.c.serverflags = 5;
  s.lightstyles[3] = "abcz";
  std::vector<uint8_t> file;
  std::string error;
  CHECK_MSG(sv::campaign_save(a.c, file, error), error);

  Game b;
  CHECK_MSG(sv::campaign_load(b.c, b.mod, options({}), file, "s", error), error);
  sv::QcServer& t = *b.c.server;
  const int32_t v = static_cast<int32_t>(*pg::entity_field(t.q.vm, 1, f.netname));
  CHECK(std::strcmp(pg::vm_string(t.q.vm, v), "a string the engine made") == 0);
  CHECK(b.c.players[0].parms[3] == 42 && b.c.players[0].active);
  CHECK(b.c.serverflags == 5);
  CHECK(t.lightstyles[3] == "abcz");
  CHECK(t.cvars.find("skill")->number == 3);
  CHECK(t.clients[0].active && t.sv.clients[0].active);
}

TEST(a_save_is_refused_when_the_game_cannot_be_saved) {
  std::vector<uint8_t> file;
  std::string error;
  sv::Campaign none;
  CHECK(!sv::campaign_save(none, file, error));
  CHECK(error.find("no map") != std::string::npos);
  Game dead;
  dead.c.server->sv.edicts[1].health = 0;
  CHECK(!sv::campaign_save(dead.c, file, error));
  CHECK_MSG(error.find("dead") != std::string::npos, error);
  Game coop({"coop 1"});
  CHECK(!sv::campaign_save(coop.c, file, error));
  CHECK_MSG(error.find("coop") != std::string::npos, error);
}

TEST(a_damaged_or_foreign_file_is_refused_on_load) {
  Game a;
  for (int k = 0; k < 5; k++) a.frame(k);
  std::vector<uint8_t> file;
  std::string error;
  CHECK_MSG(sv::campaign_save(a.c, file, error), error);
  Game b;
  std::vector<uint8_t> bad = file;
  bad[bad.size() / 2] ^= 0x40;
  CHECK(!sv::campaign_load(b.c, b.mod, options({}), bad, "bad", error));
  CHECK_MSG(error.find("bad") != std::string::npos, error);
  CHECK(!b.c.server);
  // Another program: the same form, its code changed.
  pg::Progs other = campaign_mod();
  other.globals.back() ^= 1;
  CHECK(!sv::campaign_load(b.c, other, options({}), file, "other", error));
  CHECK_MSG(error.find("another program") != std::string::npos, error);
  // The right one still loads.
  CHECK_MSG(sv::campaign_load(b.c, b.mod, options({}), file, "good", error), error);
  CHECK(sv::campaign_digest(*b.c.server) == sv::campaign_digest(*a.c.server));
}

TEST(the_listen_saves_and_loads_the_game_playing) {
  const pg::Progs stub = campaign_mod();
  const auto stub_map = [] {
    auto m = std::make_unique<cl::LoadedMap>();
    m->name = m->spec.name = "first";
    m->spec.models = sv::arena_models();
    m->spec.entities = sv::kStubEntities;
    return m;
  };
  cl::ListenOptions opt;
  opt.console = {"deathmatch 0"};
  std::string error;
  cl::Listen a;
  CHECK_MSG(cl::listen_campaign(a, stub, stub_map(), opt, error), error);
  sh::MoveCmd walk;
  walk.forward = 200;
  for (int k = 0; k < 10; k++) cl::listen_frame(a, walk);
  std::vector<uint8_t> file;
  CHECK_MSG(cl::listen_save(a, file, error), error);

  cl::Listen b;
  CHECK_MSG(cl::listen_campaign(b, stub, stub_map(), opt, error), error);
  CHECK_MSG(cl::listen_load(b, stub, opt, file, "quick", error, stub_map()), error);
  CHECK(b.map_changed && b.map && b.map->name == a.map->name);
  CHECK(b.sequence == a.sequence);
  CHECK(sv::campaign_digest(*b.server) == sv::campaign_digest(*a.server));
  for (int k = 0; k < 8; k++) {  // (the stub changes map at its 22nd frame: no files to read)
    cl::listen_frame(a, walk);
    cl::listen_frame(b, walk);
  }
  CHECK(sv::campaign_digest(*b.server) == sv::campaign_digest(*a.server));

  // Not at the level's end; a damaged save leaves the game playing.
  a.campaign.end = cl::LevelEnd::intermission;
  CHECK(!cl::listen_save(a, file, error));
  CHECK_MSG(error.find("level is over") != std::string::npos, error);
  file[file.size() / 3] ^= 1;
  const uint64_t before = sv::campaign_digest(*b.server);
  CHECK(!cl::listen_load(b, stub, opt, file, "bad", error, stub_map()));
  CHECK(b.server != nullptr && sv::campaign_digest(*b.server) == before);
}
