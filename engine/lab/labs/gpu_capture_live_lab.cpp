// LAB `gpu-capture-live`: the live capture (capture/live.hpp, gpu/readback_ring.hpp) run the way
// a game runs it, in real time: a frame loop paced at --fps draws the GPU test scene into an
// offscreen target (a window's frame, headless), flashing it white for 50 ms from each click,
// copies every frame back through the ring of readbacks and hands it to the capture; an audio
// thread pulls 10 ms of sound every 10 ms, as a device does, clicking every 1.25 s, and taps it.
// The file is then decoded (AV1 and Opus, or raw I420 and float PCM without the encoders) and
// held to: every frame handed in is in the file, every tapped sample, each click's flash less
// than a frame after it, and no frame waiting on the capture (the frame's capture call, the copy
// requested and the frames back handed over, stays far under a frame).
//
// Everything timed is `measured`, so the digest is the flags and the counts that do not depend
// on the machine. Without an adapter it notes that and draws placeholders; GE_GPU_REQUIRE=1
// fails instead. Built only with GE_GPU.
//
//   make lab LAB=gpu-capture-live ARGS="--seconds=30 --fps=60 --width=1280 --height=720"
//
// ge:summary Lab: the live capture in real time: no frame stalled, every frame and click in sync
// ge:keywords lab live-capture readback-ring tap real-time stall drop sync flash click av1 opus
// ge:keywords capture-cost frame-pace webm
// ge:skill replay labs
// ge:test tests/capture_live_test.cpp
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "capture/codecs.hpp"
#include "capture/live.hpp"
#include "capture/mkv_read.hpp"
#include "capture/yuv.hpp"
#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/readback_ring.hpp"
#include "gpu/target.hpp"
#include "gpu/test_scene.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "lab/stats.hpp"

namespace {

namespace gl = ge::lab;
namespace gc = ge::capture;
using gl::Det;

constexpr int kRate = 48000;
constexpr int kPull = 480;  // a device's pull: 10 ms
constexpr double kFirst = 0.437, kPeriod = 1.25, kClick = 0.005, kFlash = 0.05;
// The most a frame's capture call may take before it counts as the frame waiting, ms.
constexpr double kStallMs = 25;

bool env_on(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool on = _dupenv_s(&v, &n, name) == 0 && v && std::strcmp(v, "1") == 0;
  std::free(v);
  return on;
#else
  const char* v = std::getenv(name);
  return v && std::strcmp(v, "1") == 0;
#endif
}

uint64_t now_ns() {
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count());
}

double phase(double t) { return t < kFirst ? -1 : std::fmod(t - kFirst, kPeriod); }

std::string num(double v, int digits) {
  char b[40];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

// The file decoded: each picture's time, its centre's brightness and a small copy; each click's
// first loud sample.
struct Decoded {
  std::vector<int64_t> times;
  std::vector<double> bright;
  std::vector<gl::Picture> thumbs;
  std::vector<double> clicks;  // ms
  int64_t samples = 0;
  std::string error;
};

Decoded decode(const std::vector<uint8_t>& bytes, bool coded, int w, int h) {
  Decoded d;
  const auto f = gc::read_mkv(bytes, d.error);
  if (!f) return d;
  gc::Av1Decoder av1;
  gc::OpusDecoder opus;
  if (coded && (!av1.open(d.error) || !opus.open(d.error))) return d;
  int64_t preskip = 0;
  for (const auto& t : f->tracks) {
    if (t.type == 2 && t.codec_delay_ns > 0) preskip = t.codec_delay_ns * kRate / 1'000'000'000;
  }
  std::vector<uint8_t> i420;
  std::vector<float> pcm;
  bool loud = false;
  int64_t heard = 0;  // samples decoded, the pre-skip included
  for (const auto& b : f->blocks) {
    const std::span<const uint8_t> data(&bytes[b.offset], b.size);
    if (b.track == 1) {
      int pw = w, ph = h;
      bool got = true;
      if (coded) {
        if (!av1.decode(data, i420, pw, ph, got, d.error)) return d;
      } else {
        i420.assign(data.begin(), data.end());
      }
      if (!got) continue;
      double sum = 0;
      int n = 0;
      for (int y = ph * 3 / 8; y < ph * 5 / 8; y++) {
        for (int x = pw * 3 / 8; x < pw * 5 / 8; x++, n++) {
          sum += i420[static_cast<size_t>(y) * static_cast<size_t>(pw) + static_cast<size_t>(x)];
        }
      }
      d.times.push_back(b.ms);
      d.bright.push_back(n ? sum / n : 0);
      d.thumbs.push_back(gl::resized(gc::from_i420(i420.data(), pw, ph), 96, 96 * ph / pw));
    } else {
      pcm.clear();
      if (coded) {
        if (!opus.decode(data, pcm, d.error)) return d;
      } else {
        pcm.resize(b.size / 4);
        std::memcpy(pcm.data(), data.data(), pcm.size() * 4);
      }
      for (size_t i = 0; i + 1 < pcm.size(); i += 2, heard++) {
        if (heard < preskip) continue;
        const bool now = std::fabs(pcm[i]) > 0.3f;
        if (now && !loud) {
          d.clicks.push_back(static_cast<double>(heard - preskip) * 1000.0 / kRate);
        }
        loud = now;
      }
    }
  }
  d.samples = heard - preskip;
  return d;
}

void placeholder(gl::Lab& lab, const char* id, const char* title) {
  gl::Picture none = gl::create_picture(480, 120, {40, 40, 48});
  gl::draw_text(none, 8, 8, "NO GPU ADAPTER", {230, 230, 230});
  lab.shot({id, title, "No GPU adapter on this machine: a placeholder.", "nothing", {}}, none,
           Det::measured);
}

void run(gl::Lab& lab) {
  const double seconds = lab.arg_num("seconds");
  const double fps = lab.arg_num("fps");
  int width = static_cast<int>(lab.arg_num("width"));
  int height = static_cast<int>(lab.arg_num("height"));
  if (!(seconds > 0 && seconds <= 600) || !(fps >= 10 && fps <= 240) || width < 16 || height < 16 ||
      width > 3840 || height > 2160) {
    lab.fail("--seconds must be 0..600, --fps 10..240 and the size 16x16..3840x2160",
             "next: rerun with --seconds=30 --fps=60 --width=640 --height=360.");
    return;
  }
  const bool coded = gc::encoders_built();
  if (!coded) {  // raw I420 is kept in memory: a smaller frame
    width = std::max(16, width / 4 / 2 * 2);
    height = std::max(16, height / 4 / 2 * 2);
    lab.note("This build has no encoders (GE_ENCODERS is OFF): raw I420 and float PCM at " +
             std::to_string(width) + "x" + std::to_string(height) + ".");
  }
  lab.metric("seconds", seconds, "s");
  lab.metric("fps", fps, "Hz");
  const auto clicks_due = static_cast<int>(std::floor((seconds - kFirst - 0.1) / kPeriod)) + 1;
  const auto frames = static_cast<int64_t>(seconds * fps);
  // The same deterministic metrics with or without an adapter, so the digest is the same.
  lab.metric("frames.drawn", static_cast<double>(frames), "count");
  lab.metric("clicks.at-least", clicks_due, "count");

  auto device = ge::gpu::create_device({.label = "gpu-capture-live", .validation = 1});
  if (!device) {
    if (env_on("GE_GPU_REQUIRE")) {
      lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
               "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
    }
    lab.note("No GPU adapter on this machine: the capture is not run.");
    placeholder(lab, "frames-at-a-click", "FRAMES AT A CLICK");
    placeholder(lab, "capture-cost", "THE CAPTURE'S COST A FRAME");
    return;
  }
  const auto& r = device->report;
  lab.note("adapter: " + r.name + " / " + r.backend + (r.software ? " (software)" : ""));

  ge::gpu::PipelineSet manifest;
  const uint32_t scene =
      ge::gpu::add_test_scene(*device, manifest, wgpu::TextureFormat::RGBA8Unorm);
  if (!manifest.finish(*device)) {
    lab.fail("the test scene's pipeline did not build", "Read the gpu: lines above. Skill: gpu.");
    return;
  }
  ge::gpu::end_load(*device);
  const ge::gpu::Target target = ge::gpu::create_target(
      *device, "capture-live.frame", static_cast<uint32_t>(width), static_cast<uint32_t>(height));

  gc::Live live;
  gc::LiveOptions o;
  o.width = width;
  o.height = height;
  o.fps = fps;
  o.codec = coded ? "av1" : "";
  std::string error;
  if (!live.open(o, now_ns(), error)) {
    lab.fail("the capture did not open: " + error, "Read capture/live.cpp's open. Skill: replay.");
    return;
  }

  // The device: 10 ms every 10 ms on a thread of its own, clicking on its schedule.
  std::atomic<bool> stop{false};
  std::thread speaker([&] {
    std::vector<float> chunk(2 * kPull);
    int64_t pulled = 0;
    auto next = std::chrono::steady_clock::now();
    while (!stop.load()) {
      next += std::chrono::milliseconds(10);
      std::this_thread::sleep_until(next);
      for (int i = 0; i < kPull; i++) {
        const double ph = phase(static_cast<double>(pulled + i) / kRate);
        const float v = ph >= 0 && ph < kClick ? 0.8f : 0.0f;
        chunk[2 * static_cast<size_t>(i)] = chunk[2 * static_cast<size_t>(i) + 1] = v;
      }
      live.audio(chunk.data(), kPull, now_ns());
      pulled += kPull;
    }
  });

  ge::gpu::ReadbackRing ring(4);
  ge::gpu::Image back;
  gl::Picture handed;
  int64_t handed_in = 0, refused = 0;
  const auto hand = [&](ge::gpu::Image& img, uint64_t ms) {
    handed.width = static_cast<int>(img.width);
    handed.height = static_cast<int>(img.height);
    handed.rgba.swap(img.rgba);
    (live.picture(handed, static_cast<int64_t>(ms)) ? handed_in : refused)++;
    img.rgba.swap(handed.rgba);
  };
  std::vector<double> cost_ms, interval_ms, at_s, interval_at;
  const auto start = std::chrono::steady_clock::now();
  uint64_t last = now_ns();
  for (int64_t k = 0; k < frames; k++) {
    std::this_thread::sleep_until(
        start + std::chrono::nanoseconds(static_cast<int64_t>(static_cast<double>(k) * 1e9 / fps)));
    const uint64_t now = now_ns();
    const int64_t ms = live.stamp(now);
    const double ph = phase(static_cast<double>(ms) / 1000.0);
    wgpu::CommandEncoder encoder = device->device.CreateCommandEncoder();
    if (ph >= 0 && ph < kFlash) {
      wgpu::RenderPassColorAttachment colour{};
      colour.view = target.view;
      colour.loadOp = wgpu::LoadOp::Clear;
      colour.storeOp = wgpu::StoreOp::Store;
      colour.clearValue = {1, 1, 1, 1};
      wgpu::RenderPassDescriptor pass{};
      pass.colorAttachmentCount = 1;
      pass.colorAttachments = &colour;
      encoder.BeginRenderPass(&pass).End();
    } else {
      ge::gpu::draw_test_scene(encoder, manifest.render(scene), target.view, nullptr);
    }
    wgpu::CommandBuffer commands = encoder.Finish();
    device->queue.Submit(1, &commands);
    const uint64_t t0 = now_ns();
    ring.capture(*device, target, static_cast<uint64_t>(ms));
    ring.poll(*device, back, hand);
    const uint64_t t1 = now_ns();
    device->instance.ProcessEvents();
    cost_ms.push_back(static_cast<double>(t1 - t0) * 1e-6);
    at_s.push_back(static_cast<double>(k) / fps);
    if (k > 0) {
      interval_ms.push_back(static_cast<double>(now - last) * 1e-6);
      interval_at.push_back(static_cast<double>(k) / fps);
    }
    last = now;
  }
  ring.drain(*device, back, hand);
  stop.store(true);
  speaker.join();
  if (!live.close(error)) {
    lab.fail("the capture did not close: " + error, "Read capture/live.cpp. Skill: replay.");
    return;
  }
  const gc::LiveStats st = live.stats();
  const Decoded dec = decode(live.memory(), coded, width, height);
  if (!dec.error.empty()) {
    lab.fail("the file did not decode: " + dec.error, "Read capture/live.cpp. Skill: replay.");
    return;
  }

  // Each click's flash: the first bright frame from it on.
  std::vector<double> lags;
  for (const double c : dec.clicks) {
    for (size_t i = 0; i < dec.times.size(); i++) {
      if (static_cast<double>(dec.times[i]) + 1 >= c && dec.bright[i] > 200) {
        lags.push_back(static_cast<double>(dec.times[i]) - c);
        break;
      }
    }
  }
  const gl::Summary cost = gl::summarize(cost_ms);
  const gl::Summary pace = gl::summarize(interval_ms);
  const double worst_lag = lags.empty() ? 0 : *std::max_element(lags.begin(), lags.end());
  const double least_lag = lags.empty() ? 0 : *std::min_element(lags.begin(), lags.end());
  lab.metric("frames.in-file", static_cast<double>(dec.times.size()), "count", Det::measured);
  lab.metric("frames.dropped-by-ring", static_cast<double>(ring.dropped()), "count", Det::measured);
  lab.metric("frames.dropped-by-worker", static_cast<double>(st.dropped), "count", Det::measured);
  lab.metric("capture-call.max", cost.max, "ms", Det::measured);
  lab.metric("capture-call.p99", cost.p99, "ms", Det::measured);
  lab.metric("capture-call.mean", cost.mean, "ms", Det::measured);
  lab.metric("frame-interval.max", pace.max, "ms", Det::measured);
  lab.metric("encode.mean", st.encode_ms_mean, "ms", Det::measured);
  lab.metric("encode.max", st.encode_ms_max, "ms", Det::measured);
  lab.metric("sound.seconds", static_cast<double>(st.audio_frames) / kRate, "s", Det::measured);
  lab.metric("sound.lost", static_cast<double>(st.lost_frames), "frames", Det::measured);
  lab.metric("sync.clicks-heard", static_cast<double>(dec.clicks.size()), "count", Det::measured);
  lab.metric("sync.worst-lag", worst_lag, "ms", Det::measured);
  lab.metric("file", static_cast<double>(st.bytes) / 1e6, "MB", Det::measured);
  lab.table(
      {"the capture's frames and its cost a frame",
       {"drawn", "in the file", "dropped (ring)", "dropped (worker)", "call mean ms", "call p99 ms",
        "call max ms", "encode mean ms", "file MB"},
       {{std::to_string(frames), std::to_string(dec.times.size()), std::to_string(ring.dropped()),
         std::to_string(st.dropped), num(cost.mean, 3), num(cost.p99, 3), num(cost.max, 3),
         num(st.encode_ms_mean, 2), num(static_cast<double>(st.bytes) / 1e6, 2)}}},
      Det::measured);

  if (static_cast<int64_t>(dec.times.size()) != handed_in) {
    lab.fail(std::to_string(handed_in) + " frames were handed in and " +
                 std::to_string(dec.times.size()) + " are in the file",
             "Read capture/live.cpp's worker and close. Skill: replay.");
  }
  if (!coded && dec.samples != st.audio_frames) {
    lab.fail(std::to_string(st.audio_frames) + " samples were tapped and " +
                 std::to_string(dec.samples) + " are in the file",
             "Read capture/live.cpp's drain_audio. Skill: replay.");
  }
  // Dawn's null backend draws nothing: every frame reads back black, so there is no flash.
  const bool draws = r.backend != "null";
  if (!draws)
    lab.note("The adapter is Dawn's null backend, which draws nothing: no flash to time.");
  if (static_cast<int>(dec.clicks.size()) < clicks_due ||
      (draws && static_cast<int>(lags.size()) != static_cast<int>(dec.clicks.size()))) {
    lab.fail(std::to_string(clicks_due) + " clicks were due, " + std::to_string(dec.clicks.size()) +
                 " were heard and " + std::to_string(lags.size()) + " had a flash after them",
             "Open previews/gpu-capture-live/frames-at-a-click.png. Skill: replay.");
  }
  if (!lags.empty() && (least_lag <= -1.5 || worst_lag >= 1000.0 / fps + 1)) {
    lab.fail("a flash landed " + num(least_lag, 1) + " to " + num(worst_lag, 1) +
                 " ms from its click, outside one frame",
             "Read capture/live.cpp's stamp. Skill: replay.");
  }
  if (cost.max > kStallMs) {
    lab.fail("a frame's capture call took " + num(cost.max, 2) + " ms: the frame waited",
             "Read gpu/readback_ring.hpp and capture/live.cpp's picture. Skill: replay.");
  }

  // The frames at the second click (or the first), and its sound under them.
  const double click = dec.clicks.size() > 1 ? dec.clicks[1]
                       : dec.clicks.empty()  ? 0
                                             : dec.clicks[0];
  size_t at = 0;
  while (at < dec.times.size() && static_cast<double>(dec.times[at]) < click) at++;
  const size_t from = at >= 3 ? at - 3 : 0;
  const size_t to = std::min(dec.times.size(), from + 7);
  const int tw = dec.thumbs.empty() ? 96 : dec.thumbs[0].width;
  const int th = dec.thumbs.empty() ? 54 : dec.thumbs[0].height;
  gl::Picture strip =
      gl::create_picture(static_cast<int>(to - from) * (tw + 8) + 8, th + 70, {24, 24, 28});
  const double t_from = from < dec.times.size() ? static_cast<double>(dec.times[from]) : 0;
  const double px_per_ms = (tw + 8) * fps / 1000.0;
  for (size_t i = from; i < to; i++) {
    const int x = 8 + static_cast<int>(i - from) * (tw + 8);
    gl::blit(strip, dec.thumbs[i], x, 8);
    gl::draw_text(strip, x, th + 12, std::to_string(dec.times[i]) + " MS", {220, 220, 220});
  }
  const double cx = 8 + (click - t_from) * px_per_ms;
  gl::draw_line(strip, cx, th + 26, cx, th + 62, {255, 80, 60}, 2);
  gl::fill_rect(strip, cx, th + 34, kClick * 1000 * px_per_ms, 20, {255, 80, 60});
  gl::draw_text(strip, cx + 6, th + 58, "CLICK " + num(click, 1) + " MS", {255, 160, 140});
  lab.shot({"frames-at-a-click",
            "FRAMES AT A CLICK, AS THE FILE HAS THEM",
            "Seven frames decoded from the live capture around a click, each with its time, and "
            "the click under them where the decoded sound has it (red). The frame loop flashes "
            "white for 50 ms from each click's time on the tap's clock.",
            "the first white frame is the first frame at or after the red line: less than a "
            "frame late, never early",
            {{"fps", num(fps, 0)},
             {"size", std::to_string(width) + "x" + std::to_string(height)},
             {"codec", coded ? "av1+opus" : "raw"}}},
           strip, Det::measured);

  gl::LineChart lc;
  lc.frame = {"THE CAPTURE'S COST A FRAME, AND THE FRAME'S PACE",
              std::to_string(width) + "x" + std::to_string(height) + " at " + num(fps, 0) +
                  " fps for " + num(seconds, 0) + " s, " + (coded ? "AV1 and Opus" : "raw"),
              "t (s)", "ms"};
  lc.traces.push_back({"capture call (copy requested, frames back handed in)", at_s, cost_ms});
  lc.traces.push_back({"frame interval", interval_at, interval_ms});
  lc.rules.push_back(1000.0 / fps);
  const gl::Chart ch = gl::line_chart(lc);
  lab.shot({"capture-cost",
            ch.title,
            "Each frame's capture call (the readback requested and the frames come back swapped "
            "into the capture) beside the frame's interval; the dashed line is one frame.",
            "the capture line flat near zero, far under the dashed line; no interval spikes "
            "where the capture is",
            {{"fps", num(fps, 0)}}},
           gl::to_picture(ch.plot), Det::measured);
}

const gl::LabSpec kSpec{
    "gpu-capture-live",
    gl::LabKind::scenario,
    "engine",
    "engine/capture/live.hpp, engine/gpu/readback_ring.hpp",
    "replay",
    "Does the live capture take every frame and its sound in real time, in sync, with no frame "
    "waiting on it?",
    {{"seconds", "30", "how long the frame loop runs"},
     {"fps", "60", "the frame loop's rate"},
     {"width", "640", "the frame's width, px (a quarter without the encoders)"},
     {"height", "360", "the frame's height, px"}},
    "--seconds=3 --fps=30 --width=320 --height=180",
    run,
};
GE_LAB(kSpec);

}  // namespace
