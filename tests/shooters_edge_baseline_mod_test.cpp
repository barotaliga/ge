// Tests: the baseline mod (games/shooters_edge/qc/) compiled from its sources by the QC compiler
// and played by four bots on the arena with its door and plat. It compiles with the engine-side
// layout (crc and offsets, so the loader's layout check takes it), spawns every entity of its
// entity text (nothing inhibited, no unknown key or classname), and plays a deathmatch: pickups,
// hits and frags with obituaries by weapon, missiles in the air, the door opening for a player,
// the frag log adding up to the frags, the time limit ending the match; and the same match twice
// is the same to the bit.
//
// ge:summary Tests: the baseline QC mod compiles with the engine's layout and plays a deathmatch
// ge:keywords test baseline-mod compile_baseline_mod mod-match deathmatch bots door plat frags
// ge:keywords obituaries pickups layout-check
// ge:skill progs shooter testing
#include <string>

#include "check.hpp"
#include "progs/file.hpp"
#include "progs/layout.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/mod_match.hpp"

namespace sv = shooters_edge::server;
namespace pg = ge::progs;

namespace {

struct Built {
  bool ok = false;
  std::string error;
  pg::Progs progs;
};

const Built& baseline() {
  static const Built b = [] {
    Built out;
    out.ok = sv::compile_baseline_mod(GE_SOURCE_DIR, out.progs, out.error);
    return out;
  }();
  return b;
}

sv::ModMatchReport play() {
  sv::ModMatchOptions o;
  o.compiled = &baseline().progs;
  o.compiled_name = "the baseline mod";
  o.entities = sv::kBaselineArenaEntities;
  o.bots = 2;
  o.max_frames = 5100;  // the minute's match and the five seconds before the level changes
  o.console = {"deathmatch 1", "timelimit 1"};
  return sv::run_mod_match(o);
}

const sv::ModMatchReport& match() {
  static const sv::ModMatchReport rep = play();
  return rep;
}

int prints_with(const sv::ModMatchReport& r, const std::string& part) {
  int n = 0;
  for (const std::string& p : r.prints) n += p.find(part) != std::string::npos ? 1 : 0;
  return n;
}

const sv::MoverPath* mover(const sv::ModMatchReport& r, const char* classname) {
  for (const sv::MoverPath& m : r.movers)
    if (m.classname == classname) return &m;
  return nullptr;
}

}  // namespace

TEST(the_baseline_mod_compiles_with_the_engines_layout) {
  const Built& b = baseline();
  CHECK_MSG(b.ok, b.error);
  CHECK(b.progs.crc == pg::kClassicCrc);
  // Written and read back with the layout check on: the engine finds its globals and fields.
  const std::vector<uint8_t> bytes = pg::write_progs(b.progs);
  pg::Progs back;
  std::string error;
  pg::LoadOptions lo;
  lo.check_layout = true;
  CHECK_MSG(pg::load_progs(bytes.data(), bytes.size(), back, error, lo), error);
  CHECK(pg::find_function(back, "PutClientInServer") > 0);
  CHECK(pg::find_function(back, "StartFrame") > 0);
}

TEST(the_baseline_arena_spawns_without_complaint) {
  const sv::ModMatchReport& r = match();
  CHECK_MSG(r.ok, r.error);
  CHECK(r.inhibited == 0);
  for (const std::string& m : r.map_messages) CHECK_MSG(false, "map message: " + m);
  CHECK(r.faults == 0);
  CHECK_MSG(r.last_fault.empty(), r.last_fault);
}

TEST(the_baseline_mod_plays_a_deathmatch_to_its_time_limit) {
  const sv::ModMatchReport& r = match();
  CHECK(r.ended);
  CHECK(r.end_time >= 65.0 && r.end_time < 65.1);  // the limit, then 5 s on the scores
  CHECK(prints_with(r, "entered the game") == 2);
  CHECK(prints_with(r, "You got") + prints_with(r, "You receive") >= 4);
  CHECK(prints_with(r, " was shot by ") >= 2);
  CHECK(prints_with(r, "Timelimit hit.") == 1);
  CHECK(!r.missiles.empty());
  // Every death is one obituary and one respawn; the frags are kills less suicides.
  int deaths = 0;
  float frags = 0;
  for (const sv::PlayerPath& p : r.players) {
    deaths += p.deaths;
    frags += p.frags;
  }
  CHECK(deaths >= 4);
  const int kills = prints_with(r, " by ");
  const int own = prints_with(r, " fragged themselves") + prints_with(r, " died") +
                  prints_with(r, " drowned") + prints_with(r, " burned");
  CHECK(kills + own >= deaths);
  CHECK(static_cast<int>(frags) == kills - own);
}

TEST(the_door_opens_for_a_player_and_closes_again) {
  const sv::MoverPath* door = mover(match(), "func_door");
  CHECK(door != nullptr);
  if (door == nullptr) return;
  float top = 0;
  for (float z : door->z) top = z > top ? z : top;
  CHECK_MSG(top == 120, "the door rose to " + std::to_string(top) + ", not its 120");
  CHECK(door->z.front() == 0);
  CHECK(door->z.back() == 0);
  int closings = 0;
  for (size_t i = 1; i < door->z.size(); i++)
    closings += door->z[i] < door->z[i - 1] && door->z[i - 1] == 120;
  CHECK(closings >= 1);
}

// The first player in spawns on the plat: it rides him up its 96 units, waits, and comes down.
TEST(the_plat_carries_a_player_up_and_comes_back_down) {
  const sv::MoverPath* plat = mover(match(), "func_plat");
  CHECK(plat != nullptr);
  if (plat == nullptr) return;
  int rides = 0;
  bool bounds = true;
  for (size_t i = 0; i < plat->z.size(); i++) {
    bounds = bounds && plat->z[i] >= 0 && plat->z[i] <= 96;
    rides += i > 0 && plat->z[i] == 96 && plat->z[i - 1] < 96 ? 1 : 0;
  }
  CHECK(bounds);
  CHECK(rides >= 1);
  CHECK(plat->z.back() == 0);
}

TEST(the_baseline_match_replays_to_the_bit) {
  const sv::ModMatchReport again = play();
  CHECK(again.final_digest == match().final_digest);
  CHECK(again.digests == match().digests);
}
