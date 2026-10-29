// `ge tool`: a plugin's own command-line tool (plugins/<name>/tool/, built by cmake/Plugins.cmake
// into the executable `<name>` beside the labs) built and run with the arguments after `--`.
// With no name, the tools the repo's plugins carry. A tool prints one JSON object on stdout,
// `{"ok": true|false, "error": "...", ...}`, and exits 0, 1 (refused or failed: the error is a
// finding) or 2 (a usage error); the object is the answer's result.
//
// ge:summary ge tool: a plugin's command-line tool built and run, its JSON answer as the outcome
// ge:keywords cli ge tool plugin executable run passthrough json answer list
// ge:skill agent-interface
// ge:test tests/cli_tool_test.cpp
#pragma once

#include <string>
#include <vector>

#include "cli/build.hpp"

namespace ge::cli {

// The plugins under `repo` that carry a tool (plugins/<name>/tool/ with a source), sorted.
std::vector<std::string> plugin_tools(const fs::path& repo);
// The outcome of tool `name` that exited `code` printing `output`.
Outcome tool_answer(const std::string& name, int code, const std::string& output);
Outcome run_tool(const BuildOptions& o, const std::string& name,
                 const std::vector<std::string>& passthrough);

}  // namespace ge::cli
