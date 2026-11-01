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
// A CLIP is a part of the demo: from `from_s` on the play bar's timeline (timedemo.hpp's
// `DemoTimeline`, the same clock as this one) for `seconds`, or one of the tracked player's
// highlights (demo_highlights.hpp) with `highlight` = N, its window from the moment's lead-in to
// its hold. The frames before the clip are read and not drawn, and their sounds are not played.
// A file named `.gif` is an animated GIF (engine/lab/gif.hpp: shrunk to `gif_width`, one palette
// of 256 colours, no sound), which needs no encoder; every other name is a video as above.
//
// The run's report (`DemoVideoReport`) says what went in: pictures, seconds, levels, the sounds
// played and the names no pack had, the voices heard, the file's bytes and its codec.
//
// ge:summary A demo turned into a video: pictures on the GPU, its sounds and voices, one file
// ge:keywords demo-video demovideo export video mp4 webm mkv record demo-to-video frame-clock
// ge:keywords sound voice qwdv mvdv capture h264 av1 opus offscreen readback
// ge:keywords clip gif animated-gif highlight from-to start-end excerpt
// ge:keywords demo_placed demo-players other-players player-colours
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
#include "shooters_edge/client/args.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/world_models.hpp"

namespace shooters_edge::client {

inline constexpr double kMostDemoGap = 0.25;  // the most one demo frame moves the video's clock, s

struct DemoVideoOptions {
  std::string demo;       // as `find_demo` takes it: `demo1`, a pack's file, a path
  std::string file;       // the video: .mp4, .webm, .mkv or .gif ("" in memory: `memory`)
  std::string container;  // "mp4" or "mkv"; "" by the file's name (Matroska in memory)
  std::vector<uint8_t>* memory = nullptr;  // with no file: the video's bytes, when not null
  int width = 1280, height = 720;
  double fps = 30;
  std::string codec = "auto";  // capture/video.hpp: auto, h264, hevc, av1; "" uncompressed (.mkv)
  int crf = 30;                // av1's quality
  double seconds = 0;          // the most video, s (0: the whole demo)
  double from_s = 0;           // the clip's start on the demo's timeline, s
  int highlight =
      0;  // 1..: the tracked player's Nth highlight is the clip (overrides the two above)
  bool gif = false;     // an animated GIF (also when `file` ends in .gif)
  int gif_width = 480;  // a GIF's width, px (the height in proportion)
  int track = -1;       // a multiview recording's player (-1: the lowest playing)
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
  double from_s = 0, to_s = 0;  // the clip's window on the demo's timeline, s
  std::string highlight;        // the highlight that made it ("": none)
};

// Called with each picture as it goes into the file (a lab's strip): its index and its time on
// the demo's timeline.
using DemoVideoPicture = std::function<void(int64_t frame, double t, const ge::lab::Picture&)>;

// `o.demo` from `files` into `o.file` on `device`. False with the reason and the next step.
// What a demo has standing in the world this frame, for `FxFrame::models`: its entities with a
// model (not the eye's own), the other players in their colours, the level's statics. The video
// and the window's playback (`playdemo`, `timedemo`) both draw these.
void demo_placed(const DemoState& s, std::vector<Placed>& out);

bool demo_video(ge::gpu::Device& device, const SearchPath& files, const DemoVideoOptions& o,
                DemoVideoReport& report, std::string& error,
                const DemoVideoPicture& picture = nullptr);

// The report as `-demovideo` prints it: where from and to, the clip, the pictures, the file, the
// sounds, a line each.
std::vector<std::string> demo_video_lines(const DemoVideoOptions& o, const DemoVideoReport& r);

// `-demovideo <demo> <file>` on the window's command line: `args`' demo turned into a video with
// no window on a device of its own, the view riding `track`, the report printed. The exit code: 0
// done, 1 no GPU device, 2 the export failed.
int demo_video_main(const Args& args, const SearchPath& files,
                    const shooters_edge::look::Picture& picture, int track);

}  // namespace shooters_edge::client
