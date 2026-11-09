// The window's frame pacing (pacing.hpp).
//
// ge:summary The pacing cvars, the cap's wait on the clock, framestats
// ge:keywords pacing framestats add_pacing_cvars
// ge:skill platform
#include "platform/pacing.hpp"

#include "console/console.hpp"
#include "core/num.hpp"
#include "core/num_text.hpp"

namespace ge::platform {

void add_pacing_cvars(console::Console& con, Pacing& p) {
  using console::CvarKind;
  using console::kArchive;
  con.cvars().bind({.name = "vid_vsync",
                    .fallback = "1",
                    .flags = kArchive,
                    .help = "1: present at the display's refresh (no tearing); 0: as soon as drawn "
                            "(the least lag, tearing where the platform allows it)",
                    .kind = CvarKind::boolean},
                   &p.settings.vsync);
  con.cvars().bind({.name = "cl_maxfps",
                    .fallback = "0",
                    .flags = kArchive,
                    .help = "the most frames a second, held on an even grid; 0: no cap",
                    .kind = CvarKind::number,
                    .min = 0,
                    .max = 10000},
                   &p.settings.max_fps);
  con.cvars().bind({.name = "cl_maxfps_hz",
                    .fallback = "0",
                    .flags = kArchive,
                    .help = "the cap as a multiple of the display's refresh (1: the refresh, 2: "
                            "twice it, 0.5: half); wins over cl_maxfps; 0: off",
                    .kind = CvarKind::number,
                    .min = 0,
                    .max = 16},
                   &p.settings.hz_mult);
  con.cvars().bind({.name = "vid_framelatency",
                    .fallback = "2",
                    .flags = kArchive,
                    .help = "frames the GPU may have queued when a frame reads its input, 1..3: 1 "
                            "the least lag while CPU and GPU together fit a refresh; 2 (the "
                            "default) no missed refresh until either alone passes one; 3 the "
                            "swap chain's third image too",
                    .kind = CvarKind::integer,
                    .min = loop::kMinLatency,
                    .max = loop::kMaxLatency},
                   &p.settings.latency);
  con.cvars().bind({.name = "vid_steady",
                    .fallback = "1",
                    .flags = kArchive,
                    .help = "1: at vsync the game moves in whole refreshes, so motion is even "
                            "whenever a frame starts; 0: by the measured time between frames",
                    .kind = CvarKind::boolean},
                   &p.settings.steady);
  con.cvars().bind({.name = "vid_vsync_lag_fix",
                    .fallback = "0",
                    .flags = kArchive,
                    .help = "1: with vsync, each frame starts just in time to finish before its "
                            "refresh, its input that much fresher (for frames well under a "
                            "refresh; a slower frame runs as at vid_framelatency 1); 0: frames "
                            "start as soon as they may",
                    .kind = CvarKind::boolean},
                   &p.settings.late_start);
  con.cvars().bind({.name = "vid_vsync_lag_tweak",
                    .fallback = "1",
                    .flags = kArchive,
                    .help = "the time vid_vsync_lag_fix leaves spare before the refresh, ms: "
                            "lower is less lag, too low misses refreshes",
                    .kind = CvarKind::number,
                    .min = 0,
                    .max = 10},
                   &p.settings.late_margin_ms);
  con.cvars().bind({.name = "scr_showfps",
                    .fallback = "0",
                    .flags = kArchive,
                    .help = "1: the frame rate in a corner; 2: with the last frames' times as a "
                            "graph (the refresh's interval as a line); 0: off",
                    .kind = CvarKind::integer,
                    .min = 0,
                    .max = 2},
                   &p.show);
  con.add_command("framestats",
                  "the last frames' pacing in one line (rate, percentiles, jitter, hitches, "
                  "waits); `framestats clear` starts the record again",
                  [&p](console::Console& c, const console::Args& a) {
                    if (a.size() > 1 && a[1] == "clear") {
                      p.times.clear();
                      c.print("framestats: the record is empty; play a while, then framestats");
                      return;
                    }
                    c.print(pacing_text(p));
                  });
}

uint64_t pace(const Window& window, Pacing& p, double gpu_wait_s) {
  p.refresh = refresh_hz(window);
  p.cap = loop::cap_hz(p.settings, p.refresh);
  const uint64_t before = now_ns();
  const int64_t at = p.grid.start_at(static_cast<int64_t>(before), p.cap);
  if (at > static_cast<int64_t>(before)) wait_until_ns(static_cast<uint64_t>(at));
  if (late_start_on(p)) {  // then just in time for the refresh
    const int64_t now = static_cast<int64_t>(now_ns());
    const int64_t late =
        p.phase.start_at(now, loop::interval_ns(p.refresh),
                         static_cast<int64_t>(core::num::round(p.settings.late_margin_ms * 1e6)));
    if (late > now) wait_until_ns(static_cast<uint64_t>(late));
  }
  const uint64_t start = now_ns();
  if (p.last_ns != 0)
    p.times.add(static_cast<double>(start - p.last_ns) * 1e-9,
                static_cast<double>(start - before) * 1e-9, gpu_wait_s);
  p.last_ns = start;
  return start;
}

void acquired(Pacing& p, uint64_t before_ns, uint64_t after_ns) {
  p.acquire_ns = after_ns - before_ns;
  if (p.acquire_ns > 500'000) p.phase.blank(static_cast<int64_t>(after_ns));  // it blocked
}

bool late_start_on(const Pacing& p) {
  return p.settings.vsync && p.settings.late_start && p.refresh > 0;
}

void frame_done(Pacing& p, uint64_t done_ns) {
  // From the frame's start to its GPU's last work, less the wait for a free image (the display's).
  if (p.last_ns != 0 && done_ns > p.last_ns + p.acquire_ns)
    p.phase.cost(static_cast<int64_t>(done_ns - p.last_ns - p.acquire_ns));
}

double game_dt(Pacing& p, double dt) {
  const bool on = p.settings.vsync && p.settings.steady && p.refresh > 0;
  return p.steady.step(dt, on ? 1 / p.refresh : 0);
}

std::string pacing_text(const Pacing& p) {
  using core::num::to_fixed;
  std::string line = "framestats: " + loop::summary_text(p.times.summarize());
  line += "; display " + (p.refresh > 0 ? to_fixed(p.refresh, 2) + " Hz" : std::string("? Hz"));
  line += ", cap " + (p.cap > 0 ? to_fixed(p.cap, 2) + " fps" : std::string("none"));
  line += ", vsync " + std::string(p.settings.vsync ? "on" : "off");
  if (!p.present.empty()) line += " (" + p.present + ")";
  line += ", framelatency " + std::to_string(p.settings.latency);
  line += ", steady " + std::string(p.settings.steady ? "on" : "off");
  if (late_start_on(p))
    line += ", lag fix (frame " + to_fixed(static_cast<double>(p.phase.worst()) / 1e6, 2) +
            " ms + " + to_fixed(p.settings.late_margin_ms, 2) + " ms spare)";
  return line;
}

}  // namespace ge::platform
