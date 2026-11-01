// LAB `se-demo`: WHAT A RECORDED GAME HOLDS, without a GPU. A demo of any format (client/demo.hpp)
// read to its end the way the window plays it: by default a multiview recording written in code
// (demo_write.hpp: the scripted walk round the halls and a second player standing), or a real
// file with `--file=<path>` (plain or gzip). `--track=<slot>` picks the player the view rides.
//
// What it shows: the file (its format, protocol, map, records, frames, the demo's clock, the
// records no client sees), a table of every player seen (frames written, the ground covered, the
// health he fell to and how often he was down), every player's path from above (a point every
// tenth frame), and every player's health and the view's own over the demo's clock. Everything
// but the read's wall time is deterministic.
//
// ge:summary Lab: a demo of any format read headless: its players' paths, health and counts
// ge:keywords lab se-demo demo multiview mvd recording players paths health track gzip
// ge:keywords demo-info frames records hidden
// ge:skill shooter labs
// ge:test tests/shooters_edge_demo_test.cpp
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_write.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
using ge::core::num::to_fixed;

constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
constexpr int kSlots = 32;

struct Seen {
  int64_t frames = 0;      // frames he was written in
  double ground = 0;       // units covered on the plan (jumps of over 200 units left out)
  int lowest = 1 << 30;    // the lowest health his stats reached
  int downs = 0;           // times his health fell to 0 or under
  int last_health = 1;     // to count the downs
  cl::DemoEntity last;     // his last frame
  gl::Trace path, health;  // a point every tenth frame he was in; his health by the clock
};

const char* format_name(cl::DemoFormat f) {
  return f == cl::DemoFormat::single   ? "single-player"
         : f == cl::DemoFormat::client ? "client"
                                       : "multiview";
}

void run(gl::Lab& lab) {
  const std::string file = lab.arg("file");
  std::vector<uint8_t> bytes;
  if (file.empty()) {
    bytes = cl::write_multiview_demo(cl::loop_script("halls", {256, 192, 24}, 120, 600));
  } else {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
      lab.fail("se-demo: " + file + " does not open", "next: pass --file=<a demo's path>");
      return;
    }
    bytes.assign(std::istreambuf_iterator<char>(in), {});
  }
  const std::vector<std::pair<std::string, std::string>> in = {
      {"file", file.empty() ? "(written in code)" : file}, {"track", lab.arg("track")}};

  const auto t0 = std::chrono::steady_clock::now();
  cl::Demo d;
  std::string error;
  if (!cl::open_demo(std::move(bytes), d, error)) {
    lab.fail("se-demo: " + error, "next: check the file; skill: shooter");
    return;
  }
  d.track = static_cast<int>(lab.arg_num("track"));
  std::vector<Seen> seen(kSlots);
  gl::Trace view_health{"the view's health", {}, {}};
  cl::DemoRead r;
  int64_t frames = 0;
  while ((r = cl::next_frame(d, error)) == cl::DemoRead::frame) {
    const cl::DemoState& s = d.state;
    if (s.map.empty()) continue;
    frames++;
    const bool every_tenth = frames % 10 == 0;
    if (every_tenth) {
      view_health.x.push_back(s.time);
      view_health.y.push_back(s.stats[cl::kStatHealth]);
    }
    for (int k = 0; k < kSlots; k++) {
      const cl::DemoEntity& p = s.players[static_cast<size_t>(k)];
      Seen& w = seen[static_cast<size_t>(k)];
      if (!p.present) continue;
      if (w.frames > 0) {
        const double step =
            ge::core::hypot(p.origin.x - w.last.origin.x, p.origin.y - w.last.origin.y);
        if (step < 200) w.ground += step;  // a teleport or a respawn is not ground covered
      }
      w.frames++;
      w.last = p;
      if (static_cast<size_t>(k) < d.player_stats.size()) {
        const int hp = d.player_stats[static_cast<size_t>(k)][cl::kStatHealth];
        w.lowest = std::min(w.lowest, hp);
        if (w.last_health > 0 && hp <= 0) w.downs++;
        w.last_health = hp;
        if (every_tenth) {
          w.health.x.push_back(s.time);
          w.health.y.push_back(hp);
        }
      }
      if (every_tenth) {
        w.path.x.push_back(p.origin.x);
        w.path.y.push_back(p.origin.y);
      }
    }
  }
  const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  if (r == cl::DemoRead::error) {
    lab.fail("se-demo: the demo stopped at frame " + std::to_string(frames) + ": " + error,
             "next: read the reader the format names (client/demo_*.cpp); skill: shooter");
    return;
  }
  const cl::DemoState& s = d.state;

  gl::LabTable info{"the recording", {"what", "value"}, {}};
  info.rows = {{"format", format_name(s.format)},
               {"protocol", std::to_string(s.protocol)},
               {"map", s.map},
               {"levels", std::to_string(s.levels)},
               {"frames", std::to_string(frames)},
               {"demo clock s", to_fixed(s.time, 2)},
               {"bytes", std::to_string(d.bytes.size())}};
  if (s.format == cl::DemoFormat::multiview) {
    info.rows.push_back({"records", std::to_string(d.records)});
    info.rows.push_back({"records no client sees", std::to_string(d.hidden)});
    info.rows.push_back({"the view rides", "slot " + std::to_string(s.player)});
  }
  lab.table(info);
  lab.metric("demo.frames", static_cast<double>(frames), "count");
  lab.metric("demo.clock", s.time, "s");
  lab.metric("demo.records", static_cast<double>(d.records), "count");
  lab.metric("demo.hidden", static_cast<double>(d.hidden), "count");
  lab.metric("demo.read-wall", wall, "s", gl::Det::measured);
  lab.metric("demo.frames-per-second-read", static_cast<double>(frames) / std::max(wall, 1e-9),
             "1/s", gl::Det::measured);

  gl::LabTable players{"every player written",
                       {"slot", "frames", "ground u", "lowest health", "downs", "last origin"},
                       {}};
  gl::LineChart from_above, health;
  from_above.spikes = false;
  health.spikes = false;
  from_above.frame = {"THE PLAYERS FROM ABOVE",
                      format_name(s.format) + std::string(" recording on ") + s.map + ", " +
                          std::to_string(frames) + " frames",
                      "x (units)",
                      "y (units)",
                      960,
                      720};
  health.frame = {"HEALTH OVER THE DEMO'S CLOCK",
                  "each player's own stats; the view's is the tracked player's",
                  "t (s)",
                  "health",
                  960,
                  420};
  for (int k = 0; k < kSlots; k++) {
    Seen& w = seen[static_cast<size_t>(k)];
    if (w.frames == 0) continue;
    const std::string name = "slot " + std::to_string(k);
    players.rows.push_back({std::to_string(k), std::to_string(w.frames), to_fixed(w.ground, 0),
                            w.lowest == (1 << 30) ? "-" : std::to_string(w.lowest),
                            std::to_string(w.downs),
                            to_fixed(w.last.origin.x, 1) + " " + to_fixed(w.last.origin.y, 1) +
                                " " + to_fixed(w.last.origin.z, 1)});
    lab.metric("players." + std::to_string(k) + ".ground", w.ground, "u");
    lab.metric("players." + std::to_string(k) + ".downs", w.downs, "count");
    w.path.label = name;
    from_above.traces.push_back(w.path);
    if (!w.health.x.empty()) {
      w.health.label = name;
      health.traces.push_back(w.health);
    }
  }
  lab.table(players);
  lab.metric("players.seen", static_cast<double>(players.rows.size()), "count");
  if (players.rows.empty()) {
    lab.fail("se-demo: no player was written in the demo",
             "next: a single-player demo has no players; pass a multiplayer recording");
    return;
  }

  const gl::Chart above = gl::line_chart(from_above);
  lab.shot(
      {"from-above", above.title,
       "Every player the recording wrote, from above (x east, y north): his origin every "
       "tenth frame he was in, a line between them. A respawn or a teleport is a straight "
       "jump across the map.",
       "the lines trace the map's rooms and corridors; every slot the table lists has a line", in},
      gl::to_picture(above.plot));
  if (!health.traces.empty()) {
    view_health.y.push_back(kNan);
    view_health.x.push_back(s.time);
    health.traces.push_back(view_health);
    const gl::Chart hp = gl::line_chart(health);
    lab.shot({"health", hp.title,
              "Each player's health off his own stats records, every tenth frame, and the view's "
              "(the tracked player's, as the HUD reads it).",
              "a drop is a hit, a fall to 0 a death, a jump back to 100 the respawn; the view's "
              "line lies on the tracked player's",
              in},
             gl::to_picture(hp.plot));
  }
}

const gl::LabSpec kSpec{
    "se-demo",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/client: the demo reader (demo.hpp), every format, the multiview "
    "recording's players and their stats",
    "shooter",
    "What does a recorded game hold: does every frame read, who played, where did each player go "
    "and what happened to his health?",
    {
        {"file", "",
         "a demo's path (plain, packed or in an archive); empty: a recording written in code"},
        {"track", "-1",
         "the slot the view rides in a multiview recording (-1: the lowest playing)"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
