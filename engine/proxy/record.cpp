// Recording on the proxy (record.hpp): the datagrams passed on, as a replay file.
//
// ge:summary The proxy's recorder: datagrams into replay-file blocks, and read back in order
// ge:keywords proxy record recording replay-file datagrams blocks read-back
// ge:skill net replay
// ge:test tests/proxy_test.cpp
#include "proxy/record.hpp"

#include <algorithm>
#include <utility>

namespace ge::proxy {

namespace {

int64_t ms_of(const ProxyRecording& r, int64_t now_us) {
  return std::max<int64_t>(r.last_ms, (now_us - r.start_us) / 1000);
}

// The open block ended when `ms` is past it.
void roll(ProxyRecording& r, int64_t ms) {
  if (ms - r.block_at_ms < r.block_us / 1000) return;
  replay::stream_end_block(r.writer, r.writer.blocks == 0);
  r.block_at_ms = ms;
}

}  // namespace

void proxy_record_begin(ProxyRecording& r, const std::string& game, replay::Meta meta,
                        int64_t now_us) {
  r = ProxyRecording{};
  r.start_us = now_us;
  replay::StreamHeader h;
  h.game = game;
  meta.emplace_back("recorder", "proxy");
  meta.emplace_back("start_us", std::to_string(now_us));
  h.meta = std::move(meta);
  h.tracks = {{kTrackToServer, replay::TrackKind::messages, replay::TrackCodec::zstd, "to_server"},
              {kTrackToClient, replay::TrackKind::messages, replay::TrackCodec::zstd, "to_client"}};
  replay::stream_begin(r.writer, h, r.bytes);
  r.open = true;
}

void proxy_record(ProxyRecording& r, bool to_server, std::span<const uint8_t> datagram,
                  int64_t now_us) {
  if (!r.open || r.finished) return;
  const int64_t ms = ms_of(r, now_us);
  roll(r, ms);
  replay::stream_add(r.writer, to_server ? kTrackToServer : kTrackToClient, ms, datagram);
  r.last_ms = ms;
  (to_server ? r.to_server : r.to_client)++;
  r.payload += datagram.size();
}

void proxy_record_finish(ProxyRecording& r, int64_t now_us) {
  if (!r.open || r.finished) return;
  const int64_t ms = ms_of(r, now_us);
  replay::stream_end_block(r.writer, r.writer.blocks == 0);
  replay::stream_finish(r.writer, {{"duration_ms", std::to_string(ms)},
                                   {"to_server", std::to_string(r.to_server)},
                                   {"to_client", std::to_string(r.to_client)}});
  r.finished = true;
}

bool proxy_read_recording(std::span<const uint8_t> file, std::vector<Recorded>& out,
                          std::string& error) {
  out.clear();
  replay::StreamReader rd;
  if (!replay::read_stream_header(file, rd, error)) return false;
  bool ours = false;
  for (const auto& [k, v] : rd.header.meta) ours = ours || (k == "recorder" && v == "proxy");
  if (!ours) {
    error = "the file is a replay file but not a proxy's recording (no recorder=proxy in its meta)";
    return false;
  }
  replay::Block b;
  while (replay::next_block(rd, b, error)) {
    const size_t first = out.size();
    for (replay::Chunk& c : b.chunks) {
      if (c.track != kTrackToServer && c.track != kTrackToClient) continue;
      out.push_back({c.track == kTrackToServer, c.time_ms, std::move(c.bytes)});
    }
    std::stable_sort(out.begin() + static_cast<std::ptrdiff_t>(first), out.end(),
                     [](const Recorded& a, const Recorded& z) { return a.time_ms < z.time_ms; });
  }
  return error.empty();
}

}  // namespace ge::proxy
