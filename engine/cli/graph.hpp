// `ge graph`: any series or spans file (engine/lab/series.hpp) charted, as a PNG to look at and
// an SVG whose labels, values and rings are text, with every outlier the chart rings returned as
// a note finding. Several files on one command overlay: the same column of each file on one chart
// (a before and an after run), so a regression is a second line where the first is flat.
//
// What is drawn, by `--kind` (auto picks spans for a spans file, line for the rest):
//   line     one chart a column, x the file's x column, spikes ringed
//   scatter  the same as points, outliers ringed
//   hist     one histogram a column, the files overlaid on shared bins
//   box      one box chart: a row a column (and a file), percentiles and outliers
//   heat     one heatmap: a row a column, a cell a sample along x (the first file)
//   spans    a flame timeline a file and a box chart of every zone's durations
//
// Each chart is written as <out>/<file stem>.<column>.<kind>.png and .svg (out: a `graphs/`
// directory beside the first file, unless --out says otherwise).
//
// ge:summary ge graph: series and spans files charted to PNG and SVG, outliers as findings
// ge:keywords cli ge graph chart plot visualize series spans before-after overlay outliers
// ge:keywords run_graph graph-options png svg debug bench profile
// ge:skill agent-interface debug
// ge:test tests/cli_graph_test.cpp
#pragma once

#include <string>
#include <vector>

#include "cli/outcome.hpp"

namespace ge::cli {

struct GraphOptions {
  std::vector<std::string> files;
  std::string kind = "auto";         // auto, line, scatter, hist, box, heat, spans
  std::vector<std::string> columns;  // empty: every numeric column but x
  std::string x;                     // the x column (default: the file's `# x:` or its first)
  std::string out;                   // the directory the charts go in
  std::string title;                 // a title before each chart's own
  bool log_y = false;
};

Outcome run_graph(const GraphOptions& opts);

}  // namespace ge::cli
