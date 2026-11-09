// LAB `pacing`: each way a window can pace its frames, run against a model of a display, and
// measured for what a player feels: the LATENCY (from the moment a frame read its input to the
// refresh that starts showing it) and the JUDDER (how far each shown picture's moment in the game
// strays from the moment it is shown: a picture repeated, two refreshes' motion in one, or a
// picture sampled early or late in its refresh). The display refreshes at a fixed rate (or, for
// the variable-refresh row, shows a frame when it lands, no sooner than its fastest rate); the
// frame's CPU and GPU times vary by a seeded spread. Every row reuses loop/pacing.hpp's grid for
// its cap, so the lab measures the pacer the window runs.
//
// THE ROWS: vsync with the swap chain's three images and nothing else (how the window ran before
// the pacing settings: Metal's default), vsync with two images and one GPU frame in flight
// (`vid_framelatency 1`), the same with the game's time in whole refreshes (`vid_steady 1`), two in
// flight and steady (the new default), two in flight over three images (the gate alone does not
// stop finished frames queueing for the display), vsync with a cap at the refresh (`cl_maxfps_hz
// 1`, which a display's clock drifting from the CPU's undoes), vsync off uncapped (`vid_vsync 0`:
// the newest finished frame shown at each refresh, as a desktop's compositor does), and variable
// refresh capped 3% under the panel's top rate (the competitive recommendation where the display
// has it). The panel's true refresh drifts from the reported one by `--drift`. Two loads: a
// light one (a classic map on a fast machine) and a heavy one, whose CPU and GPU together take
// longer than a refresh, where one frame in flight costs frame rate.
//
// THE EYE: a server stepping at 1/77 s under a 120 Hz display. A view placed at the server's
// latest state moves 0 or 1 server steps a refresh (stairs: judder); placed one server step
// behind and blended, it moves the same distance every refresh.
//
// Deterministic (a model on the seeded stream): every number is in the digest.
//
// ge:summary Lab: pacing strategies on a display model: latency, judder, repeats and drops
// ge:keywords lab pacing frame-pacing vsync vrr latency judder frame-cap maxfps refresh promotion
// ge:keywords swap-chain frames-in-flight mailbox fifo compositor interpolation server-tick
// ge:skill platform perf labs
// ge:test tests/loop_pacing_test.cpp
#include <algorithm>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "core/prng.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/stats.hpp"
#include "loop/pacing.hpp"

namespace {

namespace gl = ge::lab;
using ge::core::num::to_fixed;

enum class Show : uint8_t { fifo, latest, vrr };

struct Strategy {
  std::string name;
  Show show;
  int images;       // the swap chain's images (fifo and vrr block on them)
  int gate;         // GPU frames in flight before the next frame reads input; 0: no gate
  double cap_mult;  // the cap as a multiple of the refresh; 0: none
  bool steady;      // the game's time in whole refreshes (loop::SteadyClock)
};

struct Load {
  std::string name;
  double cpu_ms, gpu_ms;
};

struct Outcome {
  std::vector<double> latency_ms, judder_ms;
  int frames = 0, shown = 0, repeats = 0, dropped = 0;
  double seconds = 0;
};

struct Queued {
  int64_t ready, sample;  // when the GPU finished it; when it read its input
  double game;            // its moment in the game, ns
};

constexpr int64_t kMs = 1'000'000;
constexpr int64_t kPresentNs = kMs / 10;  // the present call's own cost

Outcome run_model(const Strategy& s, const Load& load, double refresh_hz, double drift,
                  double spread, double seconds, uint32_t seed) {
  Outcome o;
  o.seconds = seconds - 0.25;  // the counted span: the settling quarter second left out
  // The panel's true period: the refresh the platform reports, off by `drift` (a display's clock
  // is never the CPU's). The cap and the steady clock know only the reported one.
  const int64_t period = ge::loop::interval_ns(refresh_hz * (1 - drift));
  const int64_t end = static_cast<int64_t>(seconds * 1e9);
  ge::core::Rng rng = ge::core::make_rng(seed);
  std::deque<Queued> queue;
  std::vector<int64_t> gpu_done;
  int64_t t = 0, gpu_free = 0, next_scan = period, last_scan = -period;
  bool shown_any = false;
  int64_t prev_scan = 0;
  Queued prev{};
  const int64_t settle = 250 * kMs;  // the first quarter second fills the queues: not counted
  const auto record = [&](int64_t at, const Queued& q, bool fresh) {
    if (at < settle) {
      shown_any = true;
      prev_scan = at;
      prev = q;
      return;
    }
    if (shown_any) {
      const double d = (q.game - prev.game) - static_cast<double>(at - prev_scan);
      o.judder_ms.push_back((d < 0 ? -d : d) / 1e6);
      if (!fresh) o.repeats++;
    }
    if (fresh) o.shown++;
    o.latency_ms.push_back(static_cast<double>(at - q.sample) / 1e6);
    shown_any = true;
    prev_scan = at;
    prev = q;
  };
  // Every refresh (or variable-refresh scan) up to `until`.
  const auto advance = [&](int64_t until) {
    if (s.show == Show::vrr) {
      while (!queue.empty() && std::max(queue.front().ready, last_scan + period) <= until) {
        const int64_t at = std::max(queue.front().ready, last_scan + period);
        record(at, queue.front(), true);
        queue.pop_front();
        last_scan = at;
      }
      return;
    }
    while (next_scan <= until) {
      const int64_t v = next_scan;
      if (s.show == Show::fifo) {
        if (!queue.empty() && queue.front().ready <= v) {
          record(v, queue.front(), true);
          queue.pop_front();
        } else if (shown_any) {
          record(v, prev, false);
        }
      } else {  // latest: the newest finished frame, the older finished ones dropped
        int ready = 0;
        while (ready < static_cast<int>(queue.size()) &&
               queue[static_cast<size_t>(ready)].ready <= v)
          ready++;
        if (ready > 0) {
          record(v, queue[static_cast<size_t>(ready - 1)], true);
          o.dropped += ready - 1;
          queue.erase(queue.begin(), queue.begin() + ready);
        } else if (shown_any) {
          record(v, prev, false);
        }
      }
      next_scan += period;
    }
  };
  ge::loop::FramePacer grid;
  ge::loop::SteadyClock steady;
  const double cap = s.cap_mult * refresh_hz;
  double game = 0;
  int64_t last_sample = 0;
  for (int k = 0; t < end; k++) {
    if (s.gate > 0 && k - s.gate >= 0) t = std::max(t, gpu_done[static_cast<size_t>(k - s.gate)]);
    advance(t);
    t = std::max(t, grid.start_at(t, cap));
    advance(t);
    const int64_t sample = t;
    const double dt = static_cast<double>(sample - last_sample) / 1e9;
    game += 1e9 * (s.steady && k > 0 ? steady.step(dt, 1 / refresh_hz) : dt);
    last_sample = sample;
    const double cpu = load.cpu_ms * (1 + spread * (2 * ge::core::next(rng) - 1));
    const double gpu = load.gpu_ms * (1 + spread * (2 * ge::core::next(rng) - 1));
    t += static_cast<int64_t>(cpu * 1e6);
    advance(t);
    // The acquire: a free image (one is on the display, the queued ones hold theirs).
    if (s.show != Show::latest) {
      while (1 + static_cast<int>(queue.size()) >= s.images) {
        int64_t at;
        if (s.show == Show::vrr) {
          at = std::max(queue.front().ready, last_scan + period);
        } else {
          at = next_scan;
          while (queue.front().ready > at) at += period;
        }
        t = std::max(t, at);
        advance(t);
      }
    }
    const int64_t start = std::max(t, gpu_free);
    const int64_t done = start + static_cast<int64_t>(gpu * 1e6);
    gpu_free = done;
    gpu_done.push_back(done);
    queue.push_back({done, sample, game});
    o.frames++;
    t += kPresentNs;
    advance(t);
  }
  return o;
}

std::vector<Strategy> strategies() {
  return {
      {"vsync, 3 images (before)", Show::fifo, 3, 0, 0, false},
      {"vsync, 2 images, 1 in flight", Show::fifo, 2, 1, 0, false},
      {"vsync, 1 in flight, steady", Show::fifo, 2, 1, 0, true},
      {"vsync, 2 in flight, steady (default)", Show::fifo, 2, 2, 0, true},
      {"vsync, 3 images, 2 in flight", Show::fifo, 3, 2, 0, true},
      {"vsync + cap at refresh", Show::fifo, 2, 1, 1, false},
      {"vsync off, uncapped", Show::latest, 3, 1, 0, false},
      {"VRR, cap 3% under refresh", Show::vrr, 2, 1, 0.97, false},
  };
}

std::string key_of(const std::string& name) {
  std::string k;
  for (const char c : name) {
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
      k += c;
    } else if (c >= 'A' && c <= 'Z') {
      k += static_cast<char>(c - 'A' + 'a');
    } else if (!k.empty() && k.back() != '-') {
      k += '-';
    }
  }
  while (!k.empty() && k.back() == '-') k.pop_back();
  return k;
}

void shoot(gl::Lab& lab, const std::string& id, const gl::Chart& chart, const std::string& caption,
           const std::string& look_for) {
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({id,
            chart.title,
            caption,
            look_for,
            {{"refresh", lab.arg("refresh")},
             {"light", lab.arg("light")},
             {"heavy", lab.arg("heavy")},
             {"spread", lab.arg("spread")},
             {"drift", lab.arg("drift")},
             {"seconds", lab.arg("seconds")},
             {"seed", lab.arg("seed")}}},
           gl::to_picture(chart.plot));
}

void run_load(gl::Lab& lab, const Load& load, double refresh, double drift, double spread,
              double seconds, uint32_t seed) {
  gl::LabTable t{"Pacing under the " + load.name + " load (CPU " + to_fixed(load.cpu_ms, 1) +
                     " ms, GPU " + to_fixed(load.gpu_ms, 1) + " ms, +-" +
                     to_fixed(spread * 100, 0) + "%) on a " + to_fixed(refresh, 0) + " Hz display",
                 {"strategy", "drawn/s", "new pictures/s", "repeats/s", "dropped/s",
                  "latency mean ms", "latency p99 ms", "judder mean ms", "judder p99 ms"},
                 {}};
  std::vector<gl::Sample> lat, jud;
  for (const Strategy& s : strategies()) {
    const Outcome o = run_model(s, load, refresh, drift, spread, seconds, seed);
    const gl::Summary l = gl::summarize(o.latency_ms), j = gl::summarize(o.judder_ms);
    t.rows.push_back({s.name, to_fixed(o.frames / seconds, 1), to_fixed(o.shown / o.seconds, 1),
                      to_fixed(o.repeats / o.seconds, 1), to_fixed(o.dropped / o.seconds, 1),
                      to_fixed(l.mean, 2), to_fixed(l.p99, 2), to_fixed(j.mean, 2),
                      to_fixed(j.p99, 2)});
    const std::string key = load.name + "-" + key_of(s.name);
    lab.metric(key + "-latency-mean", l.mean, "ms");
    lab.metric(key + "-judder-mean", j.mean, "ms");
    lab.metric(key + "-repeats", o.repeats / o.seconds, "per-s");
    lat.push_back({s.name, o.latency_ms});
    jud.push_back({s.name, o.judder_ms});
  }
  lab.table(std::move(t));
  shoot(lab, load.name + "-latency",
        gl::box_chart({"INPUT TO PICTURE, EACH WAY OF PACING (" + load.name + " load)",
                       to_fixed(refresh, 0) + " Hz display", "milliseconds", ""},
                      lat),
        "From the moment a frame read its input to the refresh that starts showing it, every "
        "refresh, one box a strategy. The swap chain's queue is the lag vsync adds; the gate and "
        "two images take it away.",
        "the before row the highest; two images and one frame in flight about a refresh lower; "
        "vsync off and VRR the lowest");
  shoot(lab, load.name + "-judder",
        gl::box_chart({"JUDDER, EACH WAY OF PACING (" + load.name + " load)",
                       to_fixed(refresh, 0) + " Hz display", "milliseconds", ""},
                      jud),
        "How far each shown picture's moment in the game strays from the time between refreshes: "
        "0 is perfectly even motion; a repeated picture or a picture sampled at a different point "
        "of its refresh shows as a spread.",
        "vsync rows near zero when the frame fits a refresh; vsync off uncapped a spread of up to "
        "a frame's time; the heavy load's one-in-flight row repeating pictures");
}

void run_eye(gl::Lab& lab, double refresh) {
  const double tic = 1.0 / lab.arg_num("tic-hz"), speed = 320;  // units a second: a run
  gl::LineChart c;
  c.frame = {"THE EYE'S STEP EACH REFRESH: SERVER STATE AGAINST BLENDED ONE STEP BEHIND",
             to_fixed(1 / tic, 0) + " Hz server, " + to_fixed(refresh, 0) + " Hz display, " +
                 "running at 320 units/s",
             "refresh", "units moved since the last refresh"};
  c.points = true;
  c.spikes = false;
  gl::Trace stepped{"server state", {}, {}}, blended{"blended, one step behind", {}, {}};
  double last_s = 0, last_b = 0;
  double stairs = 0;
  for (int i = 0; i <= 48; i++) {
    const double t = i / refresh;
    // The latest whole server step, and the place one step behind (exact under a constant run).
    const double steps = static_cast<double>(static_cast<int64_t>(t / tic));
    const double s = steps * tic * speed, b = (t > tic ? t - tic : 0) * speed;
    if (i > 0) {
      stepped.x.push_back(i);
      stepped.y.push_back(s - last_s);
      blended.x.push_back(i);
      blended.y.push_back(b - last_b);
      const double d = (s - last_s) - speed / refresh;
      stairs += d < 0 ? -d : d;
    }
    last_s = s;
    last_b = b;
  }
  lab.metric("eye-stepped-error-mean", stairs / 48, "units");
  c.traces = {stepped, blended};
  shoot(lab, "eye", gl::line_chart(c),
        "The distance the player's eye moves between two refreshes while running straight: drawn "
        "at the server's latest step it moves 0 or 1 steps a refresh (judder you see as a "
        "shimmer); blended between the last two steps it moves the same each refresh.",
        "server-state dots alternating between 0 and about 4 units; blended dots a flat line");
}

void run(gl::Lab& lab) {
  const double refresh = lab.arg_num("refresh");
  const double spread = lab.arg_num("spread");
  const double seconds = lab.arg_num("seconds");
  const uint32_t seed = static_cast<uint32_t>(lab.arg_num("seed"));
  const std::vector<double> light = lab.arg_nums("light"), heavy = lab.arg_nums("heavy");
  if (refresh <= 0 || seconds <= 0.5 || light.size() != 2 || heavy.size() != 2) {
    lab.fail("--refresh must be positive, --seconds over 0.5, --light and --heavy two numbers each",
             "next: rerun with the defaults (make lab LAB=pacing).");
    return;
  }
  const double drift = lab.arg_num("drift");
  run_load(lab, {"light", light[0], light[1]}, refresh, drift, spread, seconds, seed);
  run_load(lab, {"heavy", heavy[0], heavy[1]}, refresh, drift, spread, seconds, seed);
  run_eye(lab, refresh);
}

const gl::LabSpec kSpec{
    "pacing",
    gl::LabKind::sim,
    "engine",
    "engine/loop/pacing.hpp, engine/platform/pacing.hpp, engine/gpu/frame_gate.hpp",
    "platform",
    "Which way of pacing a window's frames gives the least lag and the evenest motion on a given "
    "display, under a light load and a heavy one?",
    {
        {"refresh", "120", "the display's refresh, Hz (120: a ProMotion panel)"},
        {"light", "2.5,3", "the light load's CPU and GPU time a frame, ms"},
        {"heavy", "4.5,5", "the heavy load's CPU and GPU time a frame, ms"},
        {"spread", "0.3", "each frame's time varies by up to this share either way"},
        {"drift", "0.0005", "the panel's true refresh this share slower than the one reported"},
        {"seconds", "10", "how long each strategy runs"},
        {"tic-hz", "77", "the server's step rate for the eye's chart"},
        {"seed", "1", "the frame times' stream"},
    },
    "--seconds=2",
    run,
};
GE_LAB(kSpec);

}  // namespace
