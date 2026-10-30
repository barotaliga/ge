// Tests: `ge qc` reads the qc executable's answer: its text, its findings (severity, what, where,
// next), its summary and its JSON result; no answer is an error naming the log, a usage error
// keeps exit 2; a build points at disasm next. The catalog holds every qc command with its flags.
// The executable itself is held by tests/progs_tool_test.cpp.
//
// ge:summary Tests: ge qc's reading of the qc executable's answer and its catalog rows
// ge:keywords test cli ge qc progs answer findings summary result catalog
// ge:skill agent-interface progs testing
// ge:test tests/cli_qc_test.cpp
#include <string>

#include "check.hpp"
#include "cli/commands.hpp"
#include "cli/qc.hpp"

using namespace ge::cli;

TEST(the_answer_lines_become_the_outcome) {
  BuildOptions o;
  o.repo = "/repo";
  const std::string output =
      "progs version 6, crc 54730: 12 statements\n"
      "function 1 f (a.qc)\n"
      "@qc finding error\tQC fault: NULL function\ta.dat: f (a.qc) statement 3, line 9\trun `ge "
      "qc trace`\n"
      "@qc finding warning\t2 of the --args went unused\ta.dat\tpass fewer\n"
      "@qc summary f faulted after 3 statements\n"
      "@qc result {\"function\":\"f\",\"ok\":false}\n";
  const Outcome out = qc_answer(o, 1, output);
  CHECK(out.text == "progs version 6, crc 54730: 12 statements\nfunction 1 f (a.qc)\n");
  CHECK(out.findings.size() == 2);
  CHECK(out.findings[0].severity == Severity::error);
  CHECK(out.findings[0].what == "QC fault: NULL function");
  CHECK(out.findings[0].where == "a.dat: f (a.qc) statement 3, line 9");
  CHECK(out.findings[0].next == "run `ge qc trace`");
  CHECK(out.findings[1].severity == Severity::warn);
  CHECK(out.summary == "f faulted after 3 statements");
  CHECK(out.result == "{\"function\":\"f\",\"ok\":false}");
  CHECK(exit_of(out) == 1);
}

TEST(no_answer_is_an_error_and_a_usage_error_exits_two) {
  BuildOptions o;
  o.repo = "/repo";
  const Outcome none = qc_answer(o, 139, "Segmentation fault\n");
  CHECK(none.findings.size() == 1 && none.findings[0].severity == Severity::error);
  CHECK(none.findings[0].what.find("no answer (exit 139)") != std::string::npos);
  const Outcome usage = qc_answer(o, 2, "@qc finding error\tunknown flag --x\t\trun `ge help`\n");
  CHECK(exit_of(usage) == 2);
  const Outcome fine = qc_answer(o, 0, "@qc summary ok\n");
  CHECK(exit_of(fine) == 0 && !fine.next.empty());
}

TEST(every_qc_command_is_in_the_catalog) {
  for (const char* name : {"qc disasm", "qc run", "qc trace", "qc profile"}) {
    const CommandSpec* c = find_command(name);
    CHECK_MSG(c != nullptr, std::string(name) + " is not in the catalog");
    if (c == nullptr) continue;
    CHECK(c->skill == "progs");
    bool function = false;
    for (const FlagSpec& f : c->flags) function = function || f.name == "function";
    CHECK(function);
  }
}

TEST(qc_build_is_in_the_catalog_and_points_at_disasm) {
  const CommandSpec* c = find_command("qc build");
  CHECK(c != nullptr);
  if (c != nullptr) {
    CHECK(c->skill == "progs");
    bool out = false;
    for (const FlagSpec& f : c->flags) out = out || f.name == "out";
    CHECK(out);
  }
  BuildOptions o;
  o.repo = "/repo";
  const Outcome built = qc_answer(o, 0, "@qc summary a.dat: 120 bytes\n", "build");
  CHECK(exit_of(built) == 0);
  CHECK(built.next.find("ge qc disasm") != std::string::npos);
}
