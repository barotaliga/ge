// The qc tool run in a game's player CLI's process (commands.hpp).
//
// ge:summary The qc tool run in-process for a game's player CLI: argv built, the output captured
// ge:keywords progs qc gamecli tool-runner in-process qc_main
// ge:skill progs agent-interface
// ge:test tests/gamecli_cli_test.cpp
#include "progs/cli/commands.hpp"

#include <sstream>

#include "progs/tool.hpp"

namespace ge::progs::cli {

std::vector<std::string> tools() { return {"qc"}; }

toolkit::ToolRun run_tool(std::string_view program, std::string_view name,
                          const std::vector<std::string>& args) {
  toolkit::ToolRun r;
  if (name != "qc") {
    r.code = -1;
    r.findings.push_back({toolkit::Severity::error,
                          "no tool called '" + std::string(name) + "' is linked", "",
                          std::string(program) + " help (the commands it has)"});
    return r;
  }
  const std::string name0(program);
  std::vector<const char*> argv{name0.c_str()};
  for (const std::string& a : args) argv.push_back(a.c_str());
  std::ostringstream out;
  r.code = qc_main(static_cast<int>(argv.size()), argv.data(), out);
  r.output = out.str();
  return r;
}

}  // namespace ge::progs::cli
