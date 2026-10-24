// ge cvar and ge cfg's answers read back: a listing as text, a check's problems as findings at
// file:line, an exec's changes, a dump written to a file.
//
// ge:summary Tests ge cvar and ge cfg's answers: listings, findings at file:line, the dump
// ge:keywords test cli cvar cfg console_answer findings
// ge:skill console agent-interface testing
#include <filesystem>
#include <fstream>

#include "check.hpp"
#include "cli/cvar.hpp"

using namespace ge::cli;

namespace {

BuildOptions repo() {
  BuildOptions o;
  o.repo = std::filesystem::temp_directory_path() / "ge_cli_cvar_test";
  std::filesystem::create_directories(o.repo);
  return o;
}

ge::play::FlatJson flat(const std::string& line) {
  ge::play::FlatJson m;
  std::string why;
  ge::play::read_flat(line, m, why, true);
  return m;
}

}  // namespace

TEST(a_listing_is_one_line_a_cvar) {
  const std::string line =
      R"({"game":"g","count":1,"cvars":[{"name":"r_glow","value":"1","default":"0.5","kind":"number","min":"0","max":"1","flags":"A.......","help":"the glow"}]})";
  const Outcome o = console_answer(repo(), {.op = "ls"}, "cvar", flat(line), line);
  CHECK(o.text == "  r_glow \"1\" (default \"0.5\")  A....... [0..1]  the glow\n");
  CHECK(o.summary.find("1 cvars of g") == 0);
}

TEST(a_checks_problems_are_findings_at_file_and_line) {
  const std::string line =
      R"({"game":"g","errors":1,"problems":[{"file":"a.cfg","line":2,"severity":"error","what":"unknown command 'r_glo'","next":"did you mean 'r_glow'?"},{"file":"a.cfg","line":3,"severity":"warning","what":"'x' is set but nothing in this game reads it","next":""}]})";
  const Outcome o = console_answer(repo(), {.op = "check"}, "cfg", flat(line), line);
  CHECK(o.findings.size() == 2);
  CHECK(o.findings[0].severity == Severity::error && o.findings[0].where == "a.cfg:2");
  CHECK(o.findings[0].next == "did you mean 'r_glow'?");
  CHECK(o.findings[1].severity == Severity::warn && !o.findings[1].next.empty());
}

TEST(a_dump_is_written_where_asked) {
  const std::string line = R"({"game":"g","text":"seta r_glow \"0.5\"\n"})";
  const BuildOptions o = repo();
  const Outcome out = console_answer(o, {.op = "dump", .out = "my.cfg"}, "cfg", flat(line), line);
  CHECK(out.written.size() == 1 && out.written[0] == "my.cfg");
  std::ifstream in(o.repo / "my.cfg");
  std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  CHECK(text == "seta r_glow \"0.5\"\n");
}
