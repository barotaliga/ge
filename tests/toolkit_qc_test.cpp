// Tests: the qc group reads the qc tool's answer: its text, its findings (severity, what, where,
// next), its summary and its JSON result; no answer is an error naming the log, a usage error
// keeps exit 2; a build points at disasm next, spelled with the calling program's name. A front
// door with no tool refuses (ge's catalog rows: tests/cli_commands_test.cpp). The tool
// itself is held by tests/progs_tool_test.cpp; secli running it by tests/gamecli_cli_test.cpp.
//
// ge:summary Tests: the qc group's reading of the tool's answer, and its refusals
// ge:keywords test toolkit qc progs answer findings summary result catalog tool-runner
// ge:skill agent-interface progs testing
// ge:test tests/toolkit_qc_test.cpp
#include <string>

#include "check.hpp"
#include "toolkit/qc.hpp"

using namespace ge::toolkit;

namespace {

ToolRun ran(int code, std::string output) {
  ToolRun r;
  r.code = code;
  r.output = std::move(output);
  r.log = "build/debug/logs/ge-qc.log";
  return r;
}

Call call_from(std::string program) {
  Call c;
  c.program = std::move(program);
  c.words = {"a.dat"};
  return c;
}

}  // namespace

TEST(the_answer_lines_become_the_outcome) {
  const std::string output =
      "progs version 6, crc 54730: 12 statements\n"
      "function 1 f (a.qc)\n"
      "@qc finding error\tQC fault: NULL function\ta.dat: f (a.qc) statement 3, line 9\trun `ge "
      "qc trace`\n"
      "@qc finding warning\t2 of the --args went unused\ta.dat\tpass fewer\n"
      "@qc summary f faulted after 3 statements\n"
      "@qc result {\"function\":\"f\",\"ok\":false}\n";
  const Outcome out = qc_answer(call_from("ge"), ran(1, output));
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
  const Outcome none = qc_answer(call_from("ge"), ran(139, "Segmentation fault\n"));
  CHECK(none.findings.size() == 1 && none.findings[0].severity == Severity::error);
  CHECK(none.findings[0].what.find("no answer (exit 139)") != std::string::npos);
  CHECK(none.findings[0].where == "build/debug/logs/ge-qc.log");
  const Outcome usage =
      qc_answer(call_from("ge"), ran(2, "@qc finding error\tunknown flag --x\t\trun `ge help`\n"));
  CHECK(exit_of(usage) == 2);
  const Outcome fine = qc_answer(call_from("ge"), ran(0, "@qc summary ok\n"));
  CHECK(exit_of(fine) == 0 && !fine.next.empty());
}

TEST(a_build_points_at_disasm_in_the_callers_name) {
  const Outcome ge = qc_answer(call_from("ge"), ran(0, "@qc summary a.dat: 120 bytes\n"), "build");
  CHECK(exit_of(ge) == 0);
  CHECK(ge.next.rfind("ge qc disasm", 0) == 0);
  const Outcome secli =
      qc_answer(call_from("secli"), ran(0, "@qc summary a.dat: 120 bytes\n"), "build");
  CHECK(secli.next.rfind("secli qc disasm", 0) == 0);
}

TEST(the_rows_hand_the_tool_the_command_words_and_flags) {
  const Row* row = find_row("qc run");
  CHECK(row != nullptr && row->tool == "qc");
  if (row == nullptr) return;
  Call c = call_from("secli");
  c.flags = {{"function", "fib"}, {"statements", ""}};
  std::vector<std::string> seen;
  c.tool = [&](std::string_view program, std::string_view tool,
               const std::vector<std::string>& args) {
    CHECK(program == "secli");
    CHECK(tool == "qc");
    seen = args;
    return ran(0, "@qc summary fib returned 55\n");
  };
  const Outcome o = run_row(*row, c);
  CHECK(o.summary == "fib returned 55");
  CHECK((seen == std::vector<std::string>{"run", "a.dat", "--function=fib", "--statements"}));
}

TEST(a_front_door_with_no_tool_refuses_and_a_tool_that_cannot_run_says_why) {
  const Row* row = find_row("qc disasm");
  CHECK(row != nullptr);
  if (row == nullptr) return;
  const Outcome none = run_row(*row, call_from("fall_line_cli"));
  CHECK(exit_of(none) == 2);
  CHECK(none.findings.at(0).what == "fall_line_cli runs no qc tool");
  Call c = call_from("ge");
  c.tool = [](std::string_view, std::string_view, const std::vector<std::string>&) {
    ToolRun r;
    r.code = -1;
    r.findings.push_back({Severity::error, "this build made no `progs` executable", "", "build"});
    return r;
  };
  const Outcome failed = run_row(*row, c);
  CHECK(exit_of(failed) == 1);
  CHECK(failed.findings.at(0).what == "this build made no `progs` executable");
}
