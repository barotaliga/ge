// THE RECORDED BITS' COMMANDS: `ge digest` (a lab's digest against the committed baseline) and
// `ge golden diff` / `ge golden write` (a test's golden tables), so moving a baseline is one
// command for the one thing that moved, and reviewing it is a table of what moved, not a wall of
// hex.
//
//   ge digest [<lab>] [--write]   the lab (or every lab) at its smoke flags, twice; each digest
//                                 beside tests/labs_digests.txt's row. --write rewrites that row
//                                 only (every row with no lab), creating the file if it is not
//                                 there. A repo with a baseline has its labs held to it by
//                                 `ge test` (the labs_smoke case).
//   ge golden diff [<golden|test>...] [--base=<ref>]
//                                 each golden file against git's <ref> (default HEAD): the rows
//                                 that changed and, column by column, what moved in them; with
//                                 no argument, every changed file under tests/golden/
//   ge golden write <test>        the test run once with GE_GOLDEN_WRITE=1 (the convention a
//                                 golden test writes its file under), then again to hold it;
//                                 the answer is the diff of every golden file the test names
//
// A golden file is rows of words, '#' lines its provenance; its last '#' line before the rows,
// when it has one word a column, names the columns. A 16-hex-digit word in a column whose name
// is not a digest's or a hash's is shown as the double its bits are too.
//
// ge:summary ge digest, ge golden diff and ge golden write: a lab's baseline, a golden's moves
// ge:keywords cli ge digest golden diff write baseline labs_digests golden-tables columns moved
// ge:keywords ge_golden_write run_digest run_golden_diff run_golden_write diff_golden
// ge:skill agent-interface
// ge:test tests/cli_golden_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "cli/build.hpp"
#include "cli/outcome.hpp"

namespace ge::cli {

inline constexpr std::string_view kDigestBaseline = "tests/labs_digests.txt";

Outcome run_digest(const BuildOptions& o, const std::string& lab, bool write);
Outcome run_golden_diff(const fs::path& repo, const std::vector<std::string>& targets,
                        const std::string& base);
Outcome run_golden_write(const BuildOptions& o, const std::string& test);

// One golden file's two versions compared, row by row (rows match by their place).
struct GoldenCell {
  std::string column;  // the header's name, or "column N" (from 1)
  std::string before, after;
};
struct GoldenRow {
  size_t row = 0;                 // from 1, among the data rows
  std::string key;                // the row's first word (a seed, a name), before the change
  std::vector<GoldenCell> cells;  // only the words that moved; a word added or gone is "-"
};
struct GoldenDiff {
  std::string file;
  size_t rows_before = 0, rows_after = 0;
  bool header_changed = false;
  std::vector<GoldenRow> rows;  // the rows that changed, in order
  // Each column that moved and the number of rows it moved in, in the columns' order.
  std::vector<std::pair<std::string, int>> moved;
};
GoldenDiff diff_golden(std::string_view file, std::string_view before, std::string_view after);
// The diff as text: a line for the file, one a column that moved, one a row (at most
// `most_rows`, each with every word that moved in it).
std::string format_golden_diff(const GoldenDiff& d, size_t most_rows);
std::string golden_diff_json(const GoldenDiff& d, size_t most_rows);
// The golden files a test's source names (tests/golden/<x>.txt), in the order it names them.
std::vector<std::string> goldens_named_in(std::string_view source);

}  // namespace ge::cli
