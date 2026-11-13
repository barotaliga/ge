// The watcher (classic_watcher.hpp): a spectator's recording broadcast as it grows.
//
// ge:summary The watcher: the spectator's client recording fed to its broadcast every frame
// ge:keywords watcher spectator broadcast recording feed
// ge:skill net replay
// ge:test tests/shooters_edge_classic_watcher_test.cpp
#include "shooters_edge/net/classic_watcher.hpp"

#include <span>
#include <string>

namespace shooters_edge::net {

namespace {

void feed(ClassicWatcher& w) {
  if (w.recording.bytes.size() <= w.fed) return;
  client::recording_broadcast_add(w.cast,
                                  std::span<const uint8_t>(w.recording.bytes).subspan(w.fed));
  w.fed = w.recording.bytes.size();
}

}  // namespace

void watcher_start(ClassicWatcher& w, gn::Transport& t, const gn::Address& server, ClientConfig cfg,
                   const ge::replay::Meta& meta, int64_t now_us) {
  cfg.spectator = true;
  record_start(w.recording, false);
  w.fed = 0;
  w.ended = false;
  ge::replay::Meta m = {{"recorder", "watcher"}, {"server", gn::to_string(server)}};
  m.insert(m.end(), meta.begin(), meta.end());
  client::recording_broadcast_begin(w.cast, m);
  client_start(w.client, t, server, cfg, static_cast<double>(now_us) / 1e6);
  w.client.record = &w.recording;
}

void watcher_tick(ClassicWatcher& w, int64_t now_us) {
  if (w.ended) return;
  client_tick(w.client, now_us);
  feed(w);
  client::recording_broadcast_tick(w.cast, now_us / 1000);
  if (w.client.stage == ClientStage::dropped) watcher_stop(w);
}

void watcher_stop(ClassicWatcher& w) {
  if (w.ended) return;
  w.client.record = nullptr;
  record_stop(w.recording);
  feed(w);
  client::recording_broadcast_end(
      w.cast, {{"stage", client_stage_name(w.client.stage)},
               {"frames", std::to_string(w.client.full_frames + w.client.delta_frames)}});
  w.ended = true;
}

}  // namespace shooters_edge::net
