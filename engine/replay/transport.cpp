// The transport's moves: the playhead held inside the replay, the speed ladder, the end pausing.
//
// ge:summary The transport's definitions: play, seek, skip, frame, tick
// ge:keywords replay transport play seek skip frame tick playhead
// ge:skill replay
// ge:test tests/replay_transport_test.cpp
#include "replay/transport.hpp"

#include "core/num.hpp"

namespace ge::replay {

namespace num = core::num;

Transport::Transport(double hz, int64_t length) : hz_(hz), length_(length < 0 ? 0 : length) {}

void Transport::play() {
  if (at_end()) head_ = 0;
  playing_ = length_ > 0;
}

void Transport::toggle() {
  if (playing_) {
    pause();
  } else {
    play();
  }
}

void Transport::faster(int dir) {
  const int last = static_cast<int>(kSpeeds.size()) - 1;
  if (dir > 0 && speed_ < last) speed_++;
  if (dir < 0 && speed_ > 0) speed_--;
}

void Transport::seek(int64_t step) {
  head_ = static_cast<double>(step < 0 ? 0 : step > length_ ? length_ : step);
}

void Transport::skip(int dir) {
  const auto by = static_cast<int64_t>(num::round(kSkipS * hz_));
  seek(at() + (dir < 0 ? -by : by));
}

void Transport::frame(int dir) {
  pause();
  const double n = num::round(kFrameS * hz_);
  const int64_t by = n < 1 ? 1 : static_cast<int64_t>(n);
  seek(at() + (dir < 0 ? -by : by));
}

int64_t Transport::tick(double dt_s) {
  if (playing_ && dt_s > 0) {
    head_ += dt_s * speed() * hz_;
    if (head_ >= static_cast<double>(length_)) {
      head_ = static_cast<double>(length_);
      playing_ = false;
    }
  }
  return at();
}

int64_t Transport::at() const { return static_cast<int64_t>(head_); }

void Transport::set_length(int64_t length) {
  length_ = length < 0 ? 0 : length;
  if (head_ > static_cast<double>(length_)) head_ = static_cast<double>(length_);
}

}  // namespace ge::replay
