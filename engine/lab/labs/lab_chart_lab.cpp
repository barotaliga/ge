// LAB `charts`: every chart form the engine draws (chart.hpp), over seeded data with outliers
// PLANTED at known places, so the lab answers whether each chart still draws, still reads, and
// still rings exactly what was planted. Each chart is a captioned shot and an SVG beside it, and
// a series file is written and read back to hold the format (series.hpp) to its round trip. This
// is the reference sheet for what a chart looks like: open previews/charts/index.md.
//
// ge:summary Lab: every chart form over seeded data with planted outliers, PNG and SVG
// ge:keywords lab charts chart plot graph outlier spike histogram box heatmap flame svg series
// ge:skill debug labs
// ge:test tests/lab_chart_test.cpp
#include <cstdint>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/prng.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/series.hpp"

namespace {

namespace gl = ge::lab;

void shoot(gl::Lab& lab, const std::string& id, const gl::Chart& chart, const std::string& caption,
           const std::string& look_for, size_t planted) {
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({id,
            chart.title,
            caption + " The SVG beside it (" + id +
                ".svg) carries every label and "
                "ring as text.",
            look_for,
            {{"seed", lab.arg("seed")}, {"points", lab.arg("points")}}},
           gl::to_picture(chart.plot));
  lab.metric(id + ".findings", static_cast<double>(chart.findings.size()), "count");
  lab.note(gl::chart_text(chart, 6));
  if (chart.findings.size() != planted) {
    lab.fail(id + ": rang " + std::to_string(chart.findings.size()) + " values, " +
                 std::to_string(planted) + " were planted",
             "Read engine/lab/stats.hpp's outliers and spikes and the chart's builder in "
             "engine/lab/chart.cpp, then rerun `make lab LAB=charts` (skill: debug).");
  }
}

void run(gl::Lab& lab) {
  auto rng = ge::core::make_rng(static_cast<uint32_t>(lab.arg_num("seed")));
  const auto n = static_cast<size_t>(lab.arg_num("points"));
  const auto noise = [&rng](double amp) { return amp * (ge::core::next(rng) * 2 - 1); };

  // A run over time: a speed that builds and settles, a frame time with hitches.
  gl::Series s = gl::make_series({"t_s", "speed", "frame_ms"});
  s.set("title", "a planted run");
  s.set("unit.speed", "km/h");
  s.set("unit.frame_ms", "ms");
  const size_t hitch_a = n / 3, hitch_b = 2 * n / 3, spike = n / 2;
  for (size_t i = 0; i < n; i++) {
    const double t = static_cast<double>(i) / 120;
    double speed = 70 * (1 - ge::core::libm::exp(-t / 3)) + noise(1.5);
    if (i == spike) speed += 30;
    double frame = 8.3 + noise(0.4);
    if (i == hitch_a || i == hitch_b) frame += 25;
    gl::add_row(s, {t, speed, frame});
  }
  gl::Series back;
  std::string error;
  if (!gl::read_series(gl::write_series(s), back, error) ||
      gl::write_series(back) != gl::write_series(s)) {
    lab.fail("the series file did not read back to itself: " + error,
             "Run tests/lab_series_test and read engine/lab/series.cpp (skill: debug).");
  }
  const std::string text = gl::write_series(s);
  lab.attach("run.tsv", std::vector<uint8_t>(text.begin(), text.end()));
  const auto& t = s.find("t_s")->num;

  gl::LineChart line;
  line.frame = {"SPEED OVER A PLANTED RUN", "one spike planted at the run's middle", "t (s)",
                "speed (km/h)"};
  line.traces.push_back({"speed", t, s.find("speed")->num});
  line.marks.push_back({t[n / 4], "GATE 3"});
  line.bands.push_back({t[n / 5], t[n / 5 + n / 20], "AIR"});
  shoot(lab, "line", gl::line_chart(line),
        "A line over time: the speed (km/h) against the run's clock, a gate marked by a grey line "
        "and a flight shaded. Red rings are spikes: values out of line with their neighbours.",
        "one red ring at the middle of the run and nowhere else; the ticks are round numbers", 1);

  gl::LineChart frames;
  frames.frame = {"FRAME TIME, BEFORE AND AFTER", "two hitches planted in the after run", "t (s)",
                  "frame (ms)"};
  std::vector<double> before(s.find("frame_ms")->num);
  before[hitch_a] -= 25;
  before[hitch_b] -= 25;
  frames.traces.push_back({"before", t, before});
  frames.traces.push_back({"after", t, s.find("frame_ms")->num});
  frames.rules.push_back(16.7);
  shoot(lab, "before-after", gl::line_chart(frames),
        "Two runs on one chart (how ge graph overlays a before and an after file): the frame "
        "time against the clock, the dashed line the 60 Hz budget.",
        "two red rings, both on the AFTER line, a third and two thirds of the way; a legend", 2);

  std::vector<double> sample_a(n), sample_b(n);
  for (size_t i = 0; i < n; i++) {
    sample_a[i] = 10 + noise(2) + noise(2);
    sample_b[i] = 12 + noise(2) + noise(2);
  }
  sample_b[7] = 40;
  shoot(lab, "histogram",
        gl::histogram_chart(
            {"STEP COST, BEFORE AND AFTER", "one outlier planted in AFTER", "step (us)", ""},
            {{"before", sample_a}, {"after", sample_b}}),
        "Two histograms over shared bins, overlaid; each set's P50 a line, its P95 dashed. A red "
        "ring under the axis is an outlier.",
        "AFTER sits to the right of BEFORE; one red ring at 40", 1);

  shoot(lab, "boxes",
        gl::box_chart({"ZONES PER STEP", "the skier zone has one planted outlier", "cost (us)", ""},
                      {{"skier", sample_b}, {"course", sample_a}, {"bot", before}}),
        "One box a zone: the box p25 to p75, the black bar the median, whiskers p5 to p95, dots "
        "the outliers (ringed red). The right column prints p50 / p95 / max.",
        "three boxes; a single ring on the skier row", 1);

  constexpr size_t kRows = 6;
  constexpr size_t kCols = 120;
  std::vector<double> grid(kRows * kCols);
  for (size_t r = 0; r < kRows; r++) {
    for (size_t c = 0; c < kCols; c++) {
      grid[r * kCols + c] = 20 + 10 * static_cast<double>(r) + noise(3);
    }
  }
  grid[2 * kCols + 90] = 160;
  shoot(lab, "heatmap",
        gl::heatmap_chart({"JOLT A JOINT A FRAME", "one cell planted hot", "t (s)", "m/s2"},
                          {"knee l", "knee r", "hip", "elbow l", "elbow r", "head"}, grid, 0, 1),
        "A heatmap: a row a joint, a column a frame, one hue light (low) to dark (high), the scale "
        "at the right.",
        "rows darken downwards; one red ring on the HIP row three quarters along", 1);

  std::vector<std::string> names;
  std::vector<double> depth, start, dur;
  double at = 0;
  for (int frame = 0; frame < 24; frame++) {
    const double step = frame == 17 ? 3.0e6 : 0.6e6 + noise(0.05e6);
    names.emplace_back("frame");
    depth.push_back(0);
    start.push_back(at);
    dur.push_back(step + 0.4e6);
    names.emplace_back("step");
    depth.push_back(1);
    start.push_back(at);
    dur.push_back(step);
    names.emplace_back("draw");
    depth.push_back(1);
    start.push_back(at + step);
    dur.push_back(0.35e6 + noise(0.02e6));
    at += step + 0.4e6 + 1.2e6;
  }
  shoot(lab, "spans",
        gl::span_chart({"A FLAME TIMELINE", "frame 17's step planted slow", "", ""}, names, depth,
                       start, dur),
        "Spans (what ge profile writes) as a timeline: a bar a zone entered, at its start and "
        "length in ms, nested zones stacked under their parent.",
        "frames in a row, STEP and DRAW under each; red rings on frame 17's FRAME and STEP", 2);
}

const gl::LabSpec kSpec{
    "charts",
    gl::LabKind::picture,
    "engine",
    "engine/lab/chart.hpp, plot.hpp, stats.hpp, series.hpp",
    "debug",
    "Does every chart form draw, read, and ring exactly the outliers planted in its data?",
    {
        {"seed", "7", "the data's seed"},
        {"points", "1200", "samples in each run and set"},
    },
    "--points=600",
    run,
};
GE_LAB(kSpec);

}  // namespace
