// THE RECORDED BITS' COMMANDS: `ge digest` (a lab's digest against the committed baseline) and
// `ge golden diff` / `ge golden write` (a test's golden tables), so moving a baseline is one
// command for the one thing that moved, and reviewing it is a table of what moved, not a wall of
// hex.
//
//   ge digest [<lab>...] [--write] the labs named (or every lab) at their smoke flags, twice; each
//                                 digest beside tests/labs_digests.txt's row. --write rewrites
//                                 those rows only (every row with no lab), creating the file if
//                                 it is not there. --affected takes the labs the change since
//                                 --base reaches instead (cli/reach.hpp, each lab by its own
//                                 source's object; a moved baseline row reaches its lab, a build
//                                 file every lab), and --list names them and runs nothing. A repo
//                                 with a baseline has its labs held to it by `ge test` (the
//                                 labs_smoke case). --diff prints only the labs whose digest is off
//                                 the baseline (or not in it), and writes nothing: after a change,
//                                 the labs it moved in one command.
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

#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "cli/build.hpp"
#include "cli/outcome.hpp"
#include "cli/reach.hpp"

namespace ge::cli {

inline constexpr std::string_view kDigestBaseline = "tests/labs_digests.txt";

struct DigestOptions {
  std::vector<std::string> labs;  // none: every lab
  bool write = false, diff = false;
  bool affected = false, list = false;  // the labs the change since `base` reaches
  std::string base = "origin/main";
};
Outcome run_digest(const BuildOptions& o, const DigestOptions& d);

// The labs a change reaches, in id order: each lab whose own source object `r.objects` holds,
// each lab with a row in `moved` (the baseline's rows the change rewrote), and every lab when a
// build file changed. `sources` is each registered lab's repository-relative source file.
std::vector<std::string> labs_reached(const Reach& r,
                                      const std::map<std::string, std::string>& sources,
                                      const std::set<std::string>& moved);
// A changed committed asset (`<root>/assets/<name>/<file>`, not its sources or its builder): a
// lab reads it at run time by path, which no include records.
struct AssetChange {
  std::string root, name;  // "plugins/skier", "skier"
};
std::vector<AssetChange> asset_changes(const std::set<std::string>& changed);
// Whether the lab at `source` (its text `text`) reads `a`: it lives under the asset's root, or
// its text names `assets/<name>/` or `<name>.glb`.
bool reads_asset(const std::string& source, std::string_view text, const AssetChange& a);
// The lab ids whose rows a unified diff of the baseline adds or removes ("+<id> <16 hex>").
std::set<std::string> moved_digest_rows(std::string_view diff);
Outcome run_golden_diff(const fs::path& repo, const std::vector<std::string>& targets,
                        const std::string& base);
Outcome run_golden_write(const BuildOptions& o, const std::string& test);

// One golden file's two versions compared, row by row. Rows match by their place, unless the
// count changed and every row's first word is a unique key on both sides most rows share (a seed,
// a scenario's id): then they match by that key, so a row inserted mid-table is one row added and
// the rows after it are not shown as moved.
struct GoldenCell {
  std::string column;  // the header's name, or "column N" (from 1)
  std::string before, after;
};
struct GoldenRow {
  size_t row = 0;                 // from 1, among the data rows
  std::string key;                // the row's first word (a seed, a name), before the change
  std::string change;             // "" moved; "added" or "gone" (rows matched by key)
  std::vector<GoldenCell> cells;  // only the words that moved; a word added or gone is "-"
};
struct GoldenDiff {
  std::string file;
  size_t rows_before = 0, rows_after = 0;
  bool header_changed = false;
  bool by_key = false;          // the rows matched by their first word, not their place
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
