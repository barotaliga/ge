// SHOOTER'S EDGE'S SCRIPTED MATCH REPLAYS TO THE BIT: the match (games/shooters_edge/sim/match.hpp:
// two clients over latency and loss, a door, a plat, a rocket, a grenade, prediction) run twice
// agrees on the server's whole-state hash after every frame, and its hashes every 100 frames and
// at the end are tests/golden/shooters_edge_match.txt, which this test writes under
// GE_GOLDEN_WRITE=1 (a deliberate change to the loop or the program rewrites it; the PR says why).
// And what the match is for happened: the door opened, was blocked and closed; the plat carried
// its rider up; a rocket and a grenade flew, the grenade bounced, both exploded; the health box
// was taken; packets were lost and replayed; the clients' predictions stayed near the server.
//
// Golden rows: frame index, the server hash after it (hex words).
//
// ge:summary Tests: the scripted match replays to the bit, golden hashes, and its events happen
// ge:keywords test match replay determinism golden hash door plat rocket grenade prediction
// ge:skill shooter testing
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "shooters_edge/sim/match.hpp"

namespace se = shooters_edge::sim;

namespace {

constexpr const char* kGolden = "tests/golden/shooters_edge_match.txt";

bool env_set(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool set = _dupenv_s(&v, &n, name) == 0 && v != nullptr;
  std::free(v);
  return set;
#else
  return std::getenv(name) != nullptr;
#endif
}

const se::MatchReport& match() {
  static const se::MatchReport rep = se::run_match({});
  return rep;
}

int count(const se::MatchReport& r, se::EventKind k) {
  int n = 0;
  for (const se::GameEvent& e : r.events)
    if (e.kind == k) n++;
  return n;
}

}  // namespace

TEST(the_match_replays_to_the_bit) {
  const se::MatchReport& a = match();
  const se::MatchReport b = se::run_match({});
  CHECK(a.frames.size() == b.frames.size());
  int first = -1;
  for (size_t i = 0; i < a.frames.size() && i < b.frames.size(); i++)
    if (a.frames[i].hash != b.frames[i].hash) {
      first = static_cast<int>(i);
      break;
    }
  CHECK_MSG(first == -1, "two runs of the match differ from frame " + std::to_string(first) +
                             ": the loop or the program reads state from outside the server "
                             "(load the determinism skill)");
}

TEST(the_match_holds_its_golden_hashes) {
  const se::MatchReport& r = match();
  std::vector<std::vector<uint64_t>> got;
  for (size_t i = 0; i < r.frames.size(); i++)
    if (i % 100 == 99 || i + 1 == r.frames.size()) got.push_back({i, r.frames[i].hash});
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# games/shooters_edge/sim/match.hpp's own record: frame, server_hash after it "
           "(tests/shooters_edge_match_test.cpp)\n";
    for (const auto& row : got) {
      char line[64];
      std::snprintf(line, sizeof line, "%llx %016llx\n", static_cast<unsigned long long>(row[0]),
                    static_cast<unsigned long long>(row[1]));
      out << line;
    }
    std::printf("wrote %s (%zu rows)\n", kGolden, got.size());
    return;
  }
  const auto rows = ge::test::golden_rows(kGolden);
  CHECK_MSG(rows.size() == got.size(), std::string(kGolden) + " has the wrong row count");
  for (size_t i = 0; i < rows.size() && i < got.size(); i++)
    CHECK_MSG(rows[i] == got[i], "the match moved off its golden hash at frame " +
                                     std::to_string(got[i][0]) +
                                     ": if the change is meant, rewrite with GE_GOLDEN_WRITE=1 "
                                     "and say why in the PR");
}

TEST(the_match_does_what_it_is_scripted_to) {
  const se::MatchReport& r = match();
  for (const se::GameEvent& e : r.events)
    std::printf("%8.3f %-15s ent %d other %d at %.1f %.1f %.1f amount %.1f\n", e.time,
                se::event_name(e.kind), e.ent, e.other, e.where.x, e.where.y, e.where.z,
                e.amount);
  std::printf("lost %d of %d packets; commands lost and replayed %lld; speed cuts %lld\n",
              r.clients[1].packets_lost, r.clients[1].packets_sent,
              static_cast<long long>(r.counts.commands_lost),
              static_cast<long long>(r.counts.speed_cuts));
  for (int p = 0; p < se::kPlayers; p++)
    std::printf("client %d: %zu corrections, worst %.3f\n", p, r.clients[p].corrections.size(),
                r.clients[p].worst_correction);
  CHECK(count(r, se::EventKind::rocket_fired) >= 2);
  CHECK(count(r, se::EventKind::grenade_thrown) == 1);
  CHECK(count(r, se::EventKind::grenade_bounce) >= 1);
  CHECK(count(r, se::EventKind::explosion) >= 3);
  CHECK(count(r, se::EventKind::health_taken) == 1);
  CHECK(count(r, se::EventKind::door_opening) >= 2);
  CHECK(count(r, se::EventKind::door_blocked) >= 1);
  CHECK(count(r, se::EventKind::door_closed) >= 1);
  CHECK(count(r, se::EventKind::plat_top) >= 1);
  CHECK(r.counts.commands_lost > 0);
  CHECK(r.counts.new_missiles >= 3);
  // The rider went up with the plat: player 2 stood on the ledge (z = 104 + 24) after it.
  bool on_ledge = false;
  for (const se::FrameRecord& f : r.frames)
    if (f.origin[1].z > 127 && f.origin[1].y < -384 && f.onground[1]) on_ledge = true;
  CHECK(on_ledge);
}
