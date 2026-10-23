// See series_charts.hpp.
//
// ge:summary chart_series: a series attached as tsv, each chart attached as SVG and shot
// ge:keywords series trace charts chart_series svg tsv
// ge:skill debug labs
// ge:test tests/lab_series_charts_test.cpp
#include "lab/series_charts.hpp"

#include <cstdint>

namespace ge::lab {

void chart_series(Lab& lab, const Series& s, const SeriesCharts& out,
                  const std::vector<SeriesChart>& charts) {
  const std::string text = write_series(s);
  lab.attach(out.prefix + "trace.tsv", std::vector<uint8_t>(text.begin(), text.end()));
  const Column* x = s.x();
  if (x == nullptr) {
    lab.fail("the series for " + out.key + " has no numeric column to chart against",
             "Give make_series a numeric x column and set(\"x\", <name>); see "
             "engine/lab/series.hpp.");
    return;
  }
  const std::string x_unit = s.unit(x->name);
  const std::string x_label = x_unit.empty() ? x->name : x->name + " (" + x_unit + ")";
  for (const SeriesChart& c : charts) {
    LineChart lc;
    lc.frame = {c.title, out.subtitle, x_label, c.y_label};
    bool whole = true;
    for (const std::string& name : c.columns) {
      const Column* col = s.find(name);
      if (col == nullptr || col->text) {
        lab.fail("chart " + c.id + " of " + out.key + " names column '" + name +
                     "', which the series has no numeric column for",
                 "Name one of the series' columns in the chart, or add it to make_series; "
                 "skill: labs.");
        whole = false;
        continue;
      }
      lc.traces.push_back({name, x->num, col->num});
    }
    if (!whole) continue;
    lc.marks = out.marks;
    lc.bands = out.bands;
    const Chart ch = line_chart(lc);
    const std::string svg = to_svg(ch.plot);
    lab.attach(out.prefix + c.id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
    lab.metric(out.key + "." + c.id + ".spikes", static_cast<double>(ch.findings.size()), "count");
    lab.note(chart_text(ch, 8));
    std::string caption = c.caption;
    if (!out.legend.empty()) caption += " " + out.legend;
    caption += " Red rings are spikes (a step out of line with its neighbours).";
    lab.shot({out.prefix + c.id, c.title, caption, c.look_for, out.inputs}, to_picture(ch.plot));
  }
}

}  // namespace ge::lab
