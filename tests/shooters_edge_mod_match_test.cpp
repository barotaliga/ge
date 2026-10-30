// Tests: the assembled deathmatch test mod played by four bots on the arena under the mod server.
// Its spawn points come from the entity text (the health box not in deathmatch inhibited, the
// light with no spawn function skipped), the players spawn on them, rockets fly and their blasts
// frag, the frag log and the obituaries follow, the time limit changes the level; the run replays
// to the bit and holds its golden digests; a progs read from a path plays the same match as the
// built one; a path that is not there, and the sv_progs cvar, are handled.
//
// ge:summary Tests: the test mod's match with bots: spawning, rockets, frags, the timer, goldens
// ge:keywords test mod-match test-mod bots frags rockets timelimit golden digest progs-path
// ge:keywords sv_progs load_mod
// ge:skill progs shooter testing
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"

namespace sv = shooters_edge::server;

namespace {

constexpr const char* kGolden = "tests/golden/shooters_edge_mod_match.txt";

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

sv::ModMatchOptions options() {
  sv::ModMatchOptions o;
  o.console = {"timelimit 0.25"};
  return o;
}

const sv::ModMatchReport& match() {
  static const sv::ModMatchReport rep = sv::run_mod_match(options());
  return rep;
}

int64_t hooks(const sv::ModMatchReport& r, sv::HookId h) {
  return r.hook_counts[static_cast<int>(h)];
}

int64_t builtin_calls(const sv::ModMatchReport& r, const char* name) {
  for (const sv::BuiltinCount& b : r.builtins)
    if (b.name == name) return b.calls;
  return 0;
}

}  // namespace

TEST(the_test_mod_plays_a_match_to_its_time_limit) {
  const sv::ModMatchReport& r = match();
  CHECK_MSG(r.ok, r.error);
  std::printf("%s: %d frames to %.3f s, ended %d, %zu missiles, %lld statements, faults %lld\n",
              r.source.c_str(), r.frames, r.end_time, r.ended ? 1 : 0, r.missiles.size(),
              static_cast<long long>(r.statements), static_cast<long long>(r.faults));
  if (r.faults > 0) std::printf("last fault: %s\n", r.last_fault.c_str());
  for (const sv::PlayerPath& p : r.players)
    std::printf("  %s frags %.0f deaths %d\n", p.name.c_str(), p.frags, p.deaths);
  for (const sv::BuiltinCount& b : r.builtins)
    std::printf("  #%d %s %lld\n", b.number, b.name.c_str(), static_cast<long long>(b.calls));
  for (const std::string& m : r.map_messages) std::printf("  map: %s\n", m.c_str());
  CHECK(r.faults == 0);
  CHECK(r.ended);
  // 15 s of match at 13 ms frames from the server's start.
  CHECK(r.end_time >= 15.0 && r.end_time < 15.02);
  CHECK(r.inhibited == 1);
  CHECK(r.map_messages.size() >= 2);  // wad is not a field; no spawn function for light
  CHECK(hooks(r, sv::HookId::spawn_function) == 5);  // the world and the four spawn points
  CHECK(hooks(r, sv::HookId::client_connect) == 4);
  CHECK(hooks(r, sv::HookId::set_new_parms) == 4);
  CHECK(hooks(r, sv::HookId::timeofday) == 1);
  CHECK(hooks(r, sv::HookId::start_frame) == r.frames);
  CHECK(hooks(r, sv::HookId::player_post_think) == 4 * static_cast<int64_t>(r.frames));
  CHECK(!r.missiles.empty());
  CHECK(builtin_calls(r, "findradius") > 0);
  CHECK(builtin_calls(r, "multicast") > 0);
  CHECK(builtin_calls(r, "changelevel") == 1);
  CHECK(builtin_calls(r, "checkextension") == 2);
  CHECK(r.multicast_bytes > 0);
}

TEST(rockets_frag_and_the_logs_follow) {
  const sv::ModMatchReport& r = match();
  float frags = 0;
  int deaths = 0;
  for (const sv::PlayerPath& p : r.players) {
    frags += p.frags;
    deaths += p.deaths;
  }
  CHECK(deaths >= 2);
  CHECK(static_cast<int>(r.frag_log.size()) == deaths);
  int obituaries = 0;
  for (const std::string& t : r.prints)
    if (t.find("'s rocket") != std::string::npos || t.find("bored") != std::string::npos)
      obituaries++;
  CHECK(obituaries == deaths);
  bool timelimit = false, entered = false;
  for (const std::string& t : r.prints) {
    if (t == "Timelimit hit.\n") timelimit = true;
    if (t == "bot1 entered the game\n") entered = true;
  }
  CHECK(timelimit);
  CHECK(entered);
  // Every frag is a kill of another (+1) or one's own rocket (-1): the frags add up to the kills
  // less twice the suicides.
  int kills = 0;
  for (const std::string& t : r.prints)
    if (t.find("'s rocket") != std::string::npos) kills++;
  CHECK(static_cast<int>(frags) == kills - (deaths - kills));
}

TEST(players_spawn_on_the_maps_spawn_points) {
  const sv::ModMatchReport& r = match();
  // The first frame's positions: each bot near one of the four spots (they spawn 1 up and fall).
  const float spots[4][2] = {{-416, -256}, {-416, 256}, {416, 256}, {416, -96}};
  for (size_t k = 0; k < r.players.size(); k++) {
    const ge::shooter::Vec3f p = r.players[k].points.front();
    bool near = false;
    for (const auto& s : spots)
      if (std::fabs(p.x - s[0]) < 8 && std::fabs(p.y - s[1]) < 8) near = true;
    CHECK_MSG(near, r.players[k].name + " did not start on a spawn point");
  }
}

TEST(nothing_passes_through_the_dividing_wall) {
  const sv::ModMatchReport& r = match();
  // A step from one room to the other crosses x = 128..144 only in the doorway (|y| < 64); a
  // player's respawn is a jump, not a step, and is left out (a step is under 40 units).
  auto check = [](const std::vector<ge::shooter::Vec3f>& pts, const std::string& who) {
    for (size_t i = 1; i < pts.size(); i++) {
      const ge::shooter::Vec3f a = pts[i - 1], b = pts[i];
      if ((a.x < 136) == (b.x < 136)) continue;
      const float dx = b.x - a.x, dy = b.y - a.y;
      if (dx * dx + dy * dy > 40 * 40) continue;
      const float y = a.y + (136 - a.x) / dx * dy;
      CHECK_MSG(std::fabs(y) < 64, who + " crossed the wall at y " + std::to_string(y));
    }
  };
  for (const sv::PlayerPath& p : r.players) check(p.points, p.name);
  for (const sv::MissilePath& m : r.missiles) check(m.points, "rocket " + std::to_string(m.ent));
}

TEST(the_mod_match_replays_to_the_bit) {
  const sv::ModMatchReport& a = match();
  const sv::ModMatchReport b = sv::run_mod_match(options());
  CHECK(a.digests.size() == b.digests.size());
  int first = -1;
  for (size_t i = 0; i < a.digests.size() && i < b.digests.size(); i++)
    if (a.digests[i] != b.digests[i]) {
      first = static_cast<int>(i);
      break;
    }
  CHECK_MSG(first == -1, "two runs differ from frame " + std::to_string(first) +
                             ": a builtin reads state from outside the server (load the "
                             "determinism skill)");
}

TEST(the_mod_match_holds_its_golden_digests) {
  const sv::ModMatchReport& r = match();
  std::vector<std::vector<uint64_t>> got;
  for (size_t i = 0; i < r.digests.size(); i++)
    if (i % 100 == 99 || i + 1 == r.digests.size()) got.push_back({i, r.digests[i]});
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# games/shooters_edge/server/mod_match.hpp's own record: frame, the digest after it "
           "(tests/shooters_edge_mod_match_test.cpp)\n";
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
    CHECK_MSG(rows[i] == got[i], "the mod match moved off its golden digest at frame " +
                                     std::to_string(got[i][0]) +
                                     ": if the change is meant, rewrite with GE_GOLDEN_WRITE=1 "
                                     "and say why in the PR");
}

TEST(a_progs_from_a_path_plays_the_same_match) {
  std::vector<int32_t> lines;
  const std::vector<uint8_t> bytes = sv::test_mod_bytes(lines);
  const std::string path = "mod_match_test.dat";
  {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
  }
  sv::ModMatchOptions o = options();
  o.progs = path;
  o.max_frames = 300;
  sv::ModMatchOptions built = options();
  built.max_frames = 300;
  const sv::ModMatchReport a = sv::run_mod_match(o);
  const sv::ModMatchReport b = sv::run_mod_match(built);
  CHECK_MSG(a.ok, a.error);
  CHECK(a.source == path);
  CHECK(a.final_digest == b.final_digest);
  // The cvar names it too.
  sv::ModMatchOptions by_cvar = options();
  by_cvar.console.push_back("sv_progs " + path);
  by_cvar.max_frames = 300;
  const sv::ModMatchReport c = sv::run_mod_match(by_cvar);
  CHECK(c.source == path);
  CHECK(c.final_digest == b.final_digest);
  std::remove(path.c_str());
}

TEST(a_missing_progs_says_what_to_do) {
  sv::ModMatchOptions o = options();
  o.progs = "no/such/progs.dat";
  const sv::ModMatchReport r = sv::run_mod_match(o);
  CHECK(!r.ok);
  CHECK(r.error.find("cannot read the progs no/such/progs.dat") != std::string::npos);
  CHECK(r.error.find("next:") != std::string::npos);
}
