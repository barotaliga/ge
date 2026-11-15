// Music played from kept files: see music.hpp.
//
// ge:summary The music player: decode on a worker, two hard-panned voices, pause by frame
// ge:keywords music soundtrack player decode worker pause resume fade
// ge:skill audio
// ge:test tests/audio_music_test.cpp
#include "audio/music.hpp"

#include <fstream>
#include <iterator>
#include <utility>

#include "audio/flac.hpp"

namespace ge::audio {

namespace {

constexpr float kFadeS = 0.1f;  // a track changed or stopped fades on this glide (gone in 0.75 s)

}  // namespace

bool music_from_flac(std::span<const uint8_t> bytes, const std::string& where, MusicTrack* out,
                     std::string* error) {
  std::vector<int32_t> s;
  FlacInfo info;
  if (!flac_decode(bytes, &s, &info, error)) {
    *error = where + ": " + *error;
    return false;
  }
  if (info.channels < 1 || info.rate <= 0 || info.bits < 4) {
    *error = where + ": the FLAC file holds no playable audio (" + std::to_string(info.channels) +
             " channels at " + std::to_string(info.rate) +
             " Hz); next: take the music in again from its source";
    return false;
  }
  const size_t ch = static_cast<size_t>(info.channels), n = s.size() / ch;
  const float scale = 1.0f / static_cast<float>(int64_t{1} << (info.bits - 1));
  out->left.samples.resize(n);
  out->right.samples.resize(n);
  for (size_t i = 0; i < n; i++) {
    out->left.samples[i] = static_cast<float>(s[i * ch]) * scale;
    out->right.samples[i] = static_cast<float>(s[i * ch + (ch > 1 ? 1 : 0)]) * scale;
  }
  out->left.rate = out->right.rate = info.rate;
  out->path = where;
  return true;
}

MusicPlayer::~MusicPlayer() {
  for (auto& j : jobs_)
    if (j->worker.joinable()) j->worker.join();
}

void MusicPlayer::play(const std::string& path, bool loop) {
  const bool same = path == want_ && !paused_;
  loop_ = loop;
  if (same) return;  // asked again while it plays (a level restarted): it carries on
  if (paused_ && path == want_) {
    resume();
    return;
  }
  want_ = path;
  paused_ = false;
  pause_now_ = false;
  cut_ = true;
  auto job = std::make_unique<Job>();
  job->path = path;
  Job* j = job.get();
  j->worker = std::thread([j] {
    std::ifstream in(j->path, std::ios::binary);
    if (!in) {
      j->error = j->path +
                 ": the music file could not be opened; next: check that it is there, "
                 "or take the game in again";
    } else {
      const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
      auto t = std::make_unique<MusicTrack>();
      if (music_from_flac(bytes, j->path, t.get(), &j->error)) j->track = std::move(t);
    }
    j->done.store(true, std::memory_order_release);
  });
  jobs_.push_back(std::move(job));
}

void MusicPlayer::stop() {
  want_.clear();
  paused_ = pause_now_ = false;
  cut_ = true;
}

void MusicPlayer::pause() {
  if (want_.empty() || paused_) return;
  paused_ = true;
  pause_now_ = true;
}

void MusicPlayer::resume() {
  if (!paused_) return;
  paused_ = false;
  pause_now_ = true;
}

std::string MusicPlayer::take_error() { return std::exchange(error_, {}); }

int64_t MusicPlayer::position(const Synth& synth) const {
  if (!now_) return 0;
  const double played = (synth.now_s() - started_at_) * now_->left.rate;
  int64_t at = started_frame_ + static_cast<int64_t>(played < 0 ? 0 : played);
  const int64_t n = now_->left.frames();
  if (n > 0 && loop_) at %= n;
  return at > n ? n : at;
}

void MusicPlayer::silence(Synth& synth, float fade_s) {
  if (left_.valid()) synth.sample_stop(left_, fade_s);
  if (right_.valid()) synth.sample_stop(right_, fade_s);
  left_ = right_ = {};
}

void MusicPlayer::start(Synth& synth, int64_t from) {
  SamplePlay p;
  p.gain = gain_ * kMusicLevel;
  p.loop = loop_ ? Loop::whole : Loop::once;
  p.from = from;
  p.pan = -1;
  left_ = synth.sample(now_->left, p, bus_);
  p.pan = 1;
  right_ = synth.sample(now_->right, p, bus_);
  started_at_ = synth.now_s();
  started_frame_ = from;
  sent_gain_ = gain_;
}

void MusicPlayer::frame(Synth& synth) {
  if (cut_) {  // another track asked, or stop: the one playing fades and is kept until unread
    cut_ = false;
    silence(synth, kFadeS);
    if (now_) retired_.push_back(std::move(now_));
  }
  for (size_t i = 0; i < jobs_.size();) {
    Job& j = *jobs_[i];
    if (!j.done.load(std::memory_order_acquire)) {
      i++;
      continue;
    }
    j.worker.join();
    if (j.path == want_ && !now_ && i + 1 == jobs_.size()) {  // the latest ask: it plays
      if (j.track) {
        now_ = std::move(j.track);
        if (!paused_) start(synth, 0);
        paused_frame_ = 0;
      } else {
        error_ = j.error;
        want_.clear();
      }
    }
    jobs_.erase(jobs_.begin() + static_cast<std::ptrdiff_t>(i));
  }
  if (pause_now_ && now_) {
    pause_now_ = false;
    if (paused_) {
      paused_frame_ = position(synth);
      silence(synth, 0.05f);
    } else {
      start(synth, paused_frame_);
    }
  }
  if (now_ && !paused_ && left_.valid() && gain_ != sent_gain_) {
    synth.sample_set(left_, gain_ * kMusicLevel, -1, 0.05f);
    synth.sample_set(right_, gain_ * kMusicLevel, 1, 0.05f);
    sent_gain_ = gain_;
  }
  if (now_ && !loop_ && !paused_ && left_.valid() && !synth.sample_live(left_) &&
      !synth.sample_live(right_)) {  // played to its end: done
    left_ = right_ = {};
    retired_.push_back(std::move(now_));
    want_.clear();
  }
  for (size_t i = 0; i < retired_.size();) {
    if (synth.reads(retired_[i]->left) || synth.reads(retired_[i]->right)) {
      i++;
    } else {
      retired_.erase(retired_.begin() + static_cast<std::ptrdiff_t>(i));
    }
  }
}

}  // namespace ge::audio
