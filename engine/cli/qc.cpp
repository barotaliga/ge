// ge's runner of the qc tool (qc.hpp).
//
// ge:summary ge's runner of the qc tool: the repo's progs target built, then the executable run
// ge:keywords cli ge qc progs tool-runner hand-off build-log
// ge:skill agent-interface progs
// ge:test tests/toolkit_qc_test.cpp
#include "cli/qc.hpp"

namespace ge::cli {

namespace {

fs::path tool_exe(const BuildOptions& o, std::string_view target) {
#ifdef _WIN32
  return o.repo / "build" / o.preset / (std::string(target) + ".exe");
#else
  return o.repo / "build" / o.preset / std::string(target);
#endif
}

std::string quoted(const std::string& s) { return "\"" + s + "\""; }

}  // namespace

toolkit::ToolRun run_tool(const BuildOptions& o, std::string_view name,
                          const std::vector<std::string>& args) {
  toolkit::ToolRun r;
  r.code = -1;
  if (name != "qc") {
    r.findings.push_back({Severity::error, "ge runs no tool called '" + std::string(name) + "'", "",
                          "ge help (the commands ge has)"});
    return r;
  }
  // The qc tool is the progs plugin's `progs` executable.
  constexpr std::string_view kTarget = "progs";
  Outcome built;
  if (!build_targets(o, built, std::string(kTarget))) {
    r.findings = built.findings;
    return r;
  }
  const fs::path exe = tool_exe(o, kTarget);
  std::error_code ec;
  if (!fs::is_regular_file(exe, ec)) {
    r.findings.push_back(
        {Severity::error, "this build made no `progs` executable", rel_path(o.repo, exe),
         "the progs plugin builds it (plugins/progs/tool/main.cpp); skill: progs"});
    return r;
  }
  std::string cmd = quoted(exe.string());
  for (const std::string& a : args) cmd += " " + quoted(a);
  const fs::path log = build_log(o, "ge-qc");
  fs::remove(log, ec);
  const Logged run = run_to_log(cmd, log);
  r.code = run.code;
  r.output = run.output;
  r.log = rel_path(o.repo, log);
  return r;
}

}  // namespace ge::cli
