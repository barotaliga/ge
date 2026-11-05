// Tests: the rules runner (games/shooters_edge/server/rules_runner.hpp). A host that runs the QC
// server through the runner's table gets the same loop, bit for bit, as one that calls the QC
// server itself; the runner owns its server or runs over one the caller keeps; a moved runner
// still runs; and the QC server behind a runner is found, while an empty runner has none.
//
// ge:summary Tests: the rules runner's table over the QC server, owned and not, moved and empty
// ge:keywords test rules-runner qc-runner runner-calls rules-common host server-hash
// ge:skill progs shooter testing
#include <memory>
#include <string>
#include <utility>

#include "check.hpp"
#include "shooter/server.hpp"
#include "shooters_edge/server/commands.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "shooters_edge/server/rules_runner.hpp"
#include "shooters_edge/server/test_mod.hpp"

namespace sv = shooters_edge::server;
namespace pg = ge::progs;
namespace sh = ge::shooter;

namespace {

std::unique_ptr<sv::QcServer> make(const pg::Progs& progs) {
  std::string error;
  sv::QcServerOptions o;
  o.loop.max_edicts = 1024;
  std::unique_ptr<sv::QcServer> s = sv::make_qc_server(progs, o, error);
  CHECK_MSG(s != nullptr, error);
  return s;
}

sv::MapSpec arena() {
  sv::MapSpec map;
  map.models = sv::arena_models();
  map.entities = sv::kArenaEntities;
  return map;
}

sh::Packet walk(int slot, int frame) {
  sh::Packet p;
  p.client = slot;
  p.sequence = static_cast<uint32_t>(frame + 1);
  for (sh::MoveCmd& c : p.cmds) {
    c.msec = 13;
    c.forward = 400;
    c.angles.y = static_cast<float>(frame);
  }
  return p;
}

TEST(a_runner_plays_the_qc_server_bit_for_bit) {
  pg::Progs progs;
  std::string error;
  CHECK_MSG(sv::build_test_mod(progs, error), error);
  std::unique_ptr<sv::QcServer> direct = make(progs);
  std::unique_ptr<sv::QcServer> made = make(progs);
  if (!direct || !made) return;
  const sv::RulesRunner r = sv::qc_runner(std::move(made));
  CHECK(r && std::string(r.calls->form) == "qc");
  CHECK(sv::qc_of(r) != nullptr);

  CHECK(sv::server_command(*direct, "deathmatch 1"));
  CHECK(r.console("deathmatch 1"));
  CHECK_MSG(sv::load_map(*direct, arena(), error), error);
  CHECK_MSG(r.start(arena(), error), error);
  sv::connect(*direct, 0, "runner", false, true);
  r.connect(0, "runner", false, true);
  sv::set_user_info(*direct, 0, "team", "red");
  r.user_info(0, "team", "red");
  for (int i = 0; i < 60; i++) {
    const sh::Packet p = walk(0, i);
    sv::qc_frame(*direct, &p, 1);
    r.frame(&p, 1);
    CHECK(sh::server_hash(direct->sv) == sh::server_hash(r->sv));
  }
  CHECK(sv::client_command(*direct, 0, "kill") == r.client_command(0, "kill"));
  CHECK(sh::server_hash(direct->sv) == sh::server_hash(r->sv));
  CHECK(r->world.clients[0].name == "runner");
  sv::disconnect(*direct, 0);
  r.drop(0);
  CHECK(!r->world.clients[0].active);
  CHECK(sh::server_hash(direct->sv) == sh::server_hash(r->sv));
}

TEST(a_runner_over_a_server_leaves_it_to_its_owner) {
  pg::Progs progs;
  std::string error;
  CHECK_MSG(sv::build_test_mod(progs, error), error);
  std::unique_ptr<sv::QcServer> s = make(progs);
  if (!s) return;
  {
    const sv::RulesRunner r = sv::qc_runner_over(*s);
    CHECK(sv::qc_of(r) == s.get() && r.common == s.get());
    CHECK_MSG(r.start(arena(), error), error);
  }  // the runner goes; the server stays
  CHECK(!s->models.empty());
  sv::connect(*s, 0, "still", false, true);
  CHECK(s->world.clients[0].active);
}

TEST(a_moved_runner_runs_and_an_empty_one_has_no_program) {
  pg::Progs progs;
  std::string error;
  CHECK_MSG(sv::build_test_mod(progs, error), error);
  sv::RulesRunner a = sv::qc_runner(make(progs));
  sv::RulesRunner b = std::move(a);
  CHECK(b && sv::qc_of(b) != nullptr);
  CHECK_MSG(b.start(arena(), error), error);
  b.connect(0, "moved", false, true);
  const sh::Packet p = walk(0, 0);
  b.frame(&p, 1);
  CHECK(b->world.clients[0].name == "moved");
  b = {};
  CHECK(!b && sv::qc_of(b) == nullptr);
}

}  // namespace
