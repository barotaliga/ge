// The TV trial (trial.hpp): the made-up match, the tree on the simulated network, the measures.
//
// ge:summary The TV trial's match, tree and measures on the simulated network
// ge:keywords tv trial run_tv_trial lag whole
// ge:skill replay net
// ge:test tests/tv_net_test.cpp
#include "tv/trial.hpp"

#include <algorithm>
#include <memory>

#include "net/sim_network.hpp"
#include "net/transport.hpp"
#include "replay/stream.hpp"
#include "tv/net_link.hpp"

namespace ge::tv {

namespace {

struct Match {
  std::vector<uint8_t> file;
  std::vector<size_t> ends;  // where the header and each block end
  std::vector<bool> key;     // each block a keyframe
};

// A whole stream split at its header and blocks.
Match split_source(const std::vector<uint8_t>& file) {
  Match m;
  m.file = file;
  replay::StreamReader r;
  std::string error;
  if (!replay::read_stream_header(file, r, error)) return m;
  m.ends.push_back(r.at);
  size_t at = r.at;
  replay::BlockPeek p;
  while (replay::peek_block(file, at, p, error) == 1) {
    at += p.size;
    m.ends.push_back(at);
    m.key.push_back(p.keyframe);
  }
  return m;
}

Match make_match(const TrialConfig& c) {
  Match m;
  replay::StreamWriter w;
  replay::StreamHeader h;
  h.game = "tv-trial";
  h.tracks = {{1, replay::TrackKind::inputs, replay::TrackCodec::zstd, "inputs"}};
  replay::stream_begin(w, h, m.file);
  m.ends.push_back(m.file.size());
  uint32_t x = c.seed;
  for (int b = 0; b < c.blocks; b++) {
    for (int k = 0; k < 10; k++) {
      std::vector<uint8_t> bytes(c.chunk_bytes);
      for (uint8_t& v : bytes) {
        x = x * 1664525u + 1013904223u;
        v = static_cast<uint8_t>(x >> 24);
      }
      replay::stream_add(w, 1, int64_t{b} * 1000 + int64_t{k} * 100, bytes);
    }
    replay::stream_end_block(w, b % c.keyframe_every == 0);
    m.ends.push_back(m.file.size());
    m.key.push_back(b % c.keyframe_every == 0);
  }
  replay::stream_finish(w, {});
  return m;
}

}  // namespace

TrialResult run_tv_trial(const TrialConfig& c) {
  TrialResult out;
  net::SimNetwork sim = net::make_sim_network(c.seed, c.link);
  const Match m = c.source != nullptr ? split_source(*c.source) : make_match(c);
  const int blocks = static_cast<int>(m.key.size());
  std::vector<std::unique_ptr<net::Transport>> ts;
  auto transport = [&](uint32_t node, uint16_t port) {
    ts.push_back(std::make_unique<net::Transport>());
    net::open_sim_transport(*ts.back(), sim, net::sim_address(node, port));
    return ts.back().get();
  };
  int64_t now = 0;
  TvHost host;
  Config hc;
  hc.name = "source";
  hc.delay_ms = c.delay_ms;
  tv_host_init(host, hc, *transport(0, 1), net::kMaxPeers, uint64_t{c.seed} + 10);
  TvRelay relay;
  Config rc;
  rc.name = "relay";
  tv_relay_init(relay, rc, *transport(1, 1), net::sim_address(0, 1), *transport(1, 2),
                net::kMaxPeers, uint64_t{c.seed} + 20, now);
  std::vector<std::unique_ptr<TvClient>> clients;
  const int count = c.viewers_direct + c.viewers_relayed;
  for (int i = 0; i < count; i++) {
    const bool relayed = i >= c.viewers_direct;
    clients.push_back(std::make_unique<TvClient>());
    tv_client_init(*clients.back(), *transport(static_cast<uint32_t>(2 + i), 1),
                   relayed ? net::sim_address(1, 2) : net::sim_address(0, 1),
                   "viewer" + std::to_string(i), false, uint64_t{c.seed} + 30 + i, now);
    TrialViewer v;
    v.relayed = relayed;
    out.viewers.push_back(v);
  }
  std::vector<int64_t> written_us;  // when the source wrote each block
  bool said = false;
  size_t fed = 0;
  const int64_t step_us = 10000;
  const int64_t most_steps = (int64_t{blocks} * c.block_ms + c.delay_ms + 60000) * 1000 / step_us;
  for (int64_t s = 0; s < most_steps; s++) {
    now += step_us;
    net::sim_advance(sim, now);
    if (fed < m.ends.size() && now >= static_cast<int64_t>(fed) * c.block_ms * 1000) {
      const size_t from = fed == 0 ? 0 : m.ends[fed - 1];
      tv_feed(host.tv, {m.file.data() + from, m.ends[fed] - from}, now / 1000);
      out.source_bytes += static_cast<int64_t>(m.ends[fed] - from);
      if (fed > 0) written_us.push_back(now);
      if (++fed == m.ends.size()) tv_end(host.tv);
    }
    tv_host_update(host, now);
    tv_relay_update(relay, now);
    for (int i = 0; i < count; i++) {
      TvClient& cl = *clients[static_cast<size_t>(i)];
      TrialViewer& v = out.viewers[static_cast<size_t>(i)];
      tv_client_update(cl, now);
      for (const auto& bytes : cl.fresh) {
        replay::BlockPeek p;
        std::string error;
        if (replay::peek_block(bytes, 0, p, error) != 1) continue;  // the header
        if (v.first < 0) v.first = p.index;
        if (p.index >= 0 && p.index < static_cast<int64_t>(written_us.size()))
          v.lag_ms.push_back(static_cast<double>(now - written_us[static_cast<size_t>(p.index)]) /
                             1000.0);
      }
    }
    if (!said && clients.back()->watch.blocks > 2) {
      tv_client_say(*clients.back(), "hello from the end of the line");
      said = true;
    }
    bool done = true;
    for (auto& cl : clients) done = done && (cl->watch.ended || !cl->watch.error.empty());
    if (done && s % 50 == 0) {
      bool heard = true;
      for (auto& cl : clients) heard = heard && !cl->watch.chat.empty();
      if (heard || !said) break;
    }
  }
  out.seconds = static_cast<double>(now) / 1e6;
  out.all_whole = true;
  const double match_s = std::max(1.0, static_cast<double>(blocks));  // a block is a second
  for (int i = 0; i < count; i++) {
    const Watch& w = clients[static_cast<size_t>(i)]->watch;
    TrialViewer& v = out.viewers[static_cast<size_t>(i)];
    v.blocks = w.blocks;
    v.told = w.viewers;
    v.error = w.error;
    v.bytes_per_s = static_cast<double>(w.bytes) / match_s;
    for (const ChatLine& l : w.chat) v.chat += l.who + ": " + l.text;
    // Whole: the header, then every block from the one it started at (a keyframe) to the end.
    std::vector<uint8_t> want(m.file.begin(),
                              m.file.begin() + static_cast<std::ptrdiff_t>(m.ends[0]));
    if (v.first >= 0 && v.first < blocks && m.key[static_cast<size_t>(v.first)])
      want.insert(
          want.end(),
          m.file.begin() + static_cast<std::ptrdiff_t>(m.ends[static_cast<size_t>(v.first)]),
          m.file.begin() + static_cast<std::ptrdiff_t>(m.ends.back()));
    v.whole = w.ended && w.stream == want;
    if (c.keep_streams) v.stream = w.stream;
    if (!v.whole) {
      out.all_whole = false;
      out.why += "viewer" + std::to_string(i) + ": from block " + std::to_string(v.first) + ", " +
                 std::to_string(w.blocks) + " blocks, " + (w.ended ? "ended" : "not ended") +
                 (w.error.empty() ? "" : ", " + w.error) + "; ";
    }
  }
  out.counted = tv_viewer_count(host.tv);
  return out;
}

}  // namespace ge::tv
