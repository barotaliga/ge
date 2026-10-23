// engine/lab/series_charts.hpp: a series attached as a tsv that reads back the same, one SVG, one
// captioned shot and one spikes metric a chart, the prefix on every file so a lab charts several
// runs, and a chart naming a column the series lacks failing the lab instead of drawing empty.
//
// ge:summary Test: chart_series attaches the tsv, each chart's SVG, shot and spikes metric
// ge:keywords test lab series charts chart_series trace tsv svg shot prefix
// ge:skill testing debug labs
#include <string>
#include <vector>

#include "check.hpp"
#include "lab/series_charts.hpp"

using namespace ge::lab;

namespace {

void nothing(Lab&) {}
const LabSpec kSpec{"series-charts", LabKind::scenario, "engine", "tests", "labs", "?", {}, "",
                    nothing};

Series ramp() {
  Series s = make_series({"t_s", "speed", "height"});
  s.set("x", "t_s");
  s.set("unit.t_s", "s");
  for (int i = 0; i < 40; i++) add_row(s, {i * 0.1, i * 1.0, i < 20 ? 0.0 : 3.0});
  return s;
}

const Lab::File* file(const Lab& lab, const std::string& name) {
  for (const Lab::File& f : lab.files()) {
    if (f.name == name) return &f;
  }
  return nullptr;
}

}  // namespace

TEST(the_series_and_each_chart_are_attached_under_the_prefix) {
  Lab lab(kSpec, {});
  const Series s = ramp();
  SeriesCharts out{"launch.",
                   "launch",
                   "a ramp",
                   {{"scenario", "launch"}},
                   {{2.0, "UP"}},
                   {{2.0, 3.9, "AIR"}},
                   "The grey line is the take-off."};
  chart_series(lab, s, out,
               {{"speed", "SPEED", "speed (m/s)", {"speed"}, "The speed.", "a straight line"},
                {"height", "HEIGHT", "height (m)", {"height"}, "The height.", "a step at 2 s"}});
  CHECK(lab.failures().empty());
  const Lab::File* tsv = file(lab, "launch.trace.tsv");
  CHECK(tsv != nullptr);
  if (tsv == nullptr) return;
  Series back;
  std::string error;
  CHECK(read_series(std::string(tsv->bytes.begin(), tsv->bytes.end()), back, error));
  CHECK(back.rows() == 40);
  CHECK(file(lab, "launch.speed.svg") != nullptr);
  CHECK(file(lab, "launch.height.svg") != nullptr);
  CHECK(lab.shots().size() == 2);
  CHECK(lab.shots()[0].shot.id == "launch.speed");
  CHECK(lab.shots()[1].shot.caption.find("take-off") != std::string::npos);
  bool spikes = false;
  for (const Metric& m : lab.metrics()) spikes = spikes || m.key == "launch.height.spikes";
  CHECK(spikes);
}

TEST(a_missing_column_fails_the_lab_and_draws_nothing) {
  Lab lab(kSpec, {});
  chart_series(lab, ramp(), SeriesCharts{"", "k", "", {}, {}, {}, ""},
               {{"bank", "BANK", "bank (deg)", {"bank"}, "", ""}});
  CHECK(lab.failures().size() == 1);
  CHECK(lab.failures()[0].find("bank") != std::string::npos);
  CHECK(lab.shots().empty());
  CHECK(file(lab, "trace.tsv") != nullptr);
}
