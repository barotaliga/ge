// engine/lab/chart: each chart form rings what was planted in its data and nothing else, a
// before-and-after overlay keeps both lines, and chart_text says it in words.
//
// ge:summary Test: each chart form finds the planted outliers; chart_text reports them
// ge:keywords test chart line histogram box heatmap spans outlier spike chart_text
// ge:skill testing debug
#include <string>
#include <vector>

#include "check.hpp"
#include "lab/chart.hpp"

namespace gl = ge::lab;

namespace {

std::vector<double> ramp(size_t n, double step) {
  std::vector<double> v(n);
  for (size_t i = 0; i < n; i++) v[i] = static_cast<double>(i) * step;
  return v;
}

std::vector<double> wobble(size_t n, double centre) {
  std::vector<double> v(n);
  for (size_t i = 0; i < n; i++) v[i] = centre + 0.3 * static_cast<double>(i % 7) - 0.9;
  return v;
}

}  // namespace

TEST(a_line_chart_rings_its_spike) {
  gl::LineChart lc;
  lc.frame = {"T", "", "t (s)", "v"};
  std::vector<double> y = wobble(2000, 10);
  y[1500] = 40;
  lc.traces.push_back({"before", ramp(2000, 0.01), wobble(2000, 10)});
  lc.traces.push_back({"after", ramp(2000, 0.01), y});
  const gl::Chart c = gl::line_chart(lc);
  CHECK(c.findings.size() == 1);
  CHECK(c.findings.size() == 1 && c.findings[0].series == "after" && c.findings[0].y == 40);
  CHECK(c.summaries.size() == 2);
  const std::string svg = gl::to_svg(c.plot);
  CHECK(svg.find(">after</text>") != std::string::npos);  // the legend
  CHECK(gl::chart_text(c).find("spike: after reads 40") != std::string::npos);
}

TEST(sets_ring_their_outliers) {
  std::vector<double> a = wobble(500, 10);
  a[3] = 90;
  const gl::Chart h = gl::histogram_chart({"H", "", "v", ""}, {{"a", a}});
  CHECK(h.findings.size() == 1 && h.findings[0].x == 3 && h.findings[0].y == 90);
  const gl::Chart b = gl::box_chart({"B", "", "v", ""}, {{"a", a}, {"b", wobble(500, 5)}});
  CHECK(b.findings.size() == 1 && b.findings[0].series == "a");
}

TEST(a_heatmap_rings_its_hot_cell_and_a_timeline_its_slow_span) {
  std::vector<double> grid = wobble(200, 1);
  grid[150] = 50;
  const gl::Chart hm = gl::heatmap_chart({"M", "", "t", ""}, {"r0", "r1"}, grid, 0, 1);
  CHECK(hm.findings.size() == 1 && hm.findings[0].series == "r1");
  std::vector<std::string> names;
  std::vector<double> depth, start, dur;
  for (int i = 0; i < 20; i++) {
    names.emplace_back("step");
    depth.push_back(0);
    start.push_back(i * 2e6);
    dur.push_back(i == 11 ? 1.8e6 : 0.5e6 + (i % 3) * 1e4);
  }
  const gl::Chart sp = gl::span_chart({"S", "", "", ""}, names, depth, start, dur);
  CHECK(sp.findings.size() == 1 && sp.findings[0].kind == "slow span");
  CHECK(sp.findings.size() == 1 && sp.findings[0].x == 22);  // ms from the first span
}
