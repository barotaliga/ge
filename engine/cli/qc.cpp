// ge's runner of the toolkit rows' tools, qc and brush (qc.hpp).
//
// ge:summary ge's runner of the toolkit's tools (qc, brush): the plugin's target built, then run
// ge:keywords cli ge qc progs brush map tool-runner hand-off build-log
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
  // The qc tool is the progs plugin's `progs` executable; the brush tool, the brush plugin's.
  std::string_view target;
  if (name == "qc") target = "progs";
  if (name == "brush") target = "brush";
  if (target.empty()) {
    r.findings.push_back({Severity::error, "ge runs no tool called '" + std::string(name) + "'", "",
                          "ge help (the commands ge has)"});
    return r;
  }
  Outcome built;
  if (!build_targets(o, built, std::string(target))) {
    r.findings = built.findings;
    return r;
  }
  const fs::path exe = tool_exe(o, target);
  std::error_code ec;
  if (!fs::is_regular_file(exe, ec)) {
    r.findings.push_back({Severity::error,
                          "this build made no `" + std::string(target) + "` executable",
                          rel_path(o.repo, exe),
                          "the " + std::string(target) + " plugin builds it (plugins/" +
                              std::string(target) + "/tool/main.cpp)"});
    return r;
  }
  std::string cmd = quoted(exe.string());
  for (const std::string& a : args) cmd += " " + quoted(a);
  const fs::path log = build_log(o, "ge-" + std::string(name));
  fs::remove(log, ec);
  const Logged run = run_to_log(cmd, log);
  r.code = run.code;
  r.output = run.output;
  r.log = rel_path(o.repo, log);
  return r;
}

}  // namespace ge::cli
