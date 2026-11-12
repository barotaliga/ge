// The server as a TV source (server_source.hpp).
//
// ge:summary The server as a TV source: header, frames a block a second, keyframe blocks, trailer
// ge:keywords tv server-source frames keyframe block stream-writer
// ge:skill replay net
// ge:test tests/tv_server_source_test.cpp
#include "tv/server_source.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ge::tv {

namespace {

int64_t ms_of(const server::Server& s) { return s.now_us / 1000; }

void keyframe(ServerSource& src, const server::Server& s) {
  src.scratch.clear();
  s.game->tv_keyframe(s.state, src.scratch);
  replay::stream_add(src.writer, kServerKeyframeTrack, ms_of(s), src.scratch);
  src.block_keyframe = true;
  src.keyframe_bytes += src.scratch.size();
  src.keyframes++;
}

std::string hex(uint64_t v) {
  char b[17];
  std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(v));
  return b;
}

}  // namespace

bool server_source_begin(ServerSource& src, const server::Server& s, std::string& error) {
  if (s.game == nullptr || s.state == nullptr) {
    error = "the server is not open; next: server_open first";
    return false;
  }
  if (s.game->tv_frame == nullptr || s.game->tv_keyframe == nullptr) {
    error = std::string(s.game->title) +
            " has no broadcast: its ServerGame fills no tv_frame and tv_keyframe "
            "(next: see engine/tv/server_source.hpp)";
    return false;
  }
  const int keyframe_seconds = src.keyframe_seconds;
  src = ServerSource{};
  src.keyframe_seconds = keyframe_seconds;
  src.per_block = std::max<int64_t>(1, std::llround(1.0 / s.frame_s));
  replay::StreamHeader h;
  h.game = s.game->binary;
  h.game_version = static_cast<int32_t>(s.game->version);
  h.meta = {{"map", s.config.map},
            {"seed", std::to_string(s.config.seed)},
            {"server", s.config.hostname},
            {"frame_us", std::to_string(std::llround(s.frame_s * 1e6))},
            {"start_frame", std::to_string(s.frame)}};
  h.tracks = {
      {kServerKeyframeTrack, replay::TrackKind::keyframe, replay::TrackCodec::zstd, "state"},
      {kServerFramesTrack, replay::TrackKind::state, replay::TrackCodec::zstd, "frames"}};
  replay::stream_begin(src.writer, h, src.bytes);
  src.open = true;
  keyframe(src, s);
  return true;
}

void server_source_frame(ServerSource& src, const server::Server& s) {
  if (!src.open || src.finished) return;
  src.scratch.clear();
  s.game->tv_frame(s.state, src.scratch);
  replay::stream_add(src.writer, kServerFramesTrack, ms_of(s), src.scratch);
  src.frame_bytes += src.scratch.size();
  if (++src.in_block < src.per_block) return;
  replay::stream_end_block(src.writer, src.block_keyframe);
  src.block_keyframe = false;
  src.in_block = 0;
  if (src.keyframe_seconds > 0 && src.writer.blocks % src.keyframe_seconds == 0) keyframe(src, s);
}

std::span<const uint8_t> server_source_fresh(ServerSource& src) {
  const std::span<const uint8_t> out = std::span<const uint8_t>(src.bytes).subspan(src.fresh_at);
  src.fresh_at = src.bytes.size();
  return out;
}

void server_source_finish(ServerSource& src, const server::Server& s) {
  if (!src.open || src.finished) return;
  replay::stream_end_block(src.writer, src.block_keyframe);
  replay::stream_finish(
      src.writer, {{"frames", std::to_string(s.frame)}, {"digest", hex(server::server_digest(s))}});
  src.finished = true;
}

}  // namespace ge::tv
