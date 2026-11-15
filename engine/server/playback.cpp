// A recorded match played back (playback.hpp).
//
// ge:summary A recorded match played back: blocks read into keyframes and frames, seek and step
// ge:keywords playback recording seek keyframe frames
// ge:skill replay net
// ge:test tests/server_record_test.cpp
#include "server/playback.hpp"

#include <cstdlib>

#include "server/record.hpp"

namespace ge::server {

Playback::~Playback() {
  if (watch != nullptr && game != nullptr && game->watch_close != nullptr) game->watch_close(watch);
}

bool playback_open(Playback& pb, const ServerGame& game, std::span<const uint8_t> bytes,
                   std::string& error) {
  if (game.watch_open == nullptr || game.watch_close == nullptr || game.watch_keyframe == nullptr ||
      game.watch_frame == nullptr) {
    error = std::string(game.title) +
            " cannot watch a recording: its ServerGame fills no watch_open, watch_close, "
            "watch_keyframe and watch_frame (next: see engine/server/playback.hpp)";
    return false;
  }
  replay::StreamReader r;
  if (!replay::read_stream_header(bytes, r, error)) return false;
  if (r.header.game != game.binary) {
    error = "the recording is " + r.header.game + "'s, not " + game.binary +
            "'s (next: play it back with its own game)";
    return false;
  }
  pb.game = &game;
  pb.header = r.header;
  for (const auto& [k, v] : r.header.meta)
    if (k == "start_frame") pb.start_frame = std::strtoll(v.c_str(), nullptr, 10);
  replay::Block b;
  while (replay::next_block(r, b, error)) {
    for (replay::Chunk& c : b.chunks) {
      if (c.track == kRecordKeyframeTrack)
        pb.keys.push_back({static_cast<int64_t>(pb.frames.size()), std::move(c.bytes)});
      else if (c.track == kRecordFramesTrack)
        pb.frames.push_back(std::move(c.bytes));
    }
  }
  if (!error.empty()) return false;
  if (pb.keys.empty() || pb.keys.front().frame != 0) {
    error = "the recording has no keyframe at its start (next: record it from record_begin)";
    return false;
  }
  pb.watch = game.watch_open();
  return playback_seek(pb, 0, error);
}

bool playback_seek(Playback& pb, int64_t frame, std::string& error) {
  if (pb.game == nullptr || frame < 0 || frame > static_cast<int64_t>(pb.frames.size())) {
    error = "frame " + std::to_string(frame) + " is outside the recording's 0.." +
            std::to_string(pb.frames.size());
    return false;
  }
  const Playback::Key* key = &pb.keys.front();
  for (const Playback::Key& k : pb.keys)
    if (k.frame <= frame) key = &k;
  if (!pb.game->watch_keyframe(pb.watch, key->bytes, error)) return false;
  pb.at = key->frame;
  while (pb.at < frame)
    if (!playback_step(pb, error)) return false;
  return true;
}

bool playback_step(Playback& pb, std::string& error) {
  if (pb.game == nullptr || pb.at >= static_cast<int64_t>(pb.frames.size())) return false;
  if (!pb.game->watch_frame(pb.watch, pb.frames[static_cast<size_t>(pb.at)], error)) return false;
  pb.at++;
  return true;
}

int playback_players(const Playback& pb) {
  return pb.game != nullptr && pb.game->watch_players != nullptr ? pb.game->watch_players(pb.watch)
                                                                 : 0;
}

void playback_view(const Playback& pb, int player, std::vector<uint8_t>& out) {
  if (pb.game != nullptr && pb.game->watch_view != nullptr)
    pb.game->watch_view(pb.watch, player, out);
}

}  // namespace ge::server
