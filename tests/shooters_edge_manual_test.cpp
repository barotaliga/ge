// The window's part of the reference manual: every flag `parse` takes is one the manual lists,
// and every game's own layer is described in its own pass (Quake's being QuakeWorld's too).
//
// ge:summary Tests for the window's manual: the flags table, each game's pass
// ge:keywords manual flags kflags known_flag describe_games pass test
// ge:skill console shooter testing
#include <string>
#include <vector>

#include "check.hpp"
#include "console/manual_page.hpp"
#include "shooters_edge/client/args.hpp"
#include "shooters_edge/client/manual.hpp"
#include "shooters_edge/server/rules_manual.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace con = ge::console;

bool parse_words(std::vector<std::string> words, cl::Args& a, std::string& why) {
  words.insert(words.begin(), "se");
  std::vector<char*> argv;
  argv.reserve(words.size());
  for (std::string& w : words) argv.push_back(w.data());
  return cl::parse(static_cast<int>(argv.size()), argv.data(), a, why);
}

}  // namespace

TEST(every_listed_flag_is_taken_as_a_flag_and_no_other) {
  for (const cl::Flag& f : cl::kFlags) {
    for (const std::string_view n : {f.name, f.also}) {
      if (n.empty()) continue;
      cl::Args a;
      std::string why;
      std::vector<std::string> words = {std::string(n)};
      if (n.back() == '=') {
        words[0] += n == "--size=" || n == "--resolution=" ? "720p"
                    : n == "--from=" || n == "--to="       ? "0:01"
                    : n == "--classic="                    ? "q2"
                                                           : "1";
      } else {
        for (char ch : f.arg)
          if (ch == '<') words.emplace_back(n == "-classic" ? "q2" : "x");
      }
      CHECK(parse_words(words, a, why));
      CHECK(a.plus.empty());  // its value taken, nothing left over as a console line
    }
  }
  cl::Args a;
  std::string why;
  CHECK(!parse_words({"-nosuchflag"}, a, why));
  CHECK(cl::known_flag("--classic") && cl::known_flag("-hidden") && !cl::known_flag("-nope"));
  const std::vector<con::FlagDoc> docs = cl::flag_docs();
  CHECK(docs.size() == std::size(cl::kFlags) + 2);  // -config and -logfile first
  CHECK(docs.front().name == "-config");
  CHECK(cl::usage().find("[-manual <file>]") != std::string::npos);
}

TEST(each_game_is_described_in_its_own_pass) {
  con::Console c(false);
  cl::GameCvarValues v;
  cl::GameStart start;
  const shooters_edge::profiles::GameProfile& q1 = shooters_edge::profiles::q1_game();
  cl::add_start_cvars(c, start, q1);
  cl::add_game_cvars(c, v, q1, {});
  const con::ManualSource s = cl::describe_games(c, v, {}, start, q1);
  CHECK(s.programs.size() == 1 && s.programs[0].id == "se");
  CHECK(s.games.size() == 3);  // Quake, QuakeWorld, Quake II
  bool q1_pass = false, q2_pass = false, q2_own = false;
  for (const con::ManualEntry& e : s.entries) {
    q1_pass = q1_pass || e.games == "q1,qw";
    q2_pass = q2_pass || e.games == "q2";
    q2_own = q2_own || (e.games == "q2" && e.layer == "game:q2");
  }
  CHECK(q1_pass && q2_pass && q2_own);
  // The loaded game's layer is back.
  CHECK(v.layer == "game:q1");
  const std::vector<con::ManualRow> rows = con::manual_rows(s);
  bool q2_only = false;
  for (const con::ManualRow& r : rows)
    q2_only = q2_only || (r.games == std::vector<std::string>{"q2"});
  CHECK(q2_only);
}

// The rules' own settings, made with no map, each game's in its own layer: Quake's (the settings
// every Quake program reads) for Quake's and QuakeWorld's pass, Quake II's for its own.
TEST(the_rules_own_settings_are_described_per_game) {
  const auto has = [](const std::vector<ge::console::ManualEntry>& v, const char* name,
                      const char* layer) {
    for (const ge::console::ManualEntry& e : v)
      if (e.name == name && e.layer == layer && !e.help.empty()) return true;
    return false;
  };
  std::vector<ge::console::ManualEntry> quake, q2;
  shooters_edge::server::rules_manual("q1,qw", quake);
  shooters_edge::server::rules_manual("q2", q2);
  for (const char* name : {"deathmatch", "coop", "skill", "fraglimit"}) {
    CHECK_MSG(has(quake, name, "game:q1"), name);
    CHECK_MSG(has(q2, name, "game:q2"), name);
  }
  CHECK(has(q2, "dmflags", "game:q2"));
  CHECK(!has(quake, "dmflags", "game:q1"));
}
