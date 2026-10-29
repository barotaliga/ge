// THE TWO TRACKS WRITTEN IN TIME ORDER (docs/replays.md § Compressed video): the encoded pictures
// and sound packets held until each can go into the file without a block of the other track
// still to come landing before it. The export (av_export.hpp) and the live capture (live.hpp)
// both hold their packets here.
//
// `write` takes the times the next packet of each track can come at no sooner than (`next_video`
// and `next_audio`, ms; INT64_MAX when a track has nothing more): a picture waits while a sound
// packet still to be cut could start before it, a sound packet while a picture still to come
// could be shown before it. `all` writes everything held, in time order (the close). A picture
// handed to an encoder is `fed`: until its packet comes out (an encoder may hold one back) it
// is still to come, whatever `next_video` says.
//
// ge:summary The encoded pictures and sound held and written into the file in time order
// ge:keywords interleave packets time-order matroska webm av1 opus video audio queue
// ge:skill replay
// ge:test tests/capture_codecs_test.cpp
#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "capture/codecs.hpp"
#include "capture/matroska.hpp"

namespace ge::capture {

struct Interleave {
  std::vector<Packet> video, audio;  // encoded, not yet written, each in time order

  // A picture at `ms` handed to the encoder.
  void fed(int64_t ms) { fed_.push_back(ms); }
  bool write(MatroskaWriter& mux, int64_t next_video, int64_t next_audio, bool all,
             std::string& error);
  bool empty() const { return v_ == video.size() && a_ == audio.size(); }

 private:
  size_t v_ = 0, a_ = 0;     // the next to write in each
  std::deque<int64_t> fed_;  // pictures fed whose packet has not come out, oldest first
};

}  // namespace ge::capture
