// LAB `spectate`: FALL LINE WATCHED BY INPUTS THROUGH A TV TREE. The bot skis a seed while its run
// is written as a replay file version 2 stream (engine/replay/spectate.hpp: one block a second of
// held inputs and the run's digest, a keyframe save every few seconds), and the stream goes out
// through a source's TV node and a relay to viewers on both (engine/tv/trial.hpp) under each
// simulated network preset. Each viewer steps its own run off the stream it got; the lab answers
// whether every viewer reached the player's digest at the player's last step, which block each
// started at, how late the blocks came, and how many bytes a second the broadcast costs: the
// inputs alone and the keyframes beside them. Deterministic: a virtual clock off one seed.
//
// ge:summary Lab: Fall Line spectated by inputs through a TV tree: digests, lag, bytes a second
// ge:keywords lab spectate spectating inputs tv relay digest keyframe bytes-per-second preset
// ge:skill replay sim labs
// ge:test tests/fall_line_spectate_test.cpp
#include <cstdio>
#include <string>
#include <vector>

#include "fall_line/sim/play.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/stats.hpp"
#include "net/conditions.hpp"
#include "play/session.hpp"
#include "replay/spectate.hpp"
#include "replay/stream.hpp"
#include "tv/trial.hpp"

namespace {

namespace gl = ge::lab;
namespace gp = ge::play;
namespace rp = ge::replay;
namespace fs = fall_line::sim;

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at <= s.size()) {
    const size_t comma = s.find(',', at);
    const std::string part =
        s.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
    if (!part.empty()) out.push_back(part);
    if (comma == std::string::npos) break;
    at = comma + 1;
  }
  return out;
}

void run(gl::Lab& lab) {
  gp::Setup setup;
  std::string error;
  if (!gp::make_setup(fs::kGame, lab.arg_num("seed"), {}, setup, error)) {
    lab.fail("the setup does not open: " + error, "next: pass a seed the game takes; skill: sim.");
    return;
  }
  gp::Session session(fs::kGame, setup);
  if (!session.ok()) {
    lab.fail("the run does not open: " + session.error(),
             "next: run make sim SEEDS=38; skill: sim.");
    return;
  }
  std::vector<uint8_t> stream;
  rp::SpectateOptions options;
  options.keyframe_seconds = static_cast<int>(lab.arg_num("keyframe-seconds"));
  options.full_every = static_cast<int>(lab.arg_num("full-every"));
  rp::SpectateWriter writer(fs::kGame, setup, stream, options);
  if (!writer.start(session.run(), error)) {
    lab.fail("the first keyframe does not save: " + error,
             "next: run make test T=play_saves; skill: save.");
    return;
  }
  std::vector<double> input(fs::kGame.axes.size(), 0), snapped(input.size(), 0);
  const auto steps = static_cast<int64_t>(lab.arg_num("seconds") * fs::kGame.hz);
  for (int64_t s = 0; s < steps && !session.done(); s++) {
    std::fill(input.begin(), input.end(), 0);
    session.run().pilot(input);
    for (size_t i = 0; i < input.size(); i++)
      snapped[i] = gp::snap_axis(input[i], fs::kGame.axes[i].kind);
    session.step(input);
    if (!writer.step(snapped, session.run(), error)) {
      lab.fail("a keyframe does not save: " + error,
               "next: run make test T=play_saves; skill: save.");
      return;
    }
  }
  writer.finish(session.run());
  const int64_t played = session.steps();
  const uint64_t digest = session.run().digest();
  const double seconds = static_cast<double>(played) / fs::kGame.hz;
  // The bytes by track.
  rp::StreamReader r;
  size_t inputs = 0, keys = 0, deltas = 0, header = 0;
  if (rp::read_stream_header(stream, r, error)) {
    header = r.at;
    rp::Block b;
    while (rp::next_block(r, b, error)) {
      for (const rp::Chunk& c : b.chunks)
        (c.track != rp::kKeyframeTrack ? inputs : b.keyframe ? keys : deltas) += c.bytes.size();
    }
  }
  gl::LabTable bytes{"the broadcast's bytes (plain, before each block's zstd) and on the wire",
                     {"part", "bytes", "bytes a second"},
                     {}};
  bytes.rows.push_back({"header", std::to_string(header), "-"});
  bytes.rows.push_back({"inputs and digests", std::to_string(inputs),
                        fmt(static_cast<double>(inputs) / seconds, 0)});
  bytes.rows.push_back({"whole keyframes (saves)", std::to_string(keys),
                        fmt(static_cast<double>(keys) / seconds, 0)});
  bytes.rows.push_back({"delta keyframes (XOR, plain)", std::to_string(deltas),
                        fmt(static_cast<double>(deltas) / seconds, 0)});
  bytes.rows.push_back({"the stream (packed)", std::to_string(stream.size()),
                        fmt(static_cast<double>(stream.size()) / seconds, 0)});
  lab.table(bytes);
  lab.metric("steps", static_cast<double>(played), "steps");
  lab.metric("stream-bytes-per-s", static_cast<double>(stream.size()) / seconds, "bytes/s");
  lab.metric("inputs-bytes-per-s", static_cast<double>(inputs) / seconds, "bytes/s");

  gl::LabTable table{
      "each preset: viewers through a source node and a relay, each stepping its own run",
      {"preset", "at the player's digest", "started at block", "lag p50 ms", "lag p95 ms"},
      {}};
  std::vector<gl::Sample> lags;
  int good = 0;
  const std::vector<std::string> presets = split(lab.arg("presets"));
  for (const std::string& name : presets) {
    ge::tv::TrialConfig c;
    if (!ge::net::conditions_preset(name, c.link)) {
      lab.fail("unknown preset '" + name + "'",
               "Pick from lan, broadband, wifi, mobile, bad, awful (engine/net/conditions.cpp); "
               "skill: net.");
      return;
    }
    c.source = &stream;
    c.keep_streams = true;
    c.block_ms = static_cast<int>(lab.arg_num("block-ms"));
    const ge::tv::TrialResult t = ge::tv::run_tv_trial(c);
    int at = 0;
    std::string starts;
    std::vector<double> all;
    for (const ge::tv::TrialViewer& v : t.viewers) {
      rp::Spectator s;
      rp::spectate_follow(s, fs::kGame, v.stream);
      at += s.step == played && s.digest == digest ? 1 : 0;
      starts += (starts.empty() ? "" : " ") + std::to_string(v.first);
      all.insert(all.end(), v.lag_ms.begin(), v.lag_ms.end());
    }
    const gl::Summary sum = gl::summarize(all);
    const int n = static_cast<int>(t.viewers.size());
    table.rows.push_back({name, std::to_string(at) + " of " + std::to_string(n), starts,
                          fmt(sum.p50, 0), fmt(sum.p95, 0)});
    lags.push_back({name, all});
    good += at == n ? 1 : 0;
    lab.metric(name + ".at-digest", at, "count");
  }
  lab.table(table);
  lab.metric("presets-at-digest", good, "count");
  const gl::ChartFrame frame{"HOW LATE EACH SECOND OF THE RUN REACHED EACH VIEWER",
                             "Fall Line, seed " + lab.arg("seed") + ", " + fmt(seconds, 0) +
                                 " s of the bot; a source, a relay and 3 viewers",
                             "", "lag after the player wrote it (ms)"};
  lab.shot(
      {"lag",
       frame.title,
       "Each preset's one-second blocks of held inputs, from the moment the player's run wrote "
       "one to the moment a viewer had it whole and could step its own run on.",
       "every preset's viewers at the player's digest (the table); the boxes rising with loss",
       {{"seed", lab.arg("seed")}, {"seconds", lab.arg("seconds")}}},
      gl::to_picture(gl::box_chart(frame, lags).plot));
  if (good != static_cast<int>(presets.size()))
    lab.fail(
        "a viewer's run did not reach the player's digest",
        "next: run make test T=fall_line_spectate and read which preset parted; skill: replay.");
}

const gl::LabSpec kSpec{
    "spectate",
    gl::LabKind::sim,
    "game:fall_line",
    "engine/replay/spectate.hpp (a run as a stream of held inputs and keyframes) through engine/tv",
    "replay",
    "Does every viewer of Fall Line through a TV tree step its own run to the player's digest "
    "under "
    "every network preset, and how many bytes a second does it cost?",
    {
        {"seed", "38", "the mountain"},
        {"seconds", "30", "how long the bot skis"},
        {"keyframe-seconds", "10", "a keyframe save every this many seconds"},
        {"full-every", "30", "every this many keyframes is a whole save; the rest are deltas"},
        {"presets", "lan,broadband,wifi,mobile,bad,awful", "condition presets, comma-separated"},
        {"block-ms", "100", "the clock between blocks sent (1000: the run's own speed)"},
    },
    "--seconds=6 --keyframe-seconds=2 --presets=lan,bad",
    run,
};
GE_LAB(kSpec);

}  // namespace
