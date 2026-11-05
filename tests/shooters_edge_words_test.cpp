// Tests: a loaded classic game's server words (server/words.hpp, profiles/words.hpp,
// server/announce.hpp): the template marks (red text, gold digits, brackets, padding, the rule),
// every template fit to travel as a server line, the countdown's centre print to the byte, the
// lobby's lines in the game's words (ready, all ready, the timer, a break that stops it, the match
// begun) and in its own without them, and the baseline mod in a match with the words: every death
// told in them, the clock's minute and seconds, the match over, nothing in the mod's own words.
//
// ge:summary Tests: a classic game's server words: the marks, the countdown, the lobby, the mod
// ge:keywords test words fill_words red-text gold-digits countdown obituary announce clock
// ge:skill shooter testing
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "check.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/profiles/words.hpp"
#include "shooters_edge/server/announce.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/words.hpp"

namespace {

namespace sv = shooters_edge::server;
namespace pf = shooters_edge::profiles;
namespace pg = ge::progs;

std::string bytes(std::initializer_list<int> b) {
  std::string s;
  for (int c : b) s.push_back(static_cast<char>(c));
  return s;
}

sv::Info classic_info() {
  sv::Info info;
  for (const pf::WordRow& w : pf::kClassicServerWords)
    info.emplace_back(std::string(w.key), std::string(w.text));
  return info;
}

// Every print's text, in order.
std::vector<std::string> prints(const sv::Announcer& a, sv::EventKind kind) {
  std::vector<std::string> out;
  for (const sv::NetEvent& ev : a.pending)
    if (ev.kind == kind) out.push_back(ev.text);
  return out;
}

bool has(const std::vector<std::string>& v, const std::string& s) {
  for (const std::string& x : v)
    if (x == s) return true;
  return false;
}

}  // namespace

TEST(the_marks_make_red_text_gold_digits_brackets_and_padding) {
  const std::vector<std::string> args = {"bob", "12", "on"};
  CHECK(sv::fill_words("%1 ^is ready^%n", args) ==
        "bob " + sv::red_text("is") + " " + sv::red_text("ready") + "\n");
  CHECK(sv::red_text("a b") == bytes({'a' + 128, ' ', 'b' + 128}));
  CHECK(sv::fill_words("%g2", args) == bytes({147, 148}));
  CHECK(sv::fill_words("%[%g2%]", args) == bytes({144, 147, 148, 145}));
  CHECK(sv::fill_words("x %>4g2|", args) == "x   " + bytes({147, 148}) + "|");
  CHECK(sv::fill_words("%>4r3", args) == "  " + sv::red_text("on"));
  CHECK(sv::fill_words("100%% %q %9", args) == "100% %q ");  // a mark it does not know stays
  CHECK(sv::fill_words("^^", args) == "^");
  const std::string rule = sv::fill_words("%l", args);
  CHECK(rule.size() == 38 && rule.front() == static_cast<char>(157) &&
        rule[1] == static_cast<char>(158) && rule.back() == static_cast<char>(159));
  CHECK(sv::fill_words("%.", args) == bytes({143}));
}

TEST(every_classic_template_travels_as_a_server_line) {
  std::set<std::string_view> keys;
  for (const pf::WordRow& w : pf::kClassicServerWords) {
    CHECK_MSG(keys.insert(w.key).second, "the key " + std::string(w.key) + " twice");
    const std::string fault = sv::words_fault(w.text);
    CHECK_MSG(fault.empty(), std::string(w.key) + ": " + fault);
  }
  // The profile row names the table, and its lines are `localinfo` lines.
  CHECK(pf::classic_game().words.size() == std::size(pf::kClassicServerWords));
  const std::vector<std::string> lines = pf::word_lines(pf::kClassicServerWords);
  CHECK(lines.front() == "localinfo obit_axe \"%1 was ax-murdered by %2%n\"");
}

TEST(the_countdown_centre_print_is_the_classic_servers_to_the_byte) {
  sv::Lobby l;
  l.settings.countdown_s = 10;
  for (int i = 0; i < 8; i++) sv::lobby_join(l, i, "p" + std::to_string(i), false, 0);
  for (int i = 0; i < 8; i++) sv::lobby_ready(l, i, true, 0);
  sv::Announcer a;
  const sv::MatchLook look{.deathmatch = 1, .teamplay = 2, .timelimit = 20, .antilag = 2};
  sv::announce(a, l, classic_info(), look, 0);
  const std::vector<std::string> centres = prints(a, sv::EventKind::centerprint);
  CHECK(centres.size() == 1);
  const std::string R = sv::red_text("Countdown");
  const std::string expect = R + ": " + bytes({147, 146}) + "\n\n\n" + "Deathmatch  " +
                             bytes({147}) + "\n" + "Mode  " + sv::red_text("T e a m") + "\n" +
                             "Antilag     " + bytes({148}) + "\n" + "Teamplay    " + bytes({148}) +
                             "\n" + "Timelimit  " + bytes({148, 146}) + "\n" + "Powerups   " +
                             sv::red_text("on") + "\n" + "\nno matchtag\n\n\n";
  CHECK(!centres.empty() && centres[0] == expect);
  // Its start: everyone ready, the timer, its sound.
  const std::vector<std::string> lines = prints(a, sv::EventKind::print);
  CHECK(has(lines, "All players ready\n"));
  CHECK(has(lines, "Timer started\n"));
  CHECK(has(prints(a, sv::EventKind::sound), "items/protect2.wav"));
  // One second on: the next number; a single digit padded to two.
  a.pending.clear();
  sv::announce(a, l, classic_info(), look, 1.0);
  const std::vector<std::string> next = prints(a, sv::EventKind::centerprint);
  CHECK(next.size() == 1 && next[0].rfind(R + ":  " + bytes({155}) + "\n", 0) == 0);
  // The same second again shows nothing new.
  a.pending.clear();
  sv::announce(a, l, classic_info(), look, 1.5);
  CHECK(a.pending.empty());
  // The last five seconds tick.
  a.pending.clear();
  sv::announce(a, l, classic_info(), look, 5.5);
  CHECK(has(prints(a, sv::EventKind::sound), "buttons/switch04.wav"));
}

TEST(the_lobby_speaks_the_classic_words_ready_break_and_begun) {
  sv::Lobby l;
  l.settings.countdown_s = 10;
  sv::lobby_join(l, 0, "ann", false, 0);
  sv::lobby_join(l, 1, "bob", false, 0);
  sv::Announcer a;
  const sv::MatchLook look{.deathmatch = 3};
  const sv::Info info = classic_info();
  sv::announce(a, l, info, look, 0);
  CHECK(a.pending.empty());  // joining says nothing: the mod tells of arrivals
  sv::lobby_ready(l, 0, true, 0);
  sv::announce(a, l, info, look, 0);
  const std::string ready =
      "ann " + sv::red_text("is") + " " + sv::red_text("ready") + " - no matchtag set\n";
  CHECK(has(prints(a, sv::EventKind::print), ready));
  a.pending.clear();
  sv::lobby_command(l, 0, "ready", 0);  // already: told alone
  sv::announce(a, l, info, look, 0);
  CHECK(a.pending.size() == 1 && a.pending[0].to == 1u &&
        a.pending[0].text == "Type break to unready yourself\n");
  a.pending.clear();
  sv::lobby_ready(l, 1, true, 0);
  sv::announce(a, l, info, look, 0);
  CHECK(sv::fill_words("%>8r1", {std::vector<std::string>{"D u e l"}}) ==
        " " + sv::red_text("D u e l"));
  CHECK(prints(a, sv::EventKind::centerprint).size() == 1);
  CHECK(prints(a, sv::EventKind::centerprint)[0].find(sv::red_text("D u e l")) !=
        std::string::npos);
  // A break in the countdown stops it, said once as that.
  a.pending.clear();
  sv::lobby_ready(l, 1, false, 2);
  sv::announce(a, l, info, look, 2);
  const std::vector<std::string> said = prints(a, sv::EventKind::print);
  CHECK(said.size() == 1 && said[0] == "bob " + sv::red_text("stops") + " " + sv::red_text("the") +
                                           " " + sv::red_text("countdown") + "\n");
  CHECK(has(prints(a, sv::EventKind::centerprint), ""));  // the count cleared
  // Ready again: the countdown runs out and the match begins.
  a.pending.clear();
  sv::lobby_ready(l, 1, true, 3);
  CHECK(sv::lobby_due(l, 13.0));
  sv::announce(a, l, info, look, 13.0);
  CHECK(has(prints(a, sv::EventKind::print), sv::red_text("The") + " " + sv::red_text("match") +
                                                 " " + sv::red_text("has") + " " +
                                                 sv::red_text("begun!") + "\n"));
  // The phase for the mod.
  sv::Info phase;
  sv::announce_phase(phase, l.phase);
  CHECK(sv::words_of(phase, "ge_phase") == "match");
}

TEST(the_lobby_says_its_own_lines_without_a_games_words) {
  sv::Lobby l;
  sv::lobby_join(l, 0, "ann", false, 0);
  sv::lobby_ready(l, 0, true, 0);
  CHECK(l.notes.size() == l.said.size());
  sv::Announcer a;
  sv::announce(a, l, {}, {}, 0);
  const std::vector<std::string> lines = prints(a, sv::EventKind::print);
  CHECK(lines.size() == 3);
  CHECK(lines[0] == "ann joins team red\n");
  CHECK(lines[1] == "ann is ready (1 of 1)\n");
  CHECK(lines[2].find("the match starts in") != std::string::npos);
  CHECK(prints(a, sv::EventKind::centerprint).empty());  // the panel shows the count
  CHECK(l.said.empty() && l.notes.empty());
}

namespace {

const pg::Progs& baseline() {
  static const pg::Progs p = [] {
    pg::Progs out;
    std::string error;
    CHECK_MSG(sv::compile_baseline_mod(GE_SOURCE_DIR, out, error), error);
    return out;
  }();
  return p;
}

sv::ModMatchReport classic_match() {
  sv::ModMatchOptions o;
  o.compiled = &baseline();
  o.compiled_name = "the baseline mod";
  o.entities = sv::kBaselineArenaEntities;
  o.bots = 2;
  o.max_frames = 10200;  // two minutes at the 13 ms frame, then the scores
  o.console = pf::item_name_lines(pf::kClassicNames);
  for (std::string& line : pf::word_lines(pf::kClassicServerWords)) o.console.push_back(line);
  o.console.insert(o.console.end(), {"deathmatch 1", "timelimit 2"});
  return sv::run_mod_match(o);
}

std::vector<std::string> lines_of(const sv::ModMatchReport& r) {
  std::vector<std::string> lines(1);
  for (const std::string& p : r.prints)
    for (char c : p) {
      if (c == '\n')
        lines.emplace_back();
      else
        lines.back() += c;
    }
  return lines;
}

// Whether `line` is template `t` (one line, without its newline) with any text for %1 .. %4.
bool fits(std::string_view line, std::string t) {
  std::vector<std::string> pieces(1);
  for (size_t i = 0; i < t.size(); i++) {
    if (t[i] == '%' && i + 1 < t.size() && t[i + 1] >= '1' && t[i + 1] <= '4') {
      pieces.emplace_back();
      i++;
    } else if (t[i] == '%' && i + 1 < t.size() && t[i + 1] == 'n') {
      i++;
    } else {
      pieces.back() += t[i];
    }
  }
  size_t at = 0;
  for (size_t k = 0; k < pieces.size(); k++) {
    const size_t f = line.find(pieces[k], at);
    if (f == std::string_view::npos || (k == 0 && f != 0)) return false;
    at = f + pieces[k].size();
  }
  return pieces.back().empty() || at == line.size();
}

}  // namespace

TEST(the_baseline_mod_tells_a_match_in_the_classic_words) {
  const sv::ModMatchReport r = classic_match();
  CHECK_MSG(r.ok, r.error);
  CHECK(r.faults == 0);
  const std::vector<std::string> lines = lines_of(r);
  std::vector<std::string> deaths;
  for (const pf::WordRow& w : pf::kClassicServerWords) {
    const std::string_view k = w.key;
    if (k.rfind("obit_", 0) == 0 || k.rfind("tk_", 0) == 0 || k.rfind("self_", 0) == 0 ||
        k.rfind("world_", 0) == 0)
      deaths.emplace_back(w.text);
  }
  int told = 0;
  for (const std::string& l : lines) {
    CHECK_MSG(l.find(" was shot by ") == std::string::npos, "a death in the mod's words: " + l);
    CHECK_MSG(l.find("fragged themselves") == std::string::npos, "the mod's words: " + l);
    CHECK_MSG(l != "Timelimit hit.", "the mod's end of the match");
    for (const std::string& d : deaths)
      if (fits(l, d)) {
        told++;
        break;
      }
  }
  int deaths_seen = 0;
  for (const sv::PlayerPath& p : r.players) deaths_seen += p.deaths;
  CHECK_MSG(deaths_seen >= 2, "only " + std::to_string(deaths_seen) + " deaths");
  // Every death is told (the report's count may miss the last frame's).
  CHECK_MSG(told >= deaths_seen,
            std::to_string(told) + " deaths told of " + std::to_string(deaths_seen));
  const std::string minute = bytes({144, 147, 145}) + " minute remaining";
  CHECK(has(lines, minute));
  CHECK(has(lines, bytes({144, 147, 146, 145}) + " seconds"));
  CHECK(has(lines, bytes({144, 147, 145}) + " second"));
  CHECK(has(lines, "The match is over"));
  // The armour and weapons by the game's names.
  for (const std::string& l : lines) CHECK_MSG(l != "You got armor", "armour in the mod's words");
}
