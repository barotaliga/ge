// `ge graph` (engine/cli/graph.hpp): a series file charted to PNG and SVG with its spike as a
// note, two files overlaid on one chart, a spans file as a timeline and zone boxes, and a usage
// error for an unknown kind.
//
// ge:summary Test: ge graph's charts, overlays, spans and errors
// ge:keywords test cli graph chart png svg overlay spans
// ge:skill testing agent-interface debug
#include <filesystem>
#include <string>

#include "check.hpp"
#include "cli/finding.hpp"
#include "cli/graph.hpp"

namespace fs = std::filesystem;
using namespace ge::cli;

namespace {

fs::path scratch() {
  const fs::path dir = fs::temp_directory_path() / "ge_cli_graph_test";
  fs::remove_all(dir);
  fs::create_directories(dir);
  return dir;
}

std::string run_file(int spike_at) {
  std::string s = "# unit.ms: ms\nstep\tms\n";
  for (int i = 0; i < 400; i++) {
    const double v = i == spike_at ? 40 : 8 + (i % 5) * 0.1;
    s += std::to_string(i) + "\t" + std::to_string(v) + "\n";
  }
  return s;
}

}  // namespace

TEST(a_series_is_charted_with_its_spike_noted) {
  const fs::path dir = scratch();
  std::string error;
  CHECK(write_text(dir / "before.tsv", run_file(-1), error));
  CHECK(write_text(dir / "after.tsv", run_file(250), error));
  GraphOptions g;
  g.files = {(dir / "before.tsv").string(), (dir / "after.tsv").string()};
  const Outcome o = run_graph(g);
  CHECK(exit_of(o) == 0);
  CHECK(fs::exists(dir / "graphs" / "before.ms.line.png"));
  CHECK(fs::exists(dir / "graphs" / "before.ms.line.svg"));
  CHECK(count_of(o.findings, Severity::note) == 1);
  CHECK(o.result.find("\"kind\":\"line\"") != std::string::npos);
  CHECK(o.text.find("after: n 400") != std::string::npos);  // both files on one chart
  fs::remove_all(dir);
}

TEST(a_spans_file_is_a_timeline_and_zone_boxes) {
  const fs::path dir = scratch();
  std::string s = "name\tdepth\tstart_ns\tdur_ns\n";
  for (int i = 0; i < 30; i++) {
    s += "frame\t0\t" + std::to_string(i * 2000000) + "\t1000000\n";
    s += "step\t1\t" + std::to_string(i * 2000000) + "\t" + (i == 9 ? "900000" : "500000") + "\n";
  }
  std::string error;
  CHECK(write_text(dir / "zones.tsv", s, error));
  GraphOptions g;
  g.files = {(dir / "zones.tsv").string()};
  g.out = (dir / "out").string();
  const Outcome o = run_graph(g);
  CHECK(exit_of(o) == 0);
  CHECK(fs::exists(dir / "out" / "zones.timeline.spans.png"));
  CHECK(fs::exists(dir / "out" / "zones.zones.box.svg"));
  CHECK(o.result.find("\"kind\":\"spans\"") != std::string::npos);
  CHECK(o.text.find("step (ms)") != std::string::npos);
  fs::remove_all(dir);
}

TEST(a_bad_kind_or_file_is_an_error) {
  GraphOptions g;
  g.files = {"no-such-file.tsv"};
  g.kind = "pie";
  CHECK(exit_of(run_graph(g)) == 2);
  g.kind = "auto";
  const Outcome o = run_graph(g);
  CHECK(exit_of(o) == 1);
  CHECK(!o.findings.empty() && o.findings[0].what.find("cannot read") != std::string::npos);
}
