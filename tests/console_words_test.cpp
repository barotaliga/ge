// Tests: a setting's words (engine/console/words.hpp): the values grammar, which values fit, and
// the faults a cvar's words can have.
//
// ge:summary Tests: the values grammar, value_fits over patterns and ranges, words_fault
// ge:keywords test values help words_fault value_fits pattern sentence
// ge:skill console testing
#include <string>
#include <vector>

#include "check.hpp"
#include "console/words.hpp"

namespace con = ge::console;

TEST(the_values_grammar_parses_and_its_patterns_match) {
  std::vector<con::ValueEntry> e;
  CHECK(con::parse_values("0: off; 1: on", e) && e.size() == 2 && e[1].meaning == "on");
  CHECK(!con::parse_values("0 off", e));
  CHECK(!con::parse_values("", e));
  CHECK(con::pattern_fits("1..32", "7") && !con::pattern_fits("1..32", "33"));
  CHECK(con::pattern_fits("N", "-2.5") && !con::pattern_fits("N", "x"));
  CHECK(con::pattern_fits("lan", "lan") && !con::pattern_fits("lan", "listed"));
  CHECK(con::pattern_fits("\"\"", "") && !con::pattern_fits("\"\"", "a"));
  CHECK(con::pattern_fits("TEXT", "any words at all"));
  CHECK(con::pattern_fits("N S T", "10 1 1") == false);  // S and T are literal words here
  CHECK(con::pattern_fits("N N N", "10 1 1") && !con::pattern_fits("N N N", "10 1"));
}

TEST(a_value_fits_the_kind_the_range_and_the_values) {
  con::Cvars cvars;
  con::Cvar& slots = cvars.add({.name = "sv_slots",
                                .fallback = "8",
                                .help = "The most players the server seats.",
                                .kind = con::CvarKind::integer,
                                .min = 1,
                                .max = 32,
                                .values = "1..32: that many players"});
  CHECK(con::value_fits(slots, "12") && !con::value_fits(slots, "40"));
  CHECK(!con::value_fits(slots, "many"));
  con::Cvar& vis = cvars.add({.name = "sv_vis",
                              .fallback = "lan",
                              .help = "Who may find the server.",
                              .kind = con::CvarKind::text,
                              .values = "lan: the local network; listed: everyone"});
  CHECK(con::value_fits(vis, "listed") && !con::value_fits(vis, "public"));
  CHECK(con::value_meaning(vis, "lan") == "the local network");
  CHECK(con::words_fault(slots).empty());
  CHECK(con::words_fault(vis).empty());
}

TEST(words_fault_names_what_is_wrong) {
  con::Cvars cvars;
  auto fault = [&](const char* name, const char* help, const char* values, const char* d) {
    return con::words_fault(cvars.add({.name = name,
                                       .fallback = d,
                                       .help = help,
                                       .kind = con::CvarKind::text,
                                       .values = values}));
  };
  CHECK(fault("a", "", "x: y", "").find("no help") != std::string::npos);
  CHECK(fault("b", "the slots.", "x: y", "").find("capital") != std::string::npos);
  CHECK(fault("c", "The slots", "x: y", "").find("full stop") != std::string::npos);
  CHECK(fault("d", "The slots. More here.", "x: y", "").find("one sentence") != std::string::npos);
  CHECK(fault("e", "The slots.", "", "").find("no values") != std::string::npos);
  CHECK(fault("f", "The slots.", "nothing", "").find("do not parse") != std::string::npos);
  CHECK(fault("g", "The slots.", "1: one", "2").find("default") != std::string::npos);
  CHECK(fault("h", "The slots.", "1: one; \"\": none", "").empty());
}
