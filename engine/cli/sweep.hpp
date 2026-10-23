// `ge lab sweep <id> --set=<name>=<a>,<b>,...`: one constant of the code run at each of several
// values through one lab, every metric side by side. The constant is a `constexpr` definition on
// one line (`inline constexpr double kSlope = 3.2;`, a spec's or a tuning table's), found by name
// among the repository's sources; for each value its initializer is rewritten in place, the labs
// executable rebuilt (only it: the files that include the constant recompile) and the lab run with
// --tag=sweep-<name>-<value>, so every run's pictures stay under previews/ for `ge lab diff`. The
// file is put back after the last run, and the labs rebuilt on the value it holds. A sweep killed
// half way leaves its backup under build/<preset>/ge-sweep/, and the next sweep puts it back first.
//
//   ge lab sweep para --set=kFlight=0.5,0.7,0.9 -- --scenario=launch
//   ge lab sweep ride --set=snow::kPlaneSpeed=4,6,8 --metrics=top-speed -- --scenario=schuss
//
// ge:summary ge lab sweep: one constant rewritten to each value, the lab rebuilt and run, metrics
// ge:keywords cli ge lab sweep tune tuning constant constexpr spec value grid metrics compare
// ge:keywords rebuild run_lab_sweep find_constants with_value sweep_table
// ge:skill agent-interface labs
// ge:test tests/cli_sweep_test.cpp
#pragma once

#include <string>
#include <vector>

#include "cli/build.hpp"
#include "cli/outcome.hpp"

namespace ge::cli {

struct SweepOptions {
  std::string id;       // the lab
  std::string set;      // "<name>=<a>,<b>,...": the name bare (kSlope) or qualified (spec::kSlope)
  std::string in;       // the source file the constant is in, when its name is in several ("")
  std::string metrics;  // comma-separated words a listed metric's key holds one of ("": all)
  std::vector<std::string> passthrough;  // the lab's own flags
};
Outcome run_lab_sweep(const BuildOptions& o, const SweepOptions& s);

// A one-line constexpr definition: `<head><value><tail>` is its line.
struct ConstantSite {
  std::string file;  // repository-relative, '/'-separated
  int line = 0;      // 1-based
  std::string head, value, tail;
};

// Every one-line constexpr definition of `name` (its last `::` part) in `text`, the file's path
// `file`. A qualified name keeps a site only when the file opens a namespace for each qualifier.
std::vector<ConstantSite> find_constants(const std::string& text, const std::string& file,
                                         const std::string& name);
// `text` with `site`'s initializer replaced by `value`.
std::string with_value(const std::string& text, const ConstantSite& site, const std::string& value);

// One run of a sweep: the value and the metrics it measured, as `key\tvalue\tunit` rows.
struct SweepRun {
  std::string value;
  std::vector<std::string> rows;  // metrics.tsv's lines
};
// The metrics side by side: a row a key (those holding one of `filters`' words, or all), a column
// a value, `current` marked with a star.
std::string sweep_table(const std::vector<SweepRun>& runs, const std::string& current,
                        const std::vector<std::string>& filters);

}  // namespace ge::cli
