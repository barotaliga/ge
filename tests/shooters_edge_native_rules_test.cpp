// Tests: rules written in C++ as a native program (progs/native.hpp) on the mod server, through
// the classic server's calls (shooters_edge/rules/calls.hpp). The test mod in C++
// (test_mod_native.cpp) and the assembled test mod play the same match side by side: every
// player's path and frags each frame, every hook call in order, every print and frag line, every
// builtin's count, and the loop's digest every frame.
//
// ge:summary Tests: the test mod in C++ plays the assembled test mod's match to the bit
// ge:keywords test native native-program rules-calls side-by-side test-mod parity mod-match
// ge:skill progs shooter testing
#include <string>

#include "check.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"

namespace sv = shooters_edge::server;

namespace {

sv::ModMatchOptions options(bool native) {
  sv::ModMatchOptions o;
  o.console = {"timelimit 0.25"};
  if (native) o.native = &sv::native_test_mod();
  return o;
}

}  // namespace

TEST(the_test_mod_in_cpp_plays_the_assembled_match) {
  const sv::ModMatchReport a = sv::run_mod_match(options(false));
  const sv::ModMatchReport n = sv::run_mod_match(options(true));
  CHECK_MSG(a.ok, a.error);
  CHECK_MSG(n.ok, n.error);
  CHECK_MSG(n.faults == 0, n.last_fault);
  CHECK(a.frames > 1000 && a.ended && n.ended);
  CHECK(n.frames == a.frames);
  CHECK(n.players.size() == a.players.size());
  for (size_t p = 0; p < a.players.size() && p < n.players.size(); p++) {
    const sv::PlayerPath& x = a.players[p];
    const sv::PlayerPath& y = n.players[p];
    CHECK_MSG(x.frags == y.frags && x.deaths == y.deaths && x.frags_at == y.frags_at, x.name);
    CHECK(x.points.size() == y.points.size());
    for (size_t i = 0; i < x.points.size() && i < y.points.size(); i++) {
      if (x.points[i].x == y.points[i].x && x.points[i].y == y.points[i].y &&
          x.points[i].z == y.points[i].z)
        continue;
      CHECK_MSG(false, x.name + " parts at frame " + std::to_string(i));
      break;
    }
  }
  CHECK(n.missiles.size() == a.missiles.size());
  CHECK(n.prints == a.prints);
  CHECK(n.frag_log == a.frag_log);
  CHECK(n.hook_log.size() == a.hook_log.size());
  for (int h = 0; h < static_cast<int>(sv::HookId::count); h++)
    CHECK(n.hook_counts[h] == a.hook_counts[h]);
  CHECK(n.builtins.size() == a.builtins.size());
  for (size_t i = 0; i < a.builtins.size() && i < n.builtins.size(); i++)
    CHECK_MSG(
        a.builtins[i].number == n.builtins[i].number && a.builtins[i].calls == n.builtins[i].calls,
        a.builtins[i].name);
  // The world as played is the same frame by frame. (The program's memory is not: the assembled
  // one keeps its constants and locals in globals, and its strings sit elsewhere in its table.)
  CHECK(n.loop_digests == a.loop_digests);
  CHECK(n.loop_digests.size() == static_cast<size_t>(a.frames));
  CHECK(a.frag_log.size() > 3);  // the match had kills to compare
}
