// THE CLIENT'S WORLD (games/shooters_edge/net/classic_mirror.hpp): a fighter fed only what its own
// classic client was sent plays a team game as one fed our server's world does. On the fixture
// halls for a minute with `client_fed`: both teams score, no player is fragged by a teammate, every
// fighter played on its mirror with its own place predicted, that place within a few units of the
// server's on average, and the mod never faults. The classic items' boxes are the ones the mirror
// gives an item off its model's name.
//
// ge:summary Tests: fighters fed only their clients' packets play a 4on4; the mirror's own place
// ge:keywords test client-mirror client_fed only-what-is-sent item_box prediction team-game
// ge:skill bots net testing
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "check.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_mirror.hpp"
#include "shooters_edge/net/classic_team.hpp"

namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;

namespace {

// The frag log's line: \frag\<killer>\<killed>\<killer's team>\<killed's team>\...
bool team_kill(const std::string& line) {
  std::vector<std::string> f;
  size_t at = 0;
  while (at <= line.size()) {
    const size_t next = line.find('\\', at);
    f.push_back(line.substr(at, next == std::string::npos ? std::string::npos : next - at));
    if (next == std::string::npos) break;
    at = next + 1;
  }
  return f.size() > 5 && f[2] != f[3] && !f[4].empty() && f[4] == f[5];
}

}  // namespace

TEST(the_classic_items_stand_in_their_boxes) {
  ge::shooter::Vec3f lo, hi;
  cn::item_box("maps/b_bh25.bsp", lo, hi);
  CHECK(lo.x == 0 && lo.z == 0 && hi.x == 32 && hi.z == 56);
  cn::item_box("progs/quaddama.mdl", lo, hi);
  CHECK(lo.x == -16 && lo.z == -24 && hi.z == 32);
  cn::item_box("progs/g_rock2.mdl", lo, hi);
  CHECK(lo.x == -16 && lo.z == 0 && hi.x == 16 && hi.z == 56);
}

TEST(fighters_fed_only_their_clients_play_a_team_game) {
  std::vector<uint8_t> bsp;
  ge::progs::Progs progs;
  std::string error;
  CHECK(ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bsp, &error));
  CHECK(cl::baseline_progs(progs, error));
  cn::TeamMatchOptions o;
  o.map = bsp;
  o.map_name = "halls";
  o.entities = cn::kTeamHallsEntities;
  o.progs = &progs;
  o.frames = 4615;  // a minute
  o.seed = 1;
  o.record = false;
  o.client_fed = true;
  const cn::TeamMatchReport r = cn::run_team_match(o);
  if (!r.ok) std::printf("client-fed match: %s\n", r.error.c_str());
  CHECK(r.ok);
  CHECK(r.faults == 0);
  std::printf("client-fed: frags %.0f to %.0f, deaths %d and %d\n", r.teams[0].frags,
              r.teams[1].frags, r.teams[0].deaths, r.teams[1].deaths);
  CHECK(r.teams[0].frags > 0);
  CHECK(r.teams[1].frags > 0);
  for (const std::string& line : r.frag_log) CHECK(!team_kill(line));
  // Every team finds the items its clients are sent; the halls hold four (one launcher, one
  // armour, the quad, a health) for eight players, so in a minute one player of a team may spend
  // it all fighting over the launcher and take nothing.
  int64_t team_pickups[2] = {}, none[2] = {};
  for (const cn::TeamPlayer& p : r.players) {
    team_pickups[p.team] += p.pickups;
    none[p.team] += p.pickups == 0;
  }
  for (int t = 0; t < 2; t++) {
    CHECK_MSG(team_pickups[t] >= 4, "team " + std::to_string(t) + " picked up " +
                                        std::to_string(team_pickups[t]) + " items in a minute");
    CHECK_MSG(none[t] <= 1, "team " + std::to_string(t) + " had " + std::to_string(none[t]) +
                                " players who picked up nothing");
  }
  for (const cn::TeamPlayer& p : r.players) {
    const double err = p.mirror_err_n > 0 ? p.mirror_err / static_cast<double>(p.mirror_err_n) : -1;
    std::printf(
        "  %-8s frags %3.0f deaths %2d pickups %3lld mirrored %6lld predicted %6lld own place off "
        "%.2f u\n",
        p.name.c_str(), p.frags, p.deaths, static_cast<long long>(p.pickups),
        static_cast<long long>(p.mirrored), static_cast<long long>(p.predicted), err);
    CHECK(p.mirrored > 1000);
    CHECK(p.predicted * 10 > p.mirrored * 9);
    CHECK(p.mirror_err_n > 0);
    CHECK(err >= 0 && err < 8);
  }
}
