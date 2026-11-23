// `ge manual <source>...`: the player's reference manual written from the manual sources a game's
// programs wrote (console/manual.hpp: `se -manual <file>`, `sesv --manual=<file>`, the console's
// `writemanual <file>`): every setting, command and launch flag merged into rows
// (console/manual_page.hpp) and written as one searchable page (`--out`, default
// user-manual.html) and its data beside it (manual.json), ready for a static host.
//
// ge:summary ge manual: the reference manual page written from the programs' manual sources
// ge:keywords cli manual reference-manual user-manual html website pages run_manual
// ge:skill agent-interface console
// ge:test tests/cli_manual_test.cpp
#pragma once

#include <string>
#include <vector>

#include "cli/finding.hpp"

namespace ge::cli {

struct ManualOptions {
  std::vector<std::string> sources;
  std::string out = "user-manual.html";
  std::string title = "Reference manual";
  std::string intro;
  std::string version;
};

Outcome run_manual(const ManualOptions& o);

}  // namespace ge::cli
