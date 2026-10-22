// THE CHARTS every lab, bench, profile and debug dump draws, so an agent can LOOK at a run's
// numbers the way it reads a log: a line over time, a scatter, a histogram, percentile boxes, a
// heatmap and a flame timeline. Each returns a `Chart`: the `Plot` (PNG through the raster, SVG
// as text, plot.hpp), the outliers it found and circled, and a summary of every series, so the
// finding is in the text as well as in the picture (`chart_text`).
//
// THE OUTLIERS are stats.hpp's, one rule everywhere: a line chart rings the SPIKES of each line
// (a value that jumps out of its neighbours), a scatter, a histogram and a box chart the
// OUTLIERS of each set (robust z over 3.5), a heatmap its hottest outlying cells, a timeline
// the spans that ran far longer than others of the same name. A ring is red (the one colour no
// series wears) and carries its value as a tooltip in the SVG. Findings come strongest first
// within each series, and at most 60 are ringed, so a noisy run still rings its worst.
//
// THE LOOK, the same on every chart so they read as one system: a near-white surface, recessive
// grid and axes, text in two inks, series in a fixed categorical order (never cycled: an eighth
// series is folded into the seventh's colour and named in the legend), one y axis a chart (two
// measures of different scale are two charts), a legend whenever there are two series or more,
// numbers on round ticks. A heatmap is one hue, light to dark. Every chart has a title that says
// what it shows; put the run's inputs in the lab shot's caption.
//
// ge:summary Charts for labs and dumps: line, scatter, histogram, box, heatmap, flame timeline
// ge:keywords chart graph plot line-chart scatter histogram box-plot percentile heatmap flame
// ge:keywords timeline spans outlier spike legend axis ticks visualize visualization
// ge:keywords line_chart histogram_chart box_chart heatmap_chart span_chart chart_text
// ge:skill debug labs
// ge:test tests/lab_chart_test.cpp
#pragma once

#include <string>
#include <vector>

#include "lab/plot.hpp"
#include "lab/stats.hpp"

namespace ge::lab {

struct ChartFrame {
  std::string title;     // what the chart shows, one line
  std::string subtitle;  // the run it is of (seed, build, flags), one line
  std::string x_label;   // with its unit: "t (s)"
  std::string y_label;   // with its unit: "speed (km/h)"
  int width = 960;
  int height = 420;  // line charts and histograms; a box chart, heatmap and timeline fit their rows
};

// One line of a line chart or one set of points of a scatter.
struct Trace {
  std::string label;
  std::vector<double> x, y;
};

// A labelled set of values (a histogram's or a box chart's).
struct Sample {
  std::string label;
  std::vector<double> values;
};

// A vertical line at an x (an event: a gate, a landing, a hitch).
struct Mark {
  double x;
  std::string label;
};

// A shaded stretch of x (a phase: in the air, down, loading).
struct Band {
  double x0, x1;
  std::string label;
};

struct LineChart {
  ChartFrame frame;
  std::vector<Trace> traces;
  std::vector<Mark> marks;
  std::vector<Band> bands;
  std::vector<double> rules;  // horizontal dashed lines at these y (a budget, a threshold)
  bool log_y = false;         // a log scale: values <= 0 are left out
  bool points = false;        // dots instead of lines (a scatter)
  bool spikes = true;         // ring each trace's spikes (points: each set's outliers)
};

// What a chart found worth a ring.
struct Finding {
  std::string series;
  std::string kind;  // "spike", "outlier", "slow span", "hot cell"
  double x = 0;      // where: a line's x, a span's start (ms), a cell's x; an outlier of a set
                     // (histogram, box) its index in the set
  double y = 0;      // the value
  std::string note;  // a sentence: what it is against what is usual
};

struct Chart {
  Plot plot;
  std::vector<Finding> findings;
  std::vector<std::pair<std::string, Summary>> summaries;  // one per series or set
  std::string title;
};

Chart line_chart(const LineChart& spec);
// A histogram of each sample over shared bins, overlaid (before and after: two samples).
Chart histogram_chart(const ChartFrame& frame, const std::vector<Sample>& samples, int bins = 40);
// One horizontal box a sample: p25 to p75, the median, whiskers p5 to p95, outliers as dots.
Chart box_chart(const ChartFrame& frame, const std::vector<Sample>& samples);
// A grid of `rows` x `columns` (row-major `values`), one hue light to dark; NaN cells grey. The
// columns span x from `x0` to `x1` (a time axis: one column a frame).
Chart heatmap_chart(const ChartFrame& frame, const std::vector<std::string>& rows,
                    const std::vector<double>& values, double x0, double x1);
// A flame timeline: one bar a span at its start and length (x, ns shown as ms), stacked by depth.
Chart span_chart(const ChartFrame& frame, const std::vector<std::string>& names,
                 const std::vector<double>& depth, const std::vector<double>& start_ns,
                 const std::vector<double>& dur_ns);

// The chart as an agent reads it: the title, a line a series (n, min, p50, p95, p99, max), then
// every finding (at most `most` listed, the count of the rest; 0: the count alone).
std::string chart_text(const Chart& chart, size_t most = 20);

}  // namespace ge::lab
