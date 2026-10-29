// The tracks written in time order (interleave.hpp).
//
// ge:summary Interleave::write: the held packets written oldest first while nothing can precede
// ge:keywords interleave write packets time-order
// ge:skill replay
// ge:test tests/capture_codecs_test.cpp
#include "capture/interleave.hpp"

#include <algorithm>

namespace ge::capture {

bool Interleave::write(MatroskaWriter& mux, int64_t next_video, int64_t next_audio, bool all,
                       std::string& error) {
  // The encoder's packets come out in time order: every picture fed up to the newest is out.
  if (v_ < video.size()) {
    while (!fed_.empty() && fed_.front() <= video.back().ms) fed_.pop_front();
  }
  if (!fed_.empty()) next_video = std::min(next_video, fed_.front());
  for (;;) {
    const bool v = v_ < video.size(), a = a_ < audio.size();
    bool take_audio = false;
    if (v && a) {
      take_audio = audio[a_].ms < video[v_].ms;
    } else if (a) {
      if (!all && audio[a_].ms >= next_video) break;
      take_audio = true;
    } else if (v) {
      if (!all && video[v_].ms > next_audio) break;
    } else {
      break;
    }
    const Packet& p = take_audio ? audio[a_++] : video[v_++];
    if (!(take_audio ? mux.audio(p.ms, p.data, error) : mux.video(p.ms, p.key, p.data, error))) {
      return false;
    }
  }
  if (v_ == video.size()) {
    video.clear();
    v_ = 0;
  }
  if (a_ == audio.size()) {
    audio.clear();
    a_ = 0;
  }
  return true;
}

}  // namespace ge::capture
