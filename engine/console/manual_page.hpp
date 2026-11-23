// THE REFERENCE MANUAL'S PAGE: every program's manual source (manual.hpp) merged into ROWS, one a
// setting, command or flag, and written as one page a player searches and filters (`ge manual`):
// a single self-contained HTML file (no fetch, no outside script or font), so it opens from a disk
// and serves as it is from any static host (each release carries it as one of its files).
//
// THE MERGE. A program wrote one pass per game it plays; a row is a name's every pass put together:
//   - the games it is in (none listed: every pass of every program that has it);
//   - its COMMON definition (the layer under the games': its default, range, help and values) and,
//     for each game whose own layer met the name, that game's (`same`: its default and words for
//     the one setting; `own`: its own meaning), so a game picked shows the game's own;
//   - its SPELLINGS: each game's other names for it (a setting synonym, a command synonym), each
//     with the games it is spelled so in;
//   - the programs that have it (the window, the dedicated server).
// A synonym whose target no pass has stays a row of its own.
//
// THE PAGE: a search over names, spellings and help; filters by game (each game's own filter takes
// in the games it names), by kind (settings, commands, flags), by layer and by program; every row
// a link (`#setting-sv_hostname`); the data embedded as JSON (`manual_json`, also written beside it
// as `manual.json` for any other tool).
//
// ge:summary The reference manual's page: every program's source merged into rows, written as HTML
// ge:keywords manual reference-manual user-manual html page search filter per-game merge rows
// ge:keywords manual_rows manual_json manual_html website pages spelling common-definition
// ge:skill console agent-interface
// ge:test tests/console_manual_test.cpp
#pragma once

#include <string>
#include <vector>

#include "console/manual.hpp"

namespace ge::console {

// One game's own definition of a row's name (THE MERGE above).
struct ManualOwn {
  std::vector<std::string> games;
  std::string overlap;  // "same" or "own"; "" a game's own name with no common one under it
  std::string layer, fallback, min, max, help, values;
};

// Another name a row goes by in some games.
struct ManualSpelling {
  std::string name;
  std::vector<std::string> games;
  std::string part;  // a part synonym's word index ("" the whole value)
};

struct ManualRow {
  ManualKind kind = ManualKind::setting;  // setting, command or flag (synonyms fold into these)
  std::string name;
  std::vector<std::string> programs;
  std::vector<std::string> games;  // empty: in every pass of its programs
  std::string layer, type, fallback, min, max, flags, help, values;
  std::string arg;  // a flag's argument
  std::vector<ManualOwn> own;
  std::vector<ManualSpelling> spellings;
};

// What the page says about itself.
struct ManualPage {
  std::string title = "Reference manual";
  std::string intro;    // a line under the title
  std::string version;  // the build's, shown at the foot ("" none)
};

// Every source's entries merged into rows (THE MERGE above), sorted by kind, then name.
std::vector<ManualRow> manual_rows(const ManualSource& source);
// The rows, the programs and the games as one JSON object (the page's data).
std::string manual_json(const ManualSource& source, const std::vector<ManualRow>& rows,
                        const ManualPage& page);
// The whole page.
std::string manual_html(const ManualSource& source, const std::vector<ManualRow>& rows,
                        const ManualPage& page);

}  // namespace ge::console
