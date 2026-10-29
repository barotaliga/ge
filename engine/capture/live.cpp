// The live capture (live.hpp): the tap's ring, the stamp, the hand-off and the worker.
//
// ge:summary The live capture's definitions: the lock-free tap, the seqlocked clock, the worker
// ge:keywords live-capture ring seqlock worker encode interleave crop pad silence
// ge:skill replay
// ge:test tests/capture_live_test.cpp
#include "capture/live.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <optional>

#include "capture/yuv.hpp"

namespace ge::capture {

namespace {

constexpr int kRate = kOpusRate;  // the tap's rate: the synth's, which Opus is fed at
constexpr size_t kCut = kOpusPacketSamples;
// How far the wall clock may carry the audio clock past its last tap, frames: a stalled device
// stops the clock instead of letting the pictures run ahead of the sound.
constexpr int64_t kCarryFrames = kRate / 10;
// A sound packet this much older than the last stamp is written with no picture after it yet.
constexpr int64_t kAudioLeadMs = 500;

size_t pow2_at_least(size_t n) {
  size_t p = 1;
  while (p < n) p <<= 1;
  return p;
}

// `src` cropped or padded (black, centred) to `w`x`h` into `dst`.
void fit(const lab::Picture& src, int w, int h, lab::Picture& dst) {
  dst.width = w;
  dst.height = h;
  dst.rgba.assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 4, 0);
  for (size_t i = 3; i < dst.rgba.size(); i += 4) dst.rgba[i] = 255;
  const int cw = std::min(w, src.width), ch = std::min(h, src.height);
  const int sx = (src.width - cw) / 2, sy = (src.height - ch) / 2;
  const int dx = (w - cw) / 2, dy = (h - ch) / 2;
  for (int y = 0; y < ch; y++) {
    std::memcpy(&dst.rgba[(static_cast<size_t>(dy + y) * static_cast<size_t>(w) + dx) * 4],
                &src.rgba[(static_cast<size_t>(sy + y) * static_cast<size_t>(src.width) + sx) * 4],
                static_cast<size_t>(cw) * 4);
  }
}

}  // namespace

Live::~Live() {
  std::string ignored;
  close(ignored);
}

bool Live::open(const LiveOptions& o, uint64_t now_ns, std::string& error) {
  if (open_) {
    error = "the live capture is already open";
    return false;
  }
  if (!o.codec.empty() && o.codec != "av1") {
    error = "no codec `" + o.codec + "`; the live capture's codecs are av1 and none (uncompressed)";
    return false;
  }
  opt_ = o;
  opt_.width = o.width / 2 * 2;
  opt_.height = o.height / 2 * 2;
  if (opt_.width < 2 || opt_.height < 2 || !(o.fps > 0)) {
    error = "the live capture needs a size of at least 2x2 and a frame rate over 0";
    return false;
  }
  opt_.queue = std::max(1, o.queue);
  coded_ = o.codec == "av1";
  MatroskaOptions m;
  VideoTrack v;
  v.width = opt_.width;
  v.height = opt_.height;
  v.fps = o.fps;
  if (coded_) {
    Av1Options a;
    a.width = opt_.width;
    a.height = opt_.height;
    a.fps = o.fps;
    a.crf = o.crf;
    a.speed = o.speed;
    a.threads = o.threads;
    if (!av1_.open(a, error)) return false;
    v.codec = "V_AV1";
    v.codec_private = av1_codec_private(av1_.sequence_header());
  }
  m.video = v;
  if (o.sound) {
    AudioTrack a;
    a.rate = kRate;
    if (coded_) {
      if (!opus_.open(o.audio_bitrate, error)) return false;
      a.codec = "A_OPUS";
      a.bits = 0;
      a.codec_private = opus_.codec_private();
      a.codec_delay_ns = std::llround(opus_.preskip() * 1e9 / kRate);
      a.seek_pre_roll_ns = kOpusSeekPreRollNs;
    }
    m.audio = a;
    ring_frames_ = pow2_at_least(static_cast<size_t>(std::max(0.1, o.audio_seconds) * kRate));
    ring_.assign(2 * ring_frames_, 0.0f);
    pcm_hold_.reserve(2 * ring_frames_);
  }
  doc_type_ = std::string(doc_type_of(m));
  if (!mux_.open(o.file, m, error)) return false;
  open_ns_ = now_ns;
  anchor_frames_.store(0);
  anchor_ns_.store(now_ns);
  last_stamp_ = -1;
  closing_ = false;
  open_ = true;
  tapping_.store(o.sound, std::memory_order_release);
  worker_ = std::thread([this] { work(); });
  return true;
}

void Live::audio(const float* stereo, int frames, uint64_t now_ns) {
  if (frames <= 0 || !tapping_.load(std::memory_order_acquire)) return;
  const uint64_t before = tapped_.load(std::memory_order_relaxed);
  // The clock's anchor: the samples before this tap were all heard by `now_ns`.
  anchor_seq_.fetch_add(1, std::memory_order_acq_rel);
  anchor_frames_.store(before, std::memory_order_relaxed);
  anchor_ns_.store(now_ns, std::memory_order_relaxed);
  anchor_seq_.fetch_add(1, std::memory_order_release);
  tapped_once_.store(true, std::memory_order_release);
  tapped_.store(before + static_cast<uint64_t>(frames), std::memory_order_release);

  const uint64_t w = written_.load(std::memory_order_relaxed);
  const uint64_t r = read_.load(std::memory_order_acquire);
  const auto n = static_cast<uint64_t>(frames);
  if (n > ring_frames_ - (w - r)) {  // the worker is behind by the whole ring: the tap is lost
    lost_.fetch_add(n, std::memory_order_relaxed);
    lost_total_.fetch_add(n, std::memory_order_relaxed);
    return;
  }
  const size_t at = static_cast<size_t>(w) & (ring_frames_ - 1);
  const size_t first = std::min(static_cast<size_t>(n), ring_frames_ - at);
  std::memcpy(&ring_[2 * at], stereo, 2 * first * sizeof(float));
  if (first < n) {
    std::memcpy(ring_.data(), stereo + 2 * first,
                2 * (static_cast<size_t>(n) - first) * sizeof(float));
  }
  written_.store(w + n, std::memory_order_release);
}

int64_t Live::stamp(uint64_t now_ns) {
  int64_t ms = 0;
  if (!opt_.sound || !tapped_once_.load(std::memory_order_acquire)) {
    ms = static_cast<int64_t>((now_ns - std::min(now_ns, open_ns_)) / 1'000'000);
  } else {
    uint64_t frames = 0, at = 0;
    for (;;) {
      const uint32_t s0 = anchor_seq_.load(std::memory_order_acquire);
      frames = anchor_frames_.load(std::memory_order_relaxed);
      at = anchor_ns_.load(std::memory_order_relaxed);
      if ((s0 & 1) == 0 && anchor_seq_.load(std::memory_order_acquire) == s0) break;
    }
    const int64_t since_ns = now_ns > at ? static_cast<int64_t>(now_ns - at) : 0;
    const int64_t carry = std::min(kCarryFrames, since_ns * kRate / 1'000'000'000);
    ms = (static_cast<int64_t>(frames) + carry) * 1000 / kRate;
  }
  ms = std::max(ms, last_stamp_ + 1);
  last_stamp_ = ms;
  stamped_ms_.store(ms, std::memory_order_release);
  return ms;
}

bool Live::picture(lab::Picture& p, int64_t ms) {
  if (!open_) return false;
  {
    std::lock_guard lk(mu_);
    if (static_cast<int>(pending_.size()) >= opt_.queue) {
      dropped_++;
      return false;
    }
    pending_.push_back({std::move(p), ms});
    p = lab::Picture{};
    if (!spares_.empty()) {
      p.rgba = std::move(spares_.back());
      spares_.pop_back();
    }
  }
  wake_.notify_one();
  return true;
}

void Live::work() {
  std::unique_lock lk(mu_);
  for (;;) {
    wake_.wait_for(lk, std::chrono::milliseconds(5),
                   [this] { return closing_ || !pending_.empty(); });
    const bool closing = closing_;
    std::optional<Pending> job;
    if (!pending_.empty()) {
      job.emplace(std::move(pending_.front()));
      pending_.pop_front();
    }
    // What is still to come, read together: a picture handed in after this was stamped before
    // `stamped_at_`, so within the readback ring's few frames of it.
    waiting_ms_ = pending_.empty() ? INT64_MAX : pending_.front().ms;
    stamped_at_ = stamped_ms_.load(std::memory_order_acquire);
    lk.unlock();
    double took = -1;
    if (worker_error_.empty()) {
      std::string e;
      const auto t0 = std::chrono::steady_clock::now();
      bool ok = drain_audio(false, e);
      if (ok && job) {
        ok = encode_picture(*job, e);
        took = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                   .count();
      }
      if (ok) ok = write(false, e);
      if (!ok) worker_error_ = e;
    }
    lk.lock();
    if (job) spares_.push_back(std::move(job->picture.rgba));
    if (took >= 0) {
      encode_ms_max_ = std::max(encode_ms_max_, took);
      encode_ms_sum_ += took;
    }
    if (closing && pending_.empty()) break;
  }
}

bool Live::cut_audio(const float* stereo, size_t frames, std::string& error) {
  if (coded_) return opus_.push(stereo, frames, held_.audio, error);
  pcm_hold_.insert(pcm_hold_.end(), stereo, stereo + 2 * frames);
  return true;
}

bool Live::drain_audio(bool all, std::string& error) {
  if (!opt_.sound) return true;
  const uint64_t r = read_.load(std::memory_order_relaxed);
  const uint64_t w = written_.load(std::memory_order_acquire);
  uint64_t at = r;
  while (at < w) {
    const size_t i = static_cast<size_t>(at) & (ring_frames_ - 1);
    const size_t n = std::min(static_cast<size_t>(w - at), ring_frames_ - i);
    if (!cut_audio(&ring_[2 * i], n, error)) return false;
    at += n;
  }
  read_.store(w, std::memory_order_release);
  // What the ring could not hold, as silence where the worker noticed it.
  if (const uint64_t lost = lost_.exchange(0, std::memory_order_relaxed); lost > 0) {
    const std::vector<float> silence(2 * static_cast<size_t>(lost), 0.0f);
    if (!cut_audio(silence.data(), static_cast<size_t>(lost), error)) return false;
  }
  if (coded_) return !all || opus_.finish(held_.audio, error);
  // Raw PCM in 20 ms blocks, the last one short at the close.
  size_t used = 0;
  while (pcm_hold_.size() - used >= 2 * kCut || (all && pcm_hold_.size() > used)) {
    const size_t n = std::min(kCut, (pcm_hold_.size() - used) / 2);
    Packet k;
    k.ms = cut_frames_ * 1000 / kRate;
    k.data.resize(2 * n * sizeof(float));
    // Little-endian float, as A_PCM/FLOAT/IEEE is (every platform the engine runs on is).
    std::memcpy(k.data.data(), &pcm_hold_[used], k.data.size());
    held_.audio.push_back(std::move(k));
    used += 2 * n;
    cut_frames_ += static_cast<int64_t>(n);
  }
  pcm_hold_.erase(pcm_hold_.begin(), pcm_hold_.begin() + static_cast<std::ptrdiff_t>(used));
  return true;
}

bool Live::encode_picture(Pending& p, std::string& error) {
  const lab::Picture* src = &p.picture;
  if (p.picture.width != opt_.width || p.picture.height != opt_.height) {
    fit(p.picture, opt_.width, opt_.height, fitted_);
    src = &fitted_;
  }
  to_i420(*src, yuv_);
  if (coded_) {
    held_.fed(p.ms);
    if (!av1_.encode(yuv_, p.ms, held_.video, error)) return false;
  } else {
    held_.video.push_back(Packet{yuv_, p.ms, true});
  }
  newest_picture_ms_ = p.ms;
  pictures_++;
  stats_pictures_.store(pictures_, std::memory_order_relaxed);
  stats_last_ms_.store(p.ms, std::memory_order_relaxed);
  return true;
}

bool Live::write(bool all, std::string& error) {
  const int64_t audio_ms = !opt_.sound ? INT64_MAX
                           : coded_    ? opus_.next_ms()
                                       : cut_frames_ * 1000 / kRate;
  // A picture still to be encoded comes no sooner than the oldest waiting, or than half a
  // second before the last stamp when none is waiting (one still in the readback ring is newer).
  const int64_t stamped = stamped_at_;
  int64_t next_video = newest_picture_ms_ == INT64_MIN ? INT64_MIN : newest_picture_ms_ + 1;
  if (!all && stamped != INT64_MIN) {
    next_video = std::max(next_video, std::min(waiting_ms_, stamped - kAudioLeadMs));
  }
  return held_.write(mux_, next_video, audio_ms, all, error);
}

bool Live::close(std::string& error) {
  if (!open_) return true;
  tapping_.store(false, std::memory_order_release);
  {
    std::lock_guard lk(mu_);
    closing_ = true;
  }
  wake_.notify_one();
  worker_.join();
  open_ = false;
  if (!worker_error_.empty()) {
    error = worker_error_;
    return false;
  }
  if (!drain_audio(true, error)) return false;
  if (coded_ && !av1_.flush(held_.video, error)) return false;
  if (!write(true, error)) return false;
  const int64_t frame = std::llround(1000.0 / opt_.fps);
  int64_t end = newest_picture_ms_ == INT64_MIN ? 0 : newest_picture_ms_ + frame;
  if (opt_.sound) {
    end = std::max(end, static_cast<int64_t>(tapped_.load() * 1000 / static_cast<uint64_t>(kRate)));
  }
  return mux_.close(end, error);
}

LiveStats Live::stats() const {
  LiveStats s;
  s.pictures = stats_pictures_.load(std::memory_order_relaxed);
  s.last_ms = stats_last_ms_.load(std::memory_order_relaxed);
  s.audio_frames = static_cast<int64_t>(tapped_.load(std::memory_order_relaxed));
  s.lost_frames = static_cast<int64_t>(lost_total_.load(std::memory_order_relaxed));
  {
    std::lock_guard lk(mu_);
    s.dropped = dropped_;
    s.encode_ms_max = encode_ms_max_;
    s.encode_ms_mean = s.pictures > 0 ? encode_ms_sum_ / static_cast<double>(s.pictures) : 0;
  }
  s.bytes = open_ ? 0 : mux_.written();  // the worker writes while open
  return s;
}

}  // namespace ge::capture
