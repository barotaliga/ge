// A RUN'S SERIES WRITTEN AND DRAWN, the step-by-step half of a lab: the series file attached
// beside the report (`<prefix>trace.tsv`, readable by `ge graph` and diffable run against run)
// and one line chart a group of its columns against its x column, each attached as SVG, shown
// as a captioned shot and summarised in a note, with the run's events as marks and its phases as
// bands. Every lab that steps something (the trace lab down a seed, a plugin's scenario lab under
// --trace) draws through this, so their charts read the same and their files have one shape.
//
//   gl::SeriesCharts out{"launch.", "launch", sub, {{"scenario", "launch"}}, marks, bands};
//   gl::chart_series(lab, s, out, {{"height", "HEIGHT OVER THE SNOW", "agl (m)", {"agl"}, ...}});
//
// ge:summary A lab's per-step series attached as a tsv and drawn as captioned line charts
// ge:keywords series trace charts per-step timeline tsv svg marks bands spikes chart_series
// ge:keywords scenario trace-flag step-by-step
// ge:skill debug labs
// ge:test tests/lab_series_charts_test.cpp
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/series.hpp"

namespace ge::lab {

// One chart of a series: some of its columns against its x column, on one y axis.
struct SeriesChart {
  std::string id;       // the shot's and the SVG's stem, after the prefix
  std::string title;    // upper case: "SPEED DOWN SEED 38"
  std::string y_label;  // with its unit: "speed (km/h)"
  std::vector<std::string> columns;
  std::string caption;   // what the lines are, a sentence
  std::string look_for;  // what a reviewer checks it for
};

// Where the files go and what every chart shares.
struct SeriesCharts {
  std::string prefix;  // the files' and shots' stem ("" or "launch."), so a lab charts several runs
  std::string key;     // the metrics' stem: `<key>.<id>.spikes`
  std::string subtitle;                                     // the run the charts are of, one line
  std::vector<std::pair<std::string, std::string>> inputs;  // the shots' inputs
  std::vector<Mark> marks;                                  // events, at their x
  std::vector<Band> bands;                                  // phases, from x to x
  std::string legend;  // a sentence on what the marks and bands are, appended to captions
};

// Attach `s` as `<prefix>trace.tsv`, then draw each of `charts` (a column `s` lacks is named in a
// lab failure, never drawn empty). Each chart's spikes are a metric, `<key>.<id>.spikes`.
void chart_series(Lab& lab, const Series& s, const SeriesCharts& out,
                  const std::vector<SeriesChart>& charts);

}  // namespace ge::lab
