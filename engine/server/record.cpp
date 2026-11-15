// A match recorded by the server (record.hpp).
//
// ge:summary A match recorded by the server: header, a block a second, keyframe blocks, trailer
// ge:keywords tv server-source frames keyframe block stream-writer
// ge:skill replay net
// ge:test tests/server_record_test.cpp
#include "server/record.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ge::server {

namespace {

int64_t ms_of(const Server& s) { return s.now_us / 1000; }

// The game's frame bytes for the server's last frame, asked of the game once whatever the number
// of recordings, so each records the same bytes and the game's own state (what it has handed out)
// moves once a frame.
const std::vector<uint8_t>& frame_bytes(const Server& s) {
  if (s.tv_frame_at != s.frame) {
    s.tv_frame_bytes.clear();
    s.game->tv_frame(s.state, s.tv_frame_bytes);
    s.tv_frame_at = s.frame;
  }
  return s.tv_frame_bytes;
}

void keyframe(Recording& src, const Server& s) {
  src.scratch.clear();
  s.game->tv_keyframe(s.state, src.scratch);
  replay::stream_add(src.writer, kRecordKeyframeTrack, ms_of(s), src.scratch);
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

bool record_begin(Recording& src, const Server& s, std::string& error) {
  if (s.game == nullptr || s.state == nullptr) {
    error = "cannot record: the server is not running yet";
    return false;
  }
  if (s.game->tv_frame == nullptr || s.game->tv_keyframe == nullptr) {
    error = std::string(s.game->title) + " has no broadcast, so its matches cannot be recorded";
    return false;
  }
  const int keyframe_seconds = src.keyframe_seconds;
  src = Recording{};
  src.keyframe_seconds = keyframe_seconds;
  src.reports_before = s.reports.empty() ? 0 : s.reports.back().id;
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
      {kRecordKeyframeTrack, replay::TrackKind::keyframe, replay::TrackCodec::zstd, "state"},
      {kRecordFramesTrack, replay::TrackKind::state, replay::TrackCodec::zstd, "frames"}};
  replay::stream_begin(src.writer, h, src.bytes);
  src.open = true;
  // The frame's bytes taken first, so a keyframe of a game that keeps its viewers' state off them
  // is of this frame, not the one before, when a recording begins on a running server.
  frame_bytes(s);
  keyframe(src, s);
  return true;
}

void record_frame(Recording& src, const Server& s) {
  if (!src.open || src.finished) return;
  const std::vector<uint8_t>& bytes = frame_bytes(s);
  replay::stream_add(src.writer, kRecordFramesTrack, ms_of(s), bytes);
  src.frame_bytes += bytes.size();
  if (++src.in_block < src.per_block) return;
  replay::stream_end_block(src.writer, src.block_keyframe);
  src.block_keyframe = false;
  src.in_block = 0;
  if (src.keyframe_seconds > 0 && src.writer.blocks % src.keyframe_seconds == 0) keyframe(src, s);
}

std::span<const uint8_t> record_fresh(Recording& src) {
  const std::span<const uint8_t> out = std::span<const uint8_t>(src.bytes).subspan(src.fresh_at);
  src.fresh_at = src.bytes.size();
  return out;
}

void record_finish(Recording& src, const Server& s) {
  if (!src.open || src.finished) return;
  replay::stream_end_block(src.writer, src.block_keyframe);
  replay::Meta fin = {{"frames", std::to_string(s.frame)}, {"digest", hex(server_digest(s))}};
  // THE CLOSING SUMMARY: the last match the game reported while this stream ran, whole, and its
  // scores in the hub's meta words, so a demo's stats and the hub's are the same record.
  if (!s.reports.empty() && s.reports.back().id != src.reports_before) {
    const net::MatchReport& r = s.reports.back();
    std::string text, error;
    if (net::report_text(r, text, error)) {
      fin.emplace_back("report", std::move(text));
      fin.emplace_back("duration_ms", std::to_string(r.duration_ms));
      for (const net::ReportTeam& t : r.teams)
        fin.emplace_back("team_score", t.name + "\t" + std::to_string(t.score));
      for (const net::ReportPlayer& p : r.players)
        fin.emplace_back("score", p.name + "\t" + std::to_string(p.score));
      src.summarised = true;
    }
  }
  replay::stream_finish(src.writer, fin);
  src.finished = true;
}

}  // namespace ge::server
