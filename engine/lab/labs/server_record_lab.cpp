// LAB `server-record`: A MATCH RECORDED BY THE SERVER, PLAYED BACK FROM ANY PLAYER'S VIEW AND
// WATCHED THROUGH A RELAY WITH A DELAY (engine/server/record.hpp, playback.hpp; docs/servers.md §
// Recording). The server core's sample game runs with its recorder on. Every player's view at every
// frame is kept off the live game (`ServerGame::player_view`); the recording is then played back
// and each player's view held to it, frame by frame from the start and after a seek into the middle
// of every second. The same recording is broadcast at the match's own speed through a source's TV
// node holding it back by the delay, a relay under it, and viewers on both, under every simulated
// network preset (engine/tv/trial.hpp): every viewer's stream must come out whole, at the server's
// digest, and no block may reach a viewer sooner than the delay after the server wrote it.
//
// The pictures: each player's view over the match as the playback gave it (a column per cell
// around the player, a row per tenth of a second, a dot per other player it sees), and the lag of
// each block to each viewer per preset beside the delay. Then the server's own TV (spectators.hpp):
// the match served live with spectators from the start and one coming halfway, each at the
// server's digest, no second before the delay, under every preset.
//
// ge:summary Lab: a server's recording played back from every player's view, relayed with a delay
// ge:keywords lab server-record recording playback player-view seek relay delay preset tv
// ge:skill replay net labs
// ge:test tests/server_record_test.cpp
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "lab/stats.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "replay/stream.hpp"
#include "server/playback.hpp"
#include "server/record.hpp"
#include "server/sample.hpp"
#include "server/server.hpp"
#include "server/spectators.hpp"
#include "tv/net_link.hpp"
#include "tv/trial.hpp"

namespace {

namespace gl = ge::lab;
namespace sv = ge::server;
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

// The sample game's view (server/sample.hpp): the player's cell, then (number, offset) pairs, the
// offset folded to unsigned. Returns the offsets of the others it sees and their numbers.
struct Seen {
  int number = 0;
  int offset = 0;
};
std::vector<Seen> read_view(const std::vector<uint8_t>& v) {
  std::vector<Seen> out;
  size_t at = 0;
  const auto get = [&](uint64_t& x) {
    x = 0;
    for (int shift = 0; shift < 64 && at < v.size(); shift += 7) {
      const uint8_t c = v[at++];
      x |= static_cast<uint64_t>(c & 0x7F) << shift;
      if ((c & 0x80) == 0) return true;
    }
    return false;
  };
  uint64_t cell = 0, n = 0, z = 0;
  if (!get(cell)) return out;
  while (get(n) && get(z)) {
    const int off = (z & 1) != 0 ? -static_cast<int>((z + 1) / 2) : static_cast<int>(z / 2);
    out.push_back({static_cast<int>(n), off});
  }
  return out;
}

constexpr int kSight = 12;  // the sample game's, either way

gl::Rgba colour_of(int n) {
  static const gl::Rgba kColours[] = {{230, 90, 70},   {80, 170, 230},  {240, 190, 60},
                                      {110, 200, 120}, {190, 120, 220}, {240, 140, 190},
                                      {120, 220, 210}, {200, 200, 200}};
  return kColours[static_cast<size_t>(n) % (sizeof kColours / sizeof kColours[0])];
}

// SPECTATORS ON THE SERVER ITSELF (server/spectators.hpp): the sample game served live with its own
// TV on the simulated network under a preset, held back by the delay; spectators from the start
// and one who comes halfway. Each is held to the server's digest at its last frame.
struct Spectated {
  int whole = 0, digested = 0, viewers = 0;
  int64_t late_first = -1;   // the block the late one's stream starts at
  std::vector<double> lags;  // ms from a block's start to a spectator holding it whole
  std::string why;
};

Spectated spectate(const ge::net::LinkConditions& link, int bots, int seconds, int keyframe_s,
                   int64_t delay_ms, int from_start, uint32_t seed) {
  namespace gn = ge::net;
  Spectated out;
  gn::SimNetwork net = gn::make_sim_network(seed, link);
  gn::Transport tv_port;
  gn::open_sim_transport(tv_port, net, gn::sim_address(0, 27600));
  sv::ServerConfig c;
  c.map = "ring";
  c.bots = bots;
  c.maxclients = 16;
  c.seed = seed;
  c.offline = true;
  c.tv_delay_s = static_cast<double>(delay_ms) / 1000;
  sv::Server s;
  sv::Recording rec;
  rec.keyframe_seconds = keyframe_s;
  if (!sv::server_open(s, sv::sample_game(), c, nullptr, nullptr, out.why) ||
      !sv::record_begin(rec, s, out.why))
    return out;
  sv::ServerTv t;
  sv::server_tv_open(t, s, tv_port);
  struct Spectator {
    gn::Transport port;
    tv::TvClient client;
    std::vector<int64_t> held_ms;  // when it held each block whole, in its stream's order
  };
  std::vector<std::unique_ptr<Spectator>> spec;
  const auto join = [&] {
    spec.push_back(std::make_unique<Spectator>());
    Spectator& sp = *spec.back();
    gn::open_sim_transport(sp.port, net, gn::sim_address(uint32_t(spec.size()), 40000));
    tv::tv_client_init(sp.client, sp.port, tv_port.local, "spectator" + std::to_string(spec.size()),
                       false, uint64_t{seed} * 131u + spec.size(), s.now_us);
  };
  for (int i = 0; i < from_start; i++) join();
  const auto tick = [&](int64_t now_us, std::span<const uint8_t> fresh) {
    sv::server_tv_frame(t, fresh, now_us);
    gn::sim_advance(net, now_us);
    for (auto& sp : spec) {
      tv::tv_client_update(sp->client, now_us);
      while (static_cast<int64_t>(sp->held_ms.size()) < sp->client.watch.blocks)
        sp->held_ms.push_back(now_us / 1000);
    }
  };
  const auto frames = static_cast<int64_t>(seconds / s.frame_s);
  tick(s.now_us, sv::record_fresh(rec));
  for (int64_t f = 0; f < frames; f++) {
    sv::server_frame(s);
    sv::record_frame(rec, s);
    tick(s.now_us, sv::record_fresh(rec));
    if (f == frames / 2) join();
  }
  const uint64_t digest = sv::server_digest(s);
  sv::record_finish(rec, s);
  sv::server_tv_end(t);
  std::span<const uint8_t> rest = sv::record_fresh(rec);
  for (int64_t at = s.now_us; at < s.now_us + delay_ms * 1000 + 10000000; at += 10000) {
    tick(at, rest);
    rest = {};
    bool all = true;
    for (const auto& sp : spec)
      all = all && (sp->client.watch.ended || !sp->client.watch.refused.empty());
    if (all) break;
  }
  out.viewers = static_cast<int>(spec.size());
  for (size_t k = 0; k < spec.size(); k++) {
    const Spectator& sp = *spec[k];
    out.whole += sp.client.watch.ended ? 1 : 0;
    ge::replay::StreamReader r;
    std::string e;
    if (!ge::replay::read_stream_header(sp.client.watch.stream, r, e)) continue;
    sv::SampleWatch w;
    ge::replay::Block b;
    size_t i = 0;
    bool read = true;
    while (ge::replay::next_block(r, b, e)) {
      if (k + 1 == spec.size() && i == 0) out.late_first = b.index;
      if (i < sp.held_ms.size())
        out.lags.push_back(static_cast<double>(sp.held_ms[i] - b.index * 1000));
      i++;
      for (const ge::replay::Chunk& ch : b.chunks)
        read = (ch.track == sv::kRecordKeyframeTrack ? sv::sample_watch_keyframe(w, ch.bytes)
                                                     : sv::sample_watch_frame(w, ch.bytes)) &&
               read;
    }
    out.digested += read && e.empty() && sv::sample_watch_hash(w) == digest ? 1 : 0;
  }
  sv::server_tv_close(t, s.now_us);
  sv::server_close(s);
  return out;
}

void run(gl::Lab& lab) {
  const int bots = static_cast<int>(lab.arg_num("bots"));
  const int seconds = static_cast<int>(lab.arg_num("seconds"));
  std::string error;

  // THE MATCH, RECORDED: every player's view at every frame kept off the live game.
  sv::ServerConfig c;
  c.map = "ring";
  c.bots = bots;
  c.maxclients = 16;
  c.seed = static_cast<uint32_t>(lab.arg_num("seed"));
  c.offline = true;
  sv::Server s;
  if (!sv::server_open(s, sv::sample_game(), c, nullptr, nullptr, error)) {
    lab.fail("the sample server did not open: " + error,
             "next: run make test T=server_core; skill: net.");
    return;
  }
  const sv::ServerGame& g = sv::sample_game();
  sv::Recording rec;
  rec.keyframe_seconds = static_cast<int>(lab.arg_num("keyframe-seconds"));
  if (!sv::record_begin(rec, s, error)) {
    lab.fail("the recording did not start: " + error,
             "next: run make test T=server_record; skill: replay.");
    return;
  }
  std::vector<std::vector<std::vector<uint8_t>>> live;
  const auto keep = [&] {
    std::vector<std::vector<uint8_t>> row(static_cast<size_t>(bots));
    for (int p = 0; p < bots; p++) g.player_view(s.state, p, row[static_cast<size_t>(p)]);
    live.push_back(std::move(row));
  };
  keep();
  const int64_t frames = static_cast<int64_t>(seconds) * 60;
  for (int64_t f = 0; f < frames; f++) {
    sv::server_frame(s);
    sv::record_frame(rec, s);
    keep();
  }
  sv::record_finish(rec, s);
  const uint64_t digest = sv::server_digest(s);
  sv::server_close(s);
  lab.metric("recording.bytes", static_cast<double>(rec.bytes.size()), "bytes");
  lab.metric("recording.bytes-per-s", static_cast<double>(rec.bytes.size()) / seconds, "bytes/s");
  lab.metric("recording.keyframes", static_cast<double>(rec.keyframes), "count");

  // PLAYED BACK FROM EVERY PLAYER'S VIEW: frame by frame, then a seek into every second.
  sv::Playback pb;
  if (!sv::playback_open(pb, g, rec.bytes, error)) {
    lab.fail("the recording did not open for playback: " + error,
             "next: run make test T=server_record; skill: replay.");
    return;
  }
  int64_t checked = 0, wrong = 0;
  std::vector<std::vector<std::vector<uint8_t>>> played;
  const auto hold = [&](int64_t frame, bool keep_row) {
    std::vector<std::vector<uint8_t>> row(static_cast<size_t>(bots));
    for (int p = 0; p < bots; p++) {
      sv::playback_view(pb, p, row[static_cast<size_t>(p)]);
      checked++;
      if (row[static_cast<size_t>(p)] != live[static_cast<size_t>(frame)][static_cast<size_t>(p)])
        wrong++;
    }
    if (keep_row) played.push_back(std::move(row));
  };
  hold(0, true);
  while (sv::playback_step(pb, error)) hold(pb.at, true);
  int64_t seeks = 0, seek_wrong_before = wrong;
  for (int64_t at = 37; at <= frames; at += 60) {
    if (!sv::playback_seek(pb, at, error)) break;
    hold(at, false);
    seeks++;
  }
  if (!error.empty()) {
    lab.fail("the playback stopped: " + error,
             "next: run make test T=server_record; skill: replay.");
    return;
  }
  lab.metric("playback.views-checked", static_cast<double>(checked), "count");
  lab.metric("playback.views-wrong", static_cast<double>(wrong), "count");
  lab.metric("playback.seeks", static_cast<double>(seeks), "count");
  lab.metric("playback.seek-views-wrong", static_cast<double>(wrong - seek_wrong_before), "count");

  // THE VIEWS PICTURED: a panel per player, a column per cell around it, a row a tenth of a second.
  {
    const int cell = 4, rows_every = 6, gap = 12, top = 22;
    const int panel_w = (2 * kSight + 1) * cell;
    const int rows = static_cast<int>(played.size() + rows_every - 1) / rows_every;
    const int w = bots * panel_w + (bots + 1) * gap;
    gl::Picture pic = gl::create_picture(w, top + rows + 14, {18, 20, 26});
    for (int p = 0; p < bots; p++) {
      const int x0 = gap + p * (panel_w + gap);
      gl::fill_rect(pic, x0, top, panel_w, rows, {30, 33, 42});
      gl::fill_rect(pic, x0 + kSight * cell, top, cell, rows, colour_of(p));  // the player itself
      gl::draw_text(pic, x0, 6, "player " + std::to_string(p + 1), colour_of(p));
      for (int r = 0; r < rows; r++) {
        const std::vector<Seen> seen =
            read_view(played[static_cast<size_t>(r) * static_cast<size_t>(rows_every)]
                            [static_cast<size_t>(p)]);
        for (const Seen& o : seen)
          gl::fill_rect(pic, x0 + (o.offset + kSight) * cell, top + r, cell, 1,
                        colour_of(o.number));
      }
    }
    gl::draw_text(pic, gap, top + rows + 4,
                  "time down, a row a tenth of a second; the player in the middle column",
                  {150, 155, 170});
    lab.shot(
        {"views",
         "EVERY PLAYER'S VIEW OF THE RECORDED MATCH, PLAYED BACK",
         "The recording played back once, seen from each of the " + std::to_string(bots) +
             " players in turn: the cells within " + std::to_string(kSight) +
             " either way of the player, and every other player there in its own colour. "
             "Each view equals the one the live game gave at the same frame.",
         "streaks where two players walk side by side; the same pair seen from both sides "
         "as mirror images",
         {{"bots", lab.arg("bots")}, {"seconds", lab.arg("seconds")}, {"seed", lab.arg("seed")}}},
        pic);
  }

  // RELAYED WITH THE DELAY: the recording broadcast at the match's speed under every preset.
  tv::TrialConfig base;
  base.blocks = seconds;
  base.block_ms = 1000;
  base.viewers_direct = static_cast<int>(lab.arg_num("direct"));
  base.viewers_relayed = static_cast<int>(lab.arg_num("relayed"));
  base.delay_ms = static_cast<int64_t>(lab.arg_num("delay-ms"));
  base.seed = static_cast<uint32_t>(lab.arg_num("seed"));
  base.source = &rec.bytes;
  base.keep_streams = true;
  gl::LabTable table{"each preset: the recording through a source with the delay, a relay, viewers",
                     {"preset", "viewers whole", "at the server's digest", "lag min ms",
                      "lag p50 ms", "lag p95 ms", "lag max ms", "none before the delay"},
                     {}};
  std::vector<gl::Sample> lags;
  int good = 0;
  const std::vector<std::string> presets = split(lab.arg("presets"));
  for (const std::string& name : presets) {
    tv::TrialConfig tc = base;
    if (!ge::net::conditions_preset(name, tc.link)) {
      lab.fail("unknown preset '" + name + "'",
               "Pick from lan, broadband, wifi, mobile, bad, awful (engine/net/conditions.cpp); "
               "skill: net.");
      return;
    }
    const tv::TrialResult r = tv::run_tv_trial(tc);
    int whole = 0, digested = 0;
    std::vector<double> all;
    for (const tv::TrialViewer& v : r.viewers) {
      whole += v.whole ? 1 : 0;
      all.insert(all.end(), v.lag_ms.begin(), v.lag_ms.end());
      sv::Playback vp;
      std::string e;
      if (v.whole && sv::playback_open(vp, g, v.stream, e) &&
          sv::playback_seek(vp, static_cast<int64_t>(vp.frames.size()), e)) {
        const auto* w = static_cast<const sv::SampleWatch*>(vp.watch);
        digested += w->frames == frames && sv::sample_watch_hash(*w) == digest ? 1 : 0;
      }
    }
    const gl::Summary st = gl::summarize(all);
    const bool held = all.empty() || st.min >= static_cast<double>(base.delay_ms);
    const int n = static_cast<int>(r.viewers.size());
    table.rows.push_back({name, std::to_string(whole) + " of " + std::to_string(n),
                          std::to_string(digested) + " of " + std::to_string(n), fmt(st.min, 0),
                          fmt(st.p50, 0), fmt(st.p95, 0), fmt(st.max, 0), held ? "yes" : "no"});
    lags.push_back({name, all});
    good += whole == n && digested == n && held ? 1 : 0;
    lab.metric(name + ".viewers-whole", whole, "count");
    lab.metric(name + ".at-digest", digested, "count");
    lab.metric(name + ".lag-min-ms", st.min, "ms");
    lab.metric(name + ".lag-p50-ms", st.p50, "ms");
  }
  lab.table(table);
  lab.metric("presets-good", good, "count");
  const gl::ChartFrame frame{"HOW LATE EACH BLOCK OF THE RECORDING REACHED EACH VIEWER",
                             "held back " + lab.arg("delay-ms") + " ms at the source; a relay; " +
                                 std::to_string(base.viewers_direct + base.viewers_relayed) +
                                 " viewers; seed " + lab.arg("seed"),
                             "", "lag after the server wrote it (ms)"};
  lab.shot({"lag",
            frame.title,
            "The recording broadcast at the match's own speed through a source's TV node that "
            "holds every block back by the delay, then a relay; a box per network preset of when "
            "each viewer had each block whole.",
            "every box at or above the delay; the boxes rising with the link's loss",
            {{"presets", lab.arg("presets")}, {"delay-ms", lab.arg("delay-ms")}}},
           gl::to_picture(gl::box_chart(frame, lags).plot));

  // SPECTATORS ON THE SERVER ITSELF: the server's own TV, live, under every preset.
  gl::LabTable spect{
      "each preset: spectators on the server's own TV, the delay, one coming halfway",
      {"preset", "spectators whole", "at the server's digest", "late one from block", "lag min ms",
       "lag p50 ms", "lag max ms", "none before the delay"},
      {}};
  std::vector<gl::Sample> spect_lags;
  int spect_good = 0;
  const int64_t delay_ms = static_cast<int64_t>(lab.arg_num("delay-ms"));
  for (const std::string& name : presets) {
    ge::net::LinkConditions link;
    ge::net::conditions_preset(name, link);
    const Spectated r = spectate(link, bots, seconds, rec.keyframe_seconds, delay_ms,
                                 static_cast<int>(lab.arg_num("direct")), c.seed);
    const gl::Summary st = gl::summarize(r.lags);
    const bool held = r.lags.empty() || st.min >= static_cast<double>(delay_ms);
    const bool ok = r.viewers > 0 && r.whole == r.viewers && r.digested == r.viewers && held;
    spect_good += ok ? 1 : 0;
    spect.rows.push_back({name, std::to_string(r.whole) + " of " + std::to_string(r.viewers),
                          std::to_string(r.digested) + " of " + std::to_string(r.viewers),
                          std::to_string(r.late_first), fmt(st.min, 0), fmt(st.p50, 0),
                          fmt(st.max, 0), held ? "yes" : "no"});
    spect_lags.push_back({name, r.lags});
    std::string key = "spectators.";
    key += name;
    lab.metric(key + ".whole", r.whole, "count");
    lab.metric(key + ".at-digest", r.digested, "count");
    lab.metric(key + ".late-first-block", static_cast<double>(r.late_first), "block");
    lab.metric(key + ".lag-min-ms", st.min, "ms");
  }
  lab.table(spect);
  lab.metric("spectators.presets-good", spect_good, "count");
  const gl::ChartFrame sframe{"HOW LATE SPECTATORS ON THE SERVER SAW EACH SECOND OF THE MATCH",
                              "the server's own TV, " + lab.arg("delay-ms") + " ms behind; " +
                                  lab.arg("direct") + " from the start and one halfway; seed " +
                                  lab.arg("seed"),
                              "", "lag after the second began (ms)"};
  lab.shot({"spectators",
            sframe.title,
            "The match watched live from the server's own TV (sv_tvport), every block held back "
            "by the delay (sv_tvdelay); a box per network preset of when each spectator had each "
            "second whole.",
            "every box at or above the delay and the second a block takes to fill; the late "
            "spectator's catch-up as the dots far to the right",
            {{"presets", lab.arg("presets")}, {"delay-ms", lab.arg("delay-ms")}}},
           gl::to_picture(gl::box_chart(sframe, spect_lags).plot));

  if (spect_good != static_cast<int>(presets.size()))
    lab.fail(
        "under a preset a spectator on the server's TV was not whole, not at the digest, or "
        "saw a second before the delay",
        "next: read the spectators table's row and run make test T=server_spectators; "
        "skill: replay.");
  if (wrong != 0)
    lab.fail(std::to_string(wrong) + " of " + std::to_string(checked) +
                 " views played back differ from the live game's",
             "next: run make test T=server_record and compare the sample game's watch_view with "
             "player_view (engine/server/sample.cpp); skill: replay.");
  if (good != static_cast<int>(presets.size()))
    lab.fail(
        "under a preset a viewer was not whole, not at the digest, or got a block before "
        "the delay",
        "next: read the table's row and run that preset in tests/tv_net_test.cpp; skill: "
        "replay.");
}

const gl::LabSpec kSpec{
    "server-record",
    gl::LabKind::sim,
    "engine",
    "engine/server (the recording, its playback, the server's own TV), engine/tv (the relay with "
    "a delay)",
    "replay",
    "Does a match the server recorded play back as every player saw it, from the start and after "
    "a seek, and do watchers on a relay, and spectators on the server's own TV, get it whole with "
    "the delay under every network preset?",
    {
        {"bots", "6", "the sample game's players"},
        {"seconds", "30", "the match's length"},
        {"keyframe-seconds", "5", "a keyframe every this many seconds of the recording"},
        {"presets", "lan,broadband,wifi,mobile,bad,awful", "condition presets, comma-separated"},
        {"delay-ms", "2000", "the source node's delay"},
        {"direct", "2", "viewers on the source's node"},
        {"relayed", "3", "viewers under the relay"},
        {"seed", "7", "the match's, the network's and the nodes' stream"},
    },
    "--seconds=12 --presets=lan,bad",
    run,
};
GE_LAB(kSpec);

}  // namespace
