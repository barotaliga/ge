// Tests: the arena in C++ (games/shooters_edge/rules/arena*.cpp) plays the matches the compiled
// QC arena played before it was deleted: the frames, the end, the frag lines, the prints and the
// loop's digest every frame, folded into one number per match. The numbers were the QC's, taken
// while the two ran side by side and agreed frame by frame (every path, mover, print, frag line,
// hook call and builtin count too); a change that moves one is a change to the rules. The
// matches: a deathmatch on the baseline arena with its door and plat to the time limit, the same
// in deathmatch 4, and one in a loaded game's words with teams.
//
// ge:summary Tests: the arena's rules in C++ play the QC arena's matches to the bit, pinned
// ge:keywords test arena native-arena parity baseline-mod mod-match deathmatch dmm4 words
// ge:keywords loop-digest pinned
// ge:skill progs shooter testing
#include <cstdint>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/profiles/words.hpp"
#include "shooters_edge/rules/arena.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/mod_match.hpp"

namespace sv = shooters_edge::server;
namespace pf = shooters_edge::profiles;

namespace {

struct Pin {
  int frames;
  size_t frags, prints;
  uint64_t hash;  // FNV-1a over every frame's loop digest, then the frag lines, then the prints
};

uint64_t fold(const sv::ModMatchReport& r) {
  uint64_t h = 1469598103934665603ull;
  const auto mix = [&h](uint64_t w) {
    for (int i = 0; i < 8; i++) {
      h ^= (w >> (8 * i)) & 0xffu;
      h *= 1099511628211ull;
    }
  };
  for (uint64_t d : r.loop_digests) mix(d);
  for (const std::string& t : r.frag_log)
    for (char c : t) mix(static_cast<uint8_t>(c));
  for (const std::string& t : r.prints)
    for (char c : t) mix(static_cast<uint8_t>(c));
  return h;
}

void same_match(const std::vector<std::string>& console, const Pin& qc) {
  sv::ModMatchOptions o;
  o.native = &sv::native_arena();
  o.entities = sv::kBaselineArenaEntities;
  o.bots = 2;
  o.max_frames = 5100;
  o.console = console;
  const sv::ModMatchReport n = sv::run_mod_match(o);
  CHECK_MSG(n.ok, n.error);
  CHECK_MSG(n.faults == 0, n.last_fault);
  CHECK(n.ended);
  CHECK(n.frames == qc.frames);
  CHECK(n.frag_log.size() == qc.frags);
  CHECK(n.prints.size() == qc.prints);
  CHECK_MSG(fold(n) == qc.hash,
            "the match parts from the QC's: frames " + std::to_string(n.frames) + ", frag lines " +
                std::to_string(n.frag_log.size()) + ", prints " + std::to_string(n.prints.size()) +
                "; next: a deliberate change to the rules rewrites the pin");
}

}  // namespace

TEST(the_arena_in_cpp_plays_the_qc_deathmatch) {
  same_match({"deathmatch 1", "timelimit 1"}, {4924, 19, 58, 0xb68f506cd41ba69bull});
}

TEST(the_arena_in_cpp_plays_the_qc_deathmatch_4) {
  same_match({"deathmatch 4", "timelimit 1"}, {4924, 10, 47, 0xa94b0ab5a15ebcf0ull});
}

TEST(the_arena_in_cpp_plays_the_qc_match_in_a_games_words) {
  std::vector<std::string> console = pf::item_name_lines(pf::kClassicNames);
  for (std::string& line : pf::word_lines(pf::kClassicServerWords)) console.push_back(line);
  console.insert(console.end(), {"deathmatch 1", "teamplay 1", "timelimit 1"});
  same_match(console, {4924, 19, 156, 0x5652a81935aee329ull});
}

TEST(the_baseline_mod_is_the_native_arena) {
  ge::progs::Progs p;
  std::string error;
  CHECK_MSG(sv::baseline_mod(p, error), error);
  CHECK(p.natives != nullptr);
  CHECK(p.statements.size() <= 1);  // no bytecode: every function is C++
}
