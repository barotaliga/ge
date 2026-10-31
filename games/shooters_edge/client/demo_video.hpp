// A RECORDED GAME TURNED INTO A VIDEO: a demo of any format (demo.hpp) played at its own pace on a
// fixed frame clock, each picture drawn offscreen by the window's own world frame (world_draw.hpp:
// the map, the entities and doors where the demo has them, the level's statics, the HUD off the
// demo's stats) and read back, and its sound (demo_sound.hpp: the pack's recordings the demo's
// sound messages start, the level's loops, the ambience) and the players' voices (demo_voice.hpp,
// from a `.qwdv` or `.mvdv`) rendered beside it, both written by engine/capture into one file:
// an `.mp4` (H.264 or HEVC from the platform's encoder), a `.webm` (AV1 and Opus) or an `.mkv`.
//
// THE CLOCK is the video's: picture k shows the demo as it stands once its clock has passed
// k / fps seconds (the demo frames up to there read, as `playdemo` reads them), and every sound a
// frame starts is played on the sample that frame's time falls on, so a shot heard and a shot seen
// are within a frame of each other whatever the machine's speed. The video's clock runs on through
// the levels: a level's start (whose own clock starts over) and a stall between two frames longer
// than `kMostDemoGap` move it on by no more than that, so a long pause in a recording does not
// become a long still in the video.
//
// The run's report (`DemoVideoReport`) says what went in: pictures, seconds, levels, the sounds
// played and the names no pack had, the voices heard, the file's bytes and its codec.
//
// ge:summary A demo turned into a video: pictures on the GPU, its sounds and voices, one file
// ge:keywords demo-video demovideo export video mp4 webm mkv record demo-to-video frame-clock
// ge:keywords sound voice qwdv mvdv capture h264 av1 opus offscreen readback
// ge:skill shooter replay render
// ge:test tests/shooters_edge_demo_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "gpu/device.hpp"
#include "lab/raster.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/files.hpp"

namespace shooters_edge::client {

inline constexpr double kMostDemoGap = 0.25;  // the most one demo frame moves the video's clock, s

struct DemoVideoOptions {
  std::string demo;       // as `find_demo` takes it: `demo1`, a pack's file, a path
  std::string file;       // the video: .mp4, .webm or .mkv ("" in memory: `memory`)
  std::string container;  // "mp4" or "mkv"; "" by the file's name (Matroska in memory)
  std::vector<uint8_t>* memory = nullptr;  // with no file: the video's bytes, when not null
  int width = 1280, height = 720;
  double fps = 30;
  std::string codec = "auto";  // capture/video.hpp: auto, h264, hevc, av1; "" uncompressed (.mkv)
  int crf = 30;                // av1's quality
  double seconds = 0;          // the most video, s (0: the whole demo)
  int track = -1;              // a multiview recording's player (-1: the lowest playing)
  bool hud = true;
  bool sound = true;
  float volume = 0.5f;                     // the game's sounds, under the voices
  float voice_volume = 1.5f;               // the players' voices, over the game as voice chat is
  const look::Picture* picture = nullptr;  // the look (null: the classic look's defaults)
};

struct DemoVideoReport {
  std::string found;   // where the demo was read from
  std::string format;  // single, client, multiview; "+ voice" for a voice format
  int64_t frames = 0;
  double seconds = 0;
  int levels = 0;
  int64_t demo_frames = 0;
  int64_t sounds = 0, missing_sounds = 0, loops = 0;
  int64_t voice_messages = 0, voice_frames = 0, voice_concealed = 0;
  uint32_t talkers = 0;  // a bit a slot heard
  int64_t bytes = 0;
  std::string container;  // mp4, webm, matroska
  std::string codec;
  std::string adapter;
};

// Called with each picture as it goes into the file (a lab's strip): its index and its time.
using DemoVideoPicture = std::function<void(int64_t frame, double t, const ge::lab::Picture&)>;

// `o.demo` from `files` into `o.file` on `device`. False with the reason and the next step.
bool demo_video(ge::gpu::Device& device, const SearchPath& files, const DemoVideoOptions& o,
                DemoVideoReport& report, std::string& error,
                const DemoVideoPicture& picture = nullptr);

}  // namespace shooters_edge::client
