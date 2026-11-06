// Tests: the compiled-module server (games/shooters_edge/server/qvm_server.hpp) and its adapter
// (games/shooters_edge/sim/qvm_program.hpp) driving the hand-assembled test mod: the map's start
// in the module's order (init's game data block, the entity text read token by token, an entity
// spawned per block), the loop calling the module's think through its export, start frame each
// frame, a client connecting (the connect print, the module's set-up of the player: model, box,
// spot, health) and moving on the loop, a client command the module answers and one it does not,
// the runner's table, a module that refuses to start, and two runs agreeing bit for bit.
//
// ge:summary Tests: a hand-assembled compiled module running a match on the module server
// ge:keywords test qvm server compiled-module game-data-block entity-token think connect
// ge:keywords client-command runner qvm_test_mod determinism
// ge:skill progs shooter testing
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "progs/layout.hpp"
#include "progs/qvm_asm.hpp"
#include "shooter/server.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/qvm_server.hpp"
#include "shooters_edge/server/qvm_test_mod.hpp"

namespace sv = shooters_edge::server;
namespace sim = shooters_edge::sim;
namespace pg = ge::progs;
namespace sh = ge::shooter;

namespace {

sv::MapSpec arena() {
  sv::MapSpec map;
  map.name = "arena";
  map.models = sv::arena_models();
  map.entities = sv::kBaselineArenaEntities;
  return map;
}

int blocks_in(const std::string& text) {
  int n = 0;
  for (char c : text) n += c == '{' ? 1 : 0;
  return n;
}

std::unique_ptr<sv::QvmServer> started(std::string& error) {
  auto s = sv::make_qvm_server(sv::qvm_test_mod(), {}, error);
  if (!s) return nullptr;
  if (!sv::qvm_load_map(*s, arena(), error)) return nullptr;
  return s;
}

int32_t word(sv::QvmServer& s, int32_t addr) { return pg::qvm_word(s.q.vm, addr); }

bool printed(const sv::QvmServer& s, const std::string& text, uint32_t to) {
  for (const sv::NetEvent& e : s.out.events)
    if (e.kind == sv::EventKind::print && e.text == text && (e.to & to) == to) return true;
  return false;
}

}  // namespace

TEST(the_map_starts_in_the_modules_order) {
  std::string error;
  auto s = started(error);
  CHECK_MSG(s != nullptr, error);
  const sim::QvmProgram& q = s->q;
  CHECK(q.data.entity_size == sv::kQvmTestEntitySize);
  CHECK(q.data.max_entities == sv::kQvmTestMaxEntities);
  CHECK(q.data.api == 16);
  CHECK(!q.references);
  CHECK(q.faults == 0);
  const int blocks = blocks_in(sv::kBaselineArenaEntities);
  CHECK(word(*s, sv::kQvmTestBlocks) == blocks);
  // One entity per block after the world's, from the first after the client slots.
  int spawned = 0;
  for (int e = 33; e < s->sv.num_edicts; e++)
    spawned += s->sv.edicts[static_cast<size_t>(e)].free ? 0 : 1;
  CHECK(spawned == blocks - 1);
  CHECK(word(*s, sv::kQvmTestFrames) == 1);  // the start frame before the load
  CHECK(s->models[1] == "maps/arena.bsp");
  CHECK(s->models.back() == "progs/player.mdl");
  CHECK(q.export_counts[sim::kQvmInit] == 1);
  CHECK(q.export_counts[sim::kQvmLoadEntities] == 1);
  CHECK(s->syscall_counts[3] >
        static_cast<int64_t>(blocks) * 2);  // a brace each way per block, and the end
  CHECK(s->syscall_counts[4] == blocks - 1);
  CHECK(s->world.map_name == "arena");
}

TEST(the_loop_runs_the_modules_thinks_and_frames) {
  std::string error;
  auto s = started(error);
  CHECK_MSG(s != nullptr, error);
  constexpr int kFrames = 80;
  for (int f = 0; f < kFrames; f++) sv::qvm_frame(*s, nullptr, 0);
  CHECK(s->q.faults == 0);
  CHECK(word(*s, sv::kQvmTestFrames) > 1);
  CHECK(s->q.export_counts[sim::kQvmStartFrame] == word(*s, sv::kQvmTestFrames));
  // Every thinker thought about once each 0.1 s of the frames' time.
  const double seconds = s->sv.time - s->sv.set.start_time;
  for (int e = 33; e < s->sv.num_edicts; e++) {
    if (s->sv.edicts[static_cast<size_t>(e)].free) continue;
    const float thinks = sim::field_f(s->q, e, pg::kFFrags);
    CHECK_MSG(thinks >= static_cast<float>(seconds / 0.1) - 2 && thinks <= seconds / 0.1 + 1,
              "entity " + std::to_string(e) + " thought " + std::to_string(thinks) + " times in " +
                  std::to_string(seconds) + " s");
  }
}

TEST(a_client_connects_and_the_module_sets_it_up) {
  std::string error;
  auto s = started(error);
  CHECK_MSG(s != nullptr, error);
  sv::qvm_connect(*s, 0, "alice", false, false);
  CHECK(s->q.faults == 0);
  CHECK(printed(*s, "connected\n", 1u));
  const sh::Edict& p = s->sv.edicts[1];
  CHECK(!p.free);
  CHECK(p.origin.x == -416 && p.origin.y == 0 && p.origin.z == 24);
  CHECK(p.movetype == sh::kMovetypeWalk);
  CHECK(p.solid == sh::kSolidSlidebox);
  CHECK(p.maxs.z == 32 && p.mins.x == -16);
  CHECK(p.gravity == 1);
  CHECK(sim::field_f(s->q, 1, pg::kFColormap) == 1);
  CHECK(static_cast<size_t>(sim::field_f(s->q, 1, pg::kFModelindex)) == s->models.size() - 1);
  CHECK(sim::entity_text(s->q, 1, pg::kFModel) == "progs/player.mdl");
  CHECK(s->world.clients[0].active && s->world.clients[0].name == "alice");
  CHECK(s->world.clients[0].numbers.health == 100);
  // The player falls onto the floor under the loop's move as frames run.
  for (int f = 0; f < 40; f++) sv::qvm_frame(*s, nullptr, 0);
  CHECK(s->q.faults == 0);
  sv::qvm_disconnect(*s, 0);
  CHECK(s->sv.edicts[1].free);
  CHECK(!s->world.clients[0].active);
}

TEST(client_commands_go_to_the_module_first) {
  std::string error;
  auto s = started(error);
  CHECK_MSG(s != nullptr, error);
  sv::qvm_connect(*s, 2, "bob", false, false);
  CHECK(sv::qvm_client_command(*s, 2, "hello") == sv::CommandResult::mod);
  CHECK(printed(*s, "hi\n", 1u << 2));
  CHECK(sv::qvm_client_command(*s, 2, "bogus") == sv::CommandResult::bad);
  CHECK(printed(*s, "Bad user command: bogus\n", 1u << 2));
  CHECK(sv::qvm_client_command(*s, 2, "say hello all") == sv::CommandResult::server);
  CHECK(printed(*s, "bob: hello all\n", 1u << 2));
  sv::qvm_set_user_info(*s, 2, "team", "red");
  CHECK(sv::info_value(s->clients[2].userinfo, "team") == "red");
  CHECK(s->q.export_counts[sim::kQvmUserInfoChanged] == 2);
}

TEST(the_console_sets_and_the_module_reads) {
  std::string error;
  auto s = started(error);
  CHECK_MSG(s != nullptr, error);
  CHECK(sv::qvm_console(*s, "fraglimit 25"));
  CHECK(s->cvars.find("fraglimit")->number == 25);
  CHECK(sv::qvm_console(*s, "serverinfo hostname tester"));
  CHECK(sv::info_value(s->serverinfo, "hostname") == "tester");
  CHECK(!sv::qvm_console(*s, "nonsense words"));
  // What a module reads about the server: its name and version as text, the rate cap.
  CHECK(s->cvars.find("version")->value == sv::QvmServerOptions{}.version);
  CHECK(s->cvars.find("sv_maxrate")->number > 0);
  CHECK(sv::qvm_console(*s, "floodprot 4 8 10"));
  CHECK(sv::info_value(s->localinfo, "floodprot") == "4 8 10");
  CHECK(sv::qvm_console(*s, "map dm4"));
  CHECK(s->pending_map == "dm4");
}

TEST(the_runner_table_is_the_modules) {
  std::string error;
  auto s = sv::make_qvm_server(sv::qvm_test_mod(), {}, error);
  CHECK_MSG(s != nullptr, error);
  const sv::RulesRunner r = sv::qvm_runner_over(*s);
  CHECK(std::string(r.calls->form) == "qvm");
  CHECK(sv::qvm_of(r) == s.get());
  CHECK(sv::qc_of(r) == nullptr);
  CHECK_MSG(r.start(arena(), error), error);
  r.connect(0, "carol", false, false);
  r.frame(nullptr, 0);
  CHECK(r->world.clients[0].active);
}

TEST(a_module_with_no_game_data_block_is_refused) {
  pg::QvmAsm m;
  m.bss = 65536;
  m.op(pg::kOpEnter, 16).op(pg::kOpConst, 0).op(pg::kOpLeave, 16);
  m.op(pg::kOpPush).op(pg::kOpLeave, 16);
  std::string error;
  auto s = sv::make_qvm_server(m.file(), {}, error);
  CHECK_MSG(s != nullptr, error);
  CHECK(!sv::qvm_load_map(*s, arena(), error));
  CHECK_MSG(error.find("no game data block") != std::string::npos, error);
  CHECK(error.find("next:") != std::string::npos);
}

TEST(a_file_that_is_not_a_module_is_refused) {
  std::string error;
  const std::vector<uint8_t> junk(64, 7);
  CHECK(sv::make_qvm_server(junk, {}, error) == nullptr);
  CHECK_MSG(error.find("compiled module is refused") != std::string::npos, error);
}

TEST(two_runs_agree_bit_for_bit) {
  uint64_t hashes[2] = {};
  for (uint64_t& h : hashes) {
    std::string error;
    auto s = started(error);
    CHECK_MSG(s != nullptr, error);
    sv::qvm_connect(*s, 0, "alice", false, false);
    for (int f = 0; f < 60; f++) sv::qvm_frame(*s, nullptr, 0);
    h = sh::server_hash(s->sv);
  }
  CHECK(hashes[0] == hashes[1]);
}
