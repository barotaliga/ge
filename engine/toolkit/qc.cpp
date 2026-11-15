// The qc group's rows and the reading of the qc tool's answer (qc.hpp).
//
// ge:summary The qc rows' flags and bodies, and the qc tool's answer lines read into an Outcome
// ge:keywords toolkit qc progs qc_answer qc_rows answer-lines finding summary result tool
// ge:skill agent-interface progs
// ge:test tests/toolkit_qc_test.cpp
#include "toolkit/qc.hpp"

#include <string>
#include <vector>

namespace ge::toolkit {

namespace {

constexpr std::string_view kTag = "@qc ";

// A finding line: severity, what, where, next, tab-separated.
Finding finding_of(std::string_view line) {
  std::string parts[4];
  int k = 0;
  for (char c : line) {
    if (c == '\t' && k < 3) {
      k++;
      continue;
    }
    parts[k] += c;
  }
  const Severity s = parts[0] == "error"     ? Severity::error
                     : parts[0] == "warning" ? Severity::warn
                                             : Severity::note;
  return {s, parts[1], parts[2], parts[3]};
}

// The command handed to the tool: its name, the progs or the sources, every flag as typed.
Outcome run_qc(const Call& call, std::string_view command) {
  if (!call.tool) {
    Outcome o;
    o.exit = 2;
    o.findings.push_back({Severity::error, call.program + " runs no qc tool", "",
                          call.program + " help (the commands it has)"});
    return o;
  }
  std::vector<std::string> args{std::string(command), call.words.at(0)};
  for (const auto& [name, value] : call.flags)
    args.push_back("--" + name + (value.empty() ? "" : "=" + value));
  const ToolRun r = call.tool(call.program, "qc", args);
  if (r.code < 0) {
    Outcome o;
    o.findings = r.findings;
    return o;
  }
  return qc_answer(call, r, command);
}

Outcome qc_build(const Call& c) { return run_qc(c, "build"); }
Outcome qc_disasm(const Call& c) { return run_qc(c, "disasm"); }
Outcome qc_run(const Call& c) { return run_qc(c, "run"); }
Outcome qc_trace(const Call& c) { return run_qc(c, "trace"); }
Outcome qc_profile(const Call& c) { return run_qc(c, "profile"); }

constexpr Flag kFunction{"function", "<name>", "the function to run (its name in the progs)"};
constexpr Flag kArgs{"args", "<n,...>", "numbers for its parameters in order, three for a vector"};
constexpr Flag kBuiltins{"builtins", "<sample>",
                         "install the sample program's builtins (a game's come with the game)"};
constexpr Flag kEntities{"entities", "<n>",
                         "entities in use before the call (default 1: the world)"};
constexpr Flag kBuildFlags[] = {
    {"out", "<file>", "where the progs goes (default: the project's output, or <file>.dat)"}};
constexpr Flag kDisasmFlags[] = {{"function", "<name>", "only this function (default: every one)"}};
constexpr Flag kRunFlags[] = {kFunction, kArgs, kBuiltins, kEntities};
constexpr Flag kTraceFlags[] = {kFunction,
                                kArgs,
                                kBuiltins,
                                kEntities,
                                {"statements", "", "every statement too, not only the calls"}};
constexpr Flag kProfileFlags[] = {kFunction,
                                  kArgs,
                                  kBuiltins,
                                  kEntities,
                                  {"repeat", "<n>", "run it n times (default 1)"},
                                  {"top", "<n>", "the most functions to list (default 10)"}};

constexpr Row kRows[] = {
    {"qc build", "<progs.src>", 1, 1, "create",
     "QC sources compiled to a progs: a project file (progs.src: the output, then the sources in "
     "order) or one .qc file; the progs and its .lines written, every mistake a finding at its "
     "file and line",
     kBuildFlags, "qc build mods/mymod/progs.src", "progs", qc_build, "qc"},
    {"qc disasm", "<progs>", 1, 1, "see",
     "a QC program (a progs file) checked and disassembled: its counts, then every function's "
     "statements with operands named by their definitions and the source lines when a .lines "
     "file lies beside it",
     kDisasmFlags, "qc disasm previews/qc/sample.dat --function=fib", "progs", qc_disasm, "qc"},
    {"qc run", "<progs>", 1, 1, "see",
     "one function of a QC program run headless: its return value and the statements it ran, "
     "or its fault with the QC stack trace (function, statement, line)",
     kRunFlags, "qc run previews/qc/sample.dat --function=fib --args=10", "progs", qc_run, "qc"},
    {"qc trace", "<progs>", 1, 1, "see",
     "a QC function run with every call it makes shown with its arguments and return value, "
     "indented by depth, and every builtin it calls",
     kTraceFlags, "qc trace previews/qc/sample.dat --function=fib --args=4", "progs", qc_trace,
     "qc"},
    {"qc profile", "<progs>", 1, 1, "see",
     "a QC function run (repeated) with each function's statements and calls, highest first, "
     "the builtins it hit, and the time it took",
     kProfileFlags,
     "qc profile previews/qc/sample.dat --function=run --args=12 --builtins=sample --repeat=20",
     "progs", qc_profile, "qc"},
};

}  // namespace

Outcome qc_answer(const Call& call, const ToolRun& run, std::string_view command) {
  Outcome out;
  bool answered = false;
  size_t at = 0;
  const std::string& output = run.output;
  while (at < output.size()) {
    size_t end = output.find('\n', at);
    if (end == std::string::npos) end = output.size();
    const std::string_view line(output.data() + at, end - at);
    at = end + 1;
    if (!line.starts_with(kTag)) {
      out.text.append(line).append("\n");
      continue;
    }
    answered = true;
    const std::string_view rest = line.substr(kTag.size());
    if (rest.starts_with("finding ")) {
      out.findings.push_back(finding_of(rest.substr(8)));
    } else if (rest.starts_with("summary ")) {
      out.summary = std::string(rest.substr(8));
    } else if (rest.starts_with("result ")) {
      out.result = std::string(rest.substr(7));
    }
  }
  if (!answered) {
    out.findings.push_back(
        {Severity::error, "the qc tool gave no answer (exit " + std::to_string(run.code) + ")",
         run.log, "read its output; the qc tool is the progs plugin's (skill: progs)"});
    return out;
  }
  if (run.code == 2) out.exit = 2;
  if (run.code == 0)
    out.next = command == "build"
                   ? call.program +
                         " qc disasm <the progs written> (its functions); a game runs it as "
                         "sv_progs"
                   : call.program + " qc trace <progs> --function=<name> (every call it makes)";
  return out;
}

std::span<const Row> qc_rows() { return kRows; }

}  // namespace ge::toolkit
