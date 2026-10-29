// The live capture (engine/capture/live.hpp), fed on a simulated clock so its file can be held to
// the times exactly while its worker runs on its own thread: the tap is the clock, so a flash
// presented on a click's sample lands within a frame of it at 60, 50 and 24 frames a second, and
// the file holds every tapped sample; a full queue drops pictures and the hand-in never waits; a
// resized window is cropped or padded to the file's size; a tap bigger than the ring is written
// as silence and the sound keeps its length; with the encoders the same file is a webm in time
// order.
//
// ge:summary Tests: the live capture's clock, sync, drops, resize, lost samples and webm order
// ge:keywords test live-capture tap clock sync flash click drop queue resize crop pad ring lost
// ge:keywords silence webm interleave
// ge:skill replay testing
// ge:test tests/capture_live_test.cpp
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "capture/codecs.hpp"
#include "capture/live.hpp"
#include "capture/mkv_read.hpp"
#include "check.hpp"
#include "lab/raster.hpp"

namespace gc = ge::capture;

namespace {

constexpr int kW = 64, kH = 48;
constexpr int kRate = 48000;
constexpr int kPull = 480;  // frames a device pull: 10 ms
constexpr uint64_t kMs = 1'000'000;

// The clicks: every 1.25 s from 0.437 s, each sounding for 5 ms and flashing for 50 ms.
constexpr double kFirst = 0.437, kPeriod = 1.25, kFlash = 0.05;

bool clicking(int64_t sample) {
  const double t = static_cast<double>(sample) / kRate - kFirst;
  if (t < 0) return false;
  return std::fmod(t, kPeriod) < 0.005;
}

bool flashing(double t) {
  if (t < kFirst) return false;
  return std::fmod(t - kFirst, kPeriod) < kFlash;
}

ge::lab::Picture solid(int w, int h, uint8_t v) { return ge::lab::create_picture(w, h, {v, v, v}); }

struct Played {
  std::vector<double> clicks;  // each click's first loud sample, ms
  std::vector<int64_t> lit;    // each picture's time when it is bright
  std::vector<int64_t> times;  // every picture's time
  int64_t samples = 0;
};

// The file's pictures and sound read back (raw I420 and float PCM).
Played read_back(const gc::ReadFile& f, const std::vector<uint8_t>& bytes) {
  Played p;
  bool loud = false;
  for (const gc::ReadBlock& b : f.blocks) {
    if (b.track == 1) {
      p.times.push_back(b.ms);
      double sum = 0;
      const size_t n = static_cast<size_t>(kW) * kH;
      for (size_t i = 0; i < n && i < b.size; i++) sum += bytes[b.offset + i];
      if (sum / static_cast<double>(n) > 128) p.lit.push_back(b.ms);
    } else {
      const size_t frames = b.size / 8;
      for (size_t i = 0; i < frames; i++) {
        float l = 0;
        std::memcpy(&l, &bytes[b.offset + i * 8], 4);
        const bool now = std::fabs(l) > 0.4f;
        if (now && !loud) {
          p.clicks.push_back(static_cast<double>(p.samples + static_cast<int64_t>(i)) * 1000.0 /
                             kRate);
        }
        loud = now;
      }
      p.samples += static_cast<int64_t>(frames);
    }
  }
  return p;
}

// `seconds` of play at `fps`: the device pulls 10 ms at a time, each frame is stamped when it is
// presented and handed in two frames later (the readback's delay).
std::vector<uint8_t> play(double fps, double seconds, const std::string& codec,
                          gc::LiveStats& stats) {
  gc::Live live;
  gc::LiveOptions o;
  o.width = kW;
  o.height = kH;
  o.fps = fps;
  o.codec = codec;
  o.queue = 1000;  // the test feeds faster than real time: nothing may drop
  std::string error;
  CHECK(live.open(o, 0, error));
  if (!error.empty()) std::fprintf(stderr, "%s\n", error.c_str());
  std::vector<float> chunk(size_t{2} * kPull);
  int64_t pulled = 0;
  struct InFlight {
    ge::lab::Picture p;
    int64_t ms;
  };
  std::vector<InFlight> flight;
  const auto frames = static_cast<int64_t>(seconds * fps);
  for (int64_t k = 0; k < frames; k++) {
    const double t = static_cast<double>(k) / fps;
    const auto now = static_cast<uint64_t>(std::llround(t * 1e9));
    // The device has pulled everything up to now.
    while (static_cast<uint64_t>(pulled) * 1'000'000'000 / kRate <= now) {
      for (int i = 0; i < kPull; i++) {
        const float v = clicking(pulled + i) ? 0.8f : 0.0f;
        chunk[2 * static_cast<size_t>(i)] = chunk[2 * static_cast<size_t>(i) + 1] = v;
      }
      live.audio(chunk.data(), kPull,
                 static_cast<uint64_t>(pulled) * 1'000'000'000 / static_cast<uint64_t>(kRate));
      pulled += kPull;
    }
    flight.push_back({solid(kW, kH, flashing(t) ? 235 : 16), live.stamp(now)});
    if (flight.size() > 2) {
      CHECK(live.picture(flight.front().p, flight.front().ms));
      flight.erase(flight.begin());
    }
  }
  for (InFlight& f : flight) CHECK(live.picture(f.p, f.ms));
  CHECK(live.close(error));
  if (!error.empty()) std::fprintf(stderr, "%s\n", error.c_str());
  stats = live.stats();
  CHECK(stats.audio_frames == pulled);
  return live.memory();
}

}  // namespace

TEST(the_tap_is_the_clock_so_each_flash_lands_within_a_frame_of_its_click) {
  for (const double fps : {60.0, 50.0, 24.0}) {
    gc::LiveStats st;
    const std::vector<uint8_t> bytes = play(fps, 4.0, "", st);
    std::string error;
    const auto f = gc::read_mkv(bytes, error);
    CHECK(f.has_value());
    if (!f) continue;
    CHECK(f->doc_type == "matroska");
    const Played p = read_back(*f, bytes);
    CHECK(static_cast<int64_t>(p.times.size()) == static_cast<int64_t>(4.0 * fps));
    CHECK(p.samples == st.audio_frames);  // every tapped sample is in the file
    CHECK(p.clicks.size() == 3);
    CHECK(st.dropped == 0);
    // Each picture at its presented time, to the millisecond.
    for (size_t k = 0; k < p.times.size(); k++) {
      CHECK(p.times[k] == static_cast<int64_t>(static_cast<double>(k) * 1000.0 / fps));
    }
    // The first bright picture at or after each click, less than a frame later.
    for (const double c : p.clicks) {
      int64_t first = -1;
      for (const int64_t ms : p.lit) {
        if (static_cast<double>(ms) + 1 >= c) {
          first = ms;
          break;
        }
      }
      CHECK(first >= 0);
      const double lag = static_cast<double>(first) - c;
      if (!(lag > -1.0 && lag < 1000.0 / fps)) {
        std::fprintf(stderr, "fps %.0f: click at %.2f ms, flash at %lld ms\n", fps, c,
                     static_cast<long long>(first));
      }
      CHECK(lag > -1.0 && lag < 1000.0 / fps);
    }
  }
}

TEST(a_full_queue_drops_pictures_and_the_hand_in_never_waits) {
  gc::Live live;
  gc::LiveOptions o;
  o.width = 640;
  o.height = 360;
  o.codec = "";
  o.sound = false;
  o.queue = 1;
  std::string error;
  CHECK(live.open(o, 0, error));
  double worst_ms = 0;
  int handed = 0, refused = 0;
  ge::lab::Picture p = solid(640, 360, 90);
  for (int k = 0; k < 200; k++) {
    if (p.rgba.size() != size_t{640} * 360 * 4) p = solid(640, 360, 90);
    const auto t0 = std::chrono::steady_clock::now();
    const bool took = live.picture(p, live.stamp(static_cast<uint64_t>(k) * 16 * kMs));
    worst_ms = std::max(
        worst_ms,
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
    (took ? handed : refused)++;
  }
  CHECK(live.close(error));
  const gc::LiveStats st = live.stats();
  CHECK(st.pictures == handed);
  CHECK(st.dropped == refused);
  CHECK(handed + refused == 200);
  // A swap of buffers under a lock, whatever the worker is doing (a generous bound for
  // sanitizers and shared runners).
  if (worst_ms >= 50) std::fprintf(stderr, "the slowest hand-in took %.2f ms\n", worst_ms);
  CHECK(worst_ms < 50);
}

TEST(a_resized_window_is_cropped_or_padded_to_the_file) {
  gc::Live live;
  gc::LiveOptions o;
  o.width = 33;  // rounded down to 32
  o.height = 24;
  o.codec = "";
  o.sound = false;
  o.queue = 10;
  std::string error;
  CHECK(live.open(o, 0, error));
  ge::lab::Picture big = solid(64, 48, 200);    // cropped: all bright
  ge::lab::Picture small = solid(16, 12, 200);  // padded: bright in the middle, black round it
  CHECK(live.picture(big, 0));
  CHECK(live.picture(small, 20));
  CHECK(live.close(error));
  const auto f = gc::read_mkv(live.memory(), error);
  CHECK(f.has_value());
  if (!f) return;
  CHECK(f->tracks.size() == 1 && f->tracks[0].width == 32 && f->tracks[0].height == 24);
  std::vector<const gc::ReadBlock*> v;
  for (const auto& b : f->blocks) v.push_back(&b);
  CHECK(v.size() == 2);
  if (v.size() != 2) return;
  const std::vector<uint8_t>& bytes = live.memory();
  const auto luma = [&](const gc::ReadBlock& b, int x, int y) {
    return bytes[b.offset + static_cast<size_t>(y) * 32 + static_cast<size_t>(x)];
  };
  CHECK(v[0]->size == size_t{32} * 24 * 3 / 2);
  CHECK(luma(*v[0], 0, 0) > 150 && luma(*v[0], 31, 23) > 150);
  CHECK(luma(*v[1], 16, 12) > 150);
  CHECK(luma(*v[1], 0, 0) < 30 && luma(*v[1], 31, 23) < 30);
}

TEST(a_tap_the_ring_cannot_hold_is_written_as_silence_and_the_sound_keeps_its_length) {
  gc::Live live;
  gc::LiveOptions o;
  o.width = kW;
  o.height = kH;
  o.codec = "";
  o.audio_seconds = 0.1;  // 8192 frames
  std::string error;
  CHECK(live.open(o, 0, error));
  std::vector<float> big(size_t{2} * 10000, 0.5f);
  live.audio(big.data(), 10000, 0);  // more than the ring: lost
  std::vector<float> small(size_t{2} * kPull, 0.25f);
  live.audio(small.data(), kPull, 300 * kMs);
  ge::lab::Picture p = solid(kW, kH, 16);
  CHECK(live.picture(p, live.stamp(300 * kMs)));
  CHECK(live.close(error));
  const gc::LiveStats st = live.stats();
  CHECK(st.lost_frames == 10000);
  CHECK(st.audio_frames == 10000 + kPull);
  const auto f = gc::read_mkv(live.memory(), error);
  CHECK(f.has_value());
  if (!f) return;
  int64_t samples = 0;
  for (const auto& b : f->blocks) {
    if (b.track == 2) samples += static_cast<int64_t>(b.size / 8);
  }
  CHECK(samples == 10000 + kPull);
}

TEST(with_the_encoders_the_capture_is_a_webm_in_time_order) {
  if (!gc::encoders_built()) {
    gc::Live live;
    gc::LiveOptions o;
    o.width = kW;
    o.height = kH;
    std::string error;
    CHECK(!live.open(o, 0, error));
    CHECK(error.find("GE_ENCODERS") != std::string::npos);
    return;
  }
  gc::LiveStats st;
  const std::vector<uint8_t> bytes = play(30, 3.0, "av1", st);
  std::string error;
  const auto f = gc::read_mkv(bytes, error);
  CHECK(f.has_value());
  if (!f) return;
  CHECK(f->doc_type == "webm");
  CHECK(st.pictures == 90);
  int64_t last = INT64_MIN;
  int video = 0, audio = 0;
  for (const auto& b : f->blocks) {
    if (b.ms < last)
      std::fprintf(stderr, "track %d at %lld after %lld\n", static_cast<int>(b.track),
                   static_cast<long long>(b.ms), static_cast<long long>(last));
    CHECK(b.ms >= last);
    last = b.ms;
    (b.track == 1 ? video : audio)++;
  }
  CHECK(video == 90);
  CHECK(audio >= 145);  // the 2.97 s tapped, as 20 ms packets
}
