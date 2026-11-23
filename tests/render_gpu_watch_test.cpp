// engine/render/gpu_watch.hpp and gpu_debug.hpp: gpustats reads the frames asked for and then
// reports the frame's and the CPU's medians, the GPU's span and the dearest pass first; a pass
// that drew nothing or was not timed is left out; a GPU with no timestamps is said so; a quiet
// watch keeps its report unprinted; gpudebug's notes are the game's, then "# gpu" and the report.
//
// ge:summary Tests for gpustats' measuring and report, and gpudebug's notes
// ge:keywords test gpustats gpu-watch report gpudebug notes
// ge:skill testing render
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
#include "render/gpu_debug.hpp"

namespace {

namespace gr = ge::render;

gr::FrameStats stats(double span_start, double shadows, double world) {
  gr::FrameStats s;
  s.prepare_ms = 0.5;
  s.count = 3;
  s.passes[0] = {"shadows", shadows, span_start, 0.2, 10, 0};
  s.passes[1] = {"world", world, span_start + shadows, 0.3, 40, 0};
  s.passes[2] = {"empty", 0.0, 0.0, 0.1, 0, 0};  // drew nothing
  return s;
}

}  // namespace

TEST(gpustats_reads_its_frames_then_reports_the_dearest_pass_first) {
  ge::console::Console con;
  std::vector<std::string> said;
  con.set_sink([&said](std::string_view l) { said.emplace_back(l); });
  gr::GpuWatch w;
  gr::add_gpustats(con, w);
  con.execute("gpustats 3");
  said.clear();
  for (int i = 0; i < 3; ++i) gr::gpu_watch_frame(w, stats(0, 1.0, 4.0), 0.010, con);
  gr::gpu_watch_frame(w, stats(0, 9, 9), 0.5, con);  // past its frames: not read
  CHECK(w.read == 3);
  CHECK_MSG(!said.empty() && said[0].find("frame 10.00 ms (100 fps)") != std::string::npos,
            said.empty() ? "" : said[0]);
  CHECK(said[0].find("the frame's CPU 1.10 ms") != std::string::npos);
  bool span = false, world_first = false, empty = false;
  for (size_t i = 0; i < said.size(); ++i) {
    span |= said[i].find("GPU frame span") != std::string::npos;
    if (said[i].find("GPU by pass") != std::string::npos && i + 1 < said.size())
      world_first = said[i + 1].find("world 4.000 ms") != std::string::npos;
    empty |= said[i].find("empty") != std::string::npos;
  }
  CHECK(span);
  CHECK(world_first);
  CHECK(!empty);
  CHECK(said == w.report);
}

TEST(a_gpu_with_no_timestamps_is_said_so) {
  ge::console::Console con;
  gr::GpuWatch w;
  gr::start_gpu_watch(w, 1, true);
  const double nan = std::numeric_limits<double>::quiet_NaN();
  gr::FrameStats s = stats(nan, nan, nan);
  std::vector<std::string> said;
  con.set_sink([&said](std::string_view l) { said.emplace_back(l); });
  gr::gpu_watch_frame(w, s, 0.016, con);
  CHECK(said.empty());  // quiet
  CHECK(w.report.size() == 2);
  CHECK(w.report.size() == 2 && w.report[1].find("not measured") != std::string::npos);
}

TEST(gpudebug_notes_are_the_games_then_the_gpus) {
  ge::console::Console con;
  gr::GpuDebug g;
  gr::add_gpu_debug(con, g, nullptr, [] { return std::vector<std::string>{"# where", "here"}; });
  CHECK(g.shot.start(2, nullptr));
  CHECK(g.watch.left == 2 && g.watch.quiet);  // its frames measured quietly
  gr::gpu_watch_frame(g.watch, stats(0, 1, 2), 0.01, con);
  gr::gpu_watch_frame(g.watch, stats(0, 1, 2), 0.01, con);
  const std::vector<std::string> notes = g.shot.notes();
  CHECK(notes.size() > 4);
  CHECK(notes.size() > 4 && notes[0] == "# where" && notes[1] == "here" && notes[2] == "# gpu");
  CHECK(notes.size() > 4 && notes[3].find("gpustats: frame") == 0);
  // The sheet is the frame with a band below it.
  const std::vector<uint8_t> px(size_t{64} * 32 * 4, 128);
  const ge::capture::SheetPixels sheet = g.shot.compose(64, 32, px, notes);
  CHECK(sheet.width == 64 && sheet.height > 32);
}
