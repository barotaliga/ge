// LAB `tv`: A MATCH BROADCAST THROUGH A TREE OF TV NODES (engine/tv). A made-up match's replay
// stream goes out from the source's node, through a relay under it, to viewers on both, over the
// simulated network under each link preset (engine/tv/trial.hpp). For each preset it answers
// whether every viewer's stream came out whole (the header and every block from the keyframe it
// started at), when each viewer started, how late each block reached each viewer after the source
// wrote it, the bytes a second each viewer's link carried, and whether a line said at the end of
// the tree and the viewer count reached everyone. The lags are drawn as a box per preset. It all
// runs on a virtual clock off one seed, so the lab is a determinism test too. With
// `--source=sample` the match is the server core's sample game broadcast as it runs
// (engine/tv/server_source.hpp), and each viewer also keeps the ring off its stream and is checked
// against the server's digest.
//
// ge:summary Lab: a match broadcast through a TV tree over each network preset: whole, lag, bytes
// ge:keywords lab tv television broadcast relay viewers lag delay bytes-per-second preset tree
// ge:skill replay net labs
// ge:test tests/tv_net_test.cpp
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/stats.hpp"
#include "net/conditions.hpp"
#include "replay/stream.hpp"
#include "server/sample.hpp"
#include "server/server.hpp"
#include "tv/server_source.hpp"
#include "tv/trial.hpp"

namespace {

namespace gl = ge::lab;
namespace tv = ge::tv;

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

// The sample game's broadcast over `seconds` of its frames; its digest and frames at the end.
bool sample_stream(int seconds, uint32_t seed, std::vector<uint8_t>& out, uint64_t& digest,
                   int64_t& frames, std::string& error) {
  ge::server::ServerConfig c;
  c.map = "ring";
  c.bots = 12;
  c.maxclients = 16;
  c.seed = seed;
  c.offline = true;
  ge::server::Server s;
  if (!ge::server::server_open(s, ge::server::sample_game(), c, nullptr, nullptr, error))
    return false;
  tv::ServerSource src;
  src.keyframe_seconds = 5;
  if (!tv::server_source_begin(src, s, error)) return false;
  for (int f = 0; f < seconds * 60; f++) {
    ge::server::server_frame(s);
    tv::server_source_frame(src, s);
  }
  tv::server_source_finish(src, s);
  digest = ge::server::server_digest(s);
  frames = s.frame;
  out = std::move(src.bytes);
  ge::server::server_close(s);
  return true;
}

// A viewer's stream kept as the sample game's ring: at the server's digest and last frame.
bool at_digest(const std::vector<uint8_t>& stream, uint64_t digest, int64_t frames) {
  ge::server::SampleWatch w;
  ge::replay::StreamReader r;
  std::string error;
  if (!ge::replay::read_stream_header(stream, r, error)) return false;
  ge::replay::Block b;
  while (ge::replay::next_block(r, b, error)) {
    for (const ge::replay::Chunk& c : b.chunks) {
      const bool ok = c.track == tv::kServerKeyframeTrack
                          ? ge::server::sample_watch_keyframe(w, c.bytes)
                          : ge::server::sample_watch_frame(w, c.bytes);
      if (!ok) return false;
    }
  }
  return w.frames == frames && ge::server::sample_watch_hash(w) == digest;
}

void run(gl::Lab& lab) {
  std::vector<uint8_t> sample;
  uint64_t digest = 0;
  int64_t frames = 0;
  const bool from_sample = lab.arg("source") == "sample";
  if (!from_sample && lab.arg("source") != "made-up") {
    lab.fail("unknown source '" + lab.arg("source") + "'",
             "next: pass --source=made-up or sample.");
    return;
  }
  if (from_sample) {
    std::string error;
    if (!sample_stream(static_cast<int>(lab.arg_num("blocks")),
                       static_cast<uint32_t>(lab.arg_num("seed")), sample, digest, frames, error)) {
      lab.fail("the sample game's broadcast did not start: " + error,
               "next: run make test T=tv_server_source; skill: replay.");
      return;
    }
    lab.metric("sample-stream-bytes-per-s",
               static_cast<double>(sample.size()) / lab.arg_num("blocks"), "bytes/s");
  }
  tv::TrialConfig base;
  base.blocks = static_cast<int>(lab.arg_num("blocks"));
  base.block_ms = static_cast<int>(lab.arg_num("block-ms"));
  base.viewers_direct = static_cast<int>(lab.arg_num("direct"));
  base.viewers_relayed = static_cast<int>(lab.arg_num("relayed"));
  base.delay_ms = static_cast<int64_t>(lab.arg_num("delay-ms"));
  base.seed = static_cast<uint32_t>(lab.arg_num("seed"));
  gl::LabTable table{
      "each preset: the tree of a source, a relay and viewers on both",
      {"preset", "viewers whole", "started at block", "lag p50 ms", "lag p95 ms", "lag max ms",
       "down kB a match second", "heard the chat", "count told", "at the server's digest"},
      {}};
  std::vector<gl::Sample> lags;
  int whole_presets = 0;
  const std::vector<std::string> presets = split(lab.arg("presets"));
  for (const std::string& name : presets) {
    tv::TrialConfig c = base;
    if (!ge::net::conditions_preset(name, c.link)) {
      lab.fail("unknown preset '" + name + "'",
               "Pick from lan, broadband, wifi, mobile, bad, awful (engine/net/conditions.cpp); "
               "skill: net.");
      return;
    }
    if (from_sample) {
      c.source = &sample;
      c.keep_streams = true;
    }
    const tv::TrialResult r = tv::run_tv_trial(c);
    int whole = 0, heard = 0, digested = 0;
    std::string starts;
    std::vector<double> all;
    double down = 0;
    int64_t told = -1;
    for (const tv::TrialViewer& v : r.viewers) {
      whole += v.whole ? 1 : 0;
      heard += v.chat.empty() ? 0 : 1;
      starts += (starts.empty() ? "" : " ") + std::to_string(v.first);
      all.insert(all.end(), v.lag_ms.begin(), v.lag_ms.end());
      down += v.bytes_per_s / 1000.0 / static_cast<double>(r.viewers.size());
      told = told < 0 || v.told < told ? v.told : told;
      digested += from_sample && at_digest(v.stream, digest, frames) ? 1 : 0;
    }
    const gl::Summary s = gl::summarize(all);
    const int n = static_cast<int>(r.viewers.size());
    table.rows.push_back(
        {name, std::to_string(whole) + " of " + std::to_string(n), starts, fmt(s.p50, 0),
         fmt(s.p95, 0), fmt(s.max, 0), fmt(down, 1),
         std::to_string(heard) + " of " + std::to_string(n), std::to_string(told),
         from_sample ? std::to_string(digested) + " of " + std::to_string(n) : "-"});
    if (from_sample) lab.metric(name + ".at-digest", digested, "count");
    lags.push_back({name, all});
    whole_presets += whole == n ? 1 : 0;
    lab.metric(name + ".viewers-whole", whole, "count");
    lab.metric(name + ".lag-p50-ms", s.p50, "ms");
    lab.metric(name + ".down-bytes-per-s", down * 1000.0, "bytes/s");
    lab.metric(name + ".count-told", static_cast<double>(told), "count");
  }
  lab.table(table);
  lab.metric("presets-whole", whole_presets, "count");
  const gl::ChartFrame frame{
      "HOW LATE EACH BLOCK REACHED EACH VIEWER",
      "a source, a relay and " + std::to_string(base.viewers_direct + base.viewers_relayed) +
          " viewers; a block every " + lab.arg("block-ms") + " ms; seed " + lab.arg("seed"),
      "", "lag after the source wrote it (ms)"};
  lab.shot({"lag",
            frame.title,
            "Each preset's blocks, from the moment the source wrote one to the moment a viewer had "
            "it whole: through the source's node for some viewers, and through the relay for the "
            "rest.",
            "the boxes low on a clean link and rising with the loss; no preset's viewers missing",
            {{"presets", lab.arg("presets")}, {"seed", lab.arg("seed")}}},
           gl::to_picture(gl::box_chart(frame, lags).plot));
  if (whole_presets != static_cast<int>(presets.size()))
    lab.fail("a viewer's stream was not whole under a preset",
             "next: read the table's 'viewers whole' column and run that preset's trial in "
             "tests/tv_net_test.cpp; skill: replay.");
}

const gl::LabSpec kSpec{
    "tv",
    gl::LabKind::sim,
    "engine",
    "engine/tv (the TV node, relays, the net link, the trial)",
    "replay",
    "Through a source's TV node and a relay, does every viewer get the whole match under every "
    "network preset, how late, and at how many bytes a second?",
    {
        {"presets", "lan,broadband,wifi,mobile,bad,awful", "condition presets, comma-separated"},
        {"blocks", "40", "the match's one-second blocks"},
        {"block-ms", "100", "the clock between the source's blocks (1000: the match's own speed)"},
        {"direct", "2", "viewers on the source's node"},
        {"relayed", "4", "viewers under the relay"},
        {"delay-ms", "0", "the source node's delay"},
        {"seed", "7", "the network's and the nodes' stream"},
        {"source", "made-up", "made-up (a made-up match) or sample (the server's sample game)"},
    },
    "--presets=lan,bad --blocks=12",
    run,
};
GE_LAB(kSpec);

}  // namespace
