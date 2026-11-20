// games/shooters_edge/client/match_name: the match's kind from its players, teams and settings;
// the default patterns filled for a duel, a 2on2, a 4on4 and a free for all, watching and with a
// nick; the cleaning of names (unfit characters one `_`, folders kept in the pattern, never from a
// name); the free `_NNN` number over several extensions; the settings and their words.
//
// ge:summary Tests the match name: kinds, codes, cleaning, free numbers, settings
// ge:keywords test match-name match_type match_format easyrecord free_match_file
// ge:skill shooter testing
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
#include "shooters_edge/client/match_name.hpp"

namespace cl = shooters_edge::client;
namespace fs = std::filesystem;

namespace {

cl::MatchFacts facts(std::vector<cl::NamedPlayer> players, int teamplay, int me = 0) {
  cl::MatchFacts f;
  f.players = std::move(players);
  f.me = me;
  f.teamplay = teamplay;
  f.map = "dm4";
  f.gamedir = "qw";
  f.year = 2026;
  f.month = 11;
  f.day = 20;
  f.hour = 9;
  f.minute = 5;
  f.second = 7;
  return f;
}

std::string named(const cl::MatchFacts& f, const cl::MatchWords& w = {}) {
  ge::console::Console con{false};
  cl::add_match_cvars(con);
  return cl::match_name(f, w, [&con](const std::string& name) {
    const ge::console::Cvar* v = con.cvars().find(name);
    return v != nullptr ? v->value : std::string();
  });
}

}  // namespace

TEST(the_kind_follows_the_players_and_teams) {
  CHECK(cl::match_type(facts({}, 0)).empty());
  CHECK(cl::match_type(facts({{"a", "", false}}, 0)) == "solo");
  CHECK(cl::match_type(facts({{"a", "", false}, {"b", "", false}}, 0)) == "duel");
  CHECK(cl::match_type(facts({{"a", "red", false}, {"b", "red", false}}, 2)) == "unknown");
  CHECK(cl::match_type(facts({{"a", "", false}, {"b", "", false}, {"c", "", false}}, 0)) == "ffa");
  CHECK(cl::match_type(facts({{"a", "red", false},
                              {"b", "red", false},
                              {"c", "blue", false},
                              {"d", "blue", false},
                              {"w", "", true}},
                             2)) == "2on2");
  std::vector<cl::NamedPlayer> eight;
  eight.reserve(9);
  for (int i = 0; i < 8; i++) eight.push_back({"p" + std::to_string(i), i < 4 ? "red" : "blue"});
  CHECK(cl::match_type(facts(eight, 2)) == "4on4");
  eight.push_back({"x", "green", false});
  CHECK(cl::match_type(facts(eight, 2)) == "multiteam");
  cl::MatchFacts coop = facts({{"a", "", false}, {"b", "", false}}, 0);
  coop.deathmatch = 0;
  CHECK(cl::match_type(coop) == "coop");
  cl::MatchFacts race = facts({{"a", "", false}, {"b", "", false}}, 0);
  race.race = true;
  CHECK(cl::match_type(race) == "race");
  CHECK(cl::match_format_setting("tfduel") == "match_format_tf_duel");
  CHECK(cl::match_format_setting("unknown").empty());
}

TEST(the_default_patterns_name_a_duel_a_2on2_and_a_4on4) {
  const cl::MatchFacts duel = facts({{"me", "", false}, {"you", "", false}}, 0);
  CHECK(named(duel) == "duel/me - me_vs_you - [dmm1] - [dm4]");
  const cl::MatchFacts two = facts(
      {{"me", "red", false}, {"bo", "blue", false}, {"al", "red", false}, {"cy", "blue", false}},
      2);
  CHECK(named(two) == "2on2/me - [me_&_al_vs_bo_&_cy] - [dm4]");
  std::vector<cl::NamedPlayer> eight;
  eight.reserve(9);
  for (int i = 0; i < 8; i++) eight.push_back({"p" + std::to_string(i), i < 4 ? "red" : "blue"});
  CHECK(named(facts(eight, 2)) == "tdm/p0 - [4on4_red_vs_blue] - [dm4]");
  cl::MatchFacts watched = duel;
  watched.players.push_back({"eye", "", true});
  watched.me = 2;
  watched.spectating = true;
  CHECK(named(watched) == "duel/eye(SPEC) - me_vs_you - [dmm1] - [dm4]");
  cl::MatchWords w;
  w.nick = "ace";
  CHECK(named(duel, w) == "duel/ace - me_vs_you - [dmm1] - [dm4]");
  CHECK(named(facts({{"me", "", false}}, 0)) == "solo/me - [dm4]");
  CHECK(cl::expand_match_name("%Y-%m-%d %H%Q%S %G %x %%", duel, w) == "2026-11-20 090507 qw %x %");
}

TEST(names_lose_the_characters_a_file_cannot_hold) {
  CHECK(cl::clean_match_text("a  b", false) == "a_b");
  CHECK(cl::clean_match_text("a/b:c", false) == "a_b_c");
  CHECK(cl::clean_match_text("\\tdm/x y", true) == "tdm/x y");
  const cl::MatchFacts slash = facts({{"a/b", "", false}, {"c", "", false}}, 0);
  CHECK(named(slash) == "duel/a_b - a_b_vs_c - [dmm1] - [dm4]");  // no folder from a name
}

TEST(the_number_is_the_first_free_in_every_extension) {
  const fs::path dir = fs::temp_directory_path() / "ge_match_name_test";
  fs::remove_all(dir);
  fs::create_directories(dir);
  CHECK(cl::free_match_file(dir, "m", {"qwd", "mvd"}) == "m_000");
  std::ofstream(dir / "m_000.qwd") << "x";
  std::ofstream(dir / "m_001.mvd") << "x";
  CHECK(cl::free_match_file(dir, "m", {"qwd", "mvd"}) == "m_002");
  CHECK(cl::free_match_file(dir, "m", {"qwd"}) == "m_001");
  fs::remove_all(dir);
}

TEST(the_settings_are_the_classic_layer_s) {
  ge::console::Console con{false};
  cl::add_match_cvars(con);
  CHECK(con.cvars().find("match_format_duel")->layer == "classic");
  CHECK(con.cvars().find("match_auto_record")->value == "0");
  CHECK(con.cvars().find("match_auto_minlength")->value == "30");
  con.cvars().set("match_name_versus", "-v-", ge::console::Source::local);
  CHECK(cl::match_words(con.cvars()).versus == "-v-");
}
