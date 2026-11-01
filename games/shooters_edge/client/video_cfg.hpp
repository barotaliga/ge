// VIDEO.CFG: THE SETTINGS A DEMO VIDEO IS MADE WITH. `-demovideo` (demo_video.hpp) draws with the
// newer graphics whatever the player plays with, and reads its own size, rate, codec and words
// from cvars a `video.cfg` sets, so a player who plays in the classic look still exports in the
// modern one, and keeps one file for how their videos are made.
//
// THE ORDER, after the game's own startup (default.cfg, config.cfg, autoexec.cfg, the `+` words):
// 1. the built-in defaults (`video_defaults`): `r_look modern`, the look's other `r_` cvars and
//    every classic-enhance switch (`ce_<id>`) reset to their defaults, the newer look as the
//    engine ships it, whatever config.cfg chose for play;
// 2. `video.cfg`, the player's, from the game folder (`-basedir`'s `-game` folder) or else the
//    current folder, when one is there;
// 3. the command line's `+` words again, so `+r_look classic` still wins for one video;
// 4. the command line's flags (`--resolution`, `--fps`, `--codec`) over the cvars below.
//
// THE CVARS (`add_video_cvars`, not archived: they belong in video.cfg, not config.cfg):
// `demovideo_size` (`1080p`, or `WxH`, or 480p, 720p, 1440p, 4k), `demovideo_fps` (60),
// `demovideo_codec` (auto, h264, hevc, av1, none), `demovideo_text` (1: the words in the upper
// left, demo_caption.hpp) and `demovideo_hud` (1: the status bar).
//
// ge:summary video.cfg: the newer look and the size, rate, codec and words a demo video uses
// ge:keywords video-cfg video_cfg demovideo demovideo_size demovideo_fps demovideo_codec
// ge:keywords demovideo_text demovideo_hud export settings newer-look modern ce-defaults reset
// ge:skill shooter replay console
// ge:test tests/shooters_edge_demo_caption_test.cpp
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"

namespace shooters_edge::client {

inline constexpr std::string_view kVideoCfgFile = "video.cfg";

// The look's cvars besides the ce_ switches that video.cfg's defaults put back (app/look.hpp).
inline constexpr std::string_view kVideoLookCvars[] = {"r_look",  "r_fx_lights", "r_fx_glow",
                                                       "r_bloom", "r_lava_glow", "r_slime_glow"};

struct VideoSettings {
  std::string size = "1080p";  // demovideo_size
  double fps = 60;             // demovideo_fps
  std::string codec = "auto";  // demovideo_codec
  bool text = true;            // demovideo_text
  bool hud = true;             // demovideo_hud
};

// Registers the demovideo_ cvars over `*out` (it must outlive `con`).
void add_video_cvars(ge::console::Console& con, VideoSettings* out);

// The built-in defaults as cfg text: a `reset` for every look cvar `cvars` has (kVideoLookCvars,
// then every `ce_` cvar by name).
std::string video_defaults(const ge::console::Cvars& cvars);

// Steps 1 to 3 above on `con`, video.cfg looked for in `dirs` in order; the path read, or "" when
// no folder had one.
std::filesystem::path apply_video_cfg(ge::console::Console& con,
                                      const std::vector<std::filesystem::path>& dirs,
                                      const std::vector<std::string>& plus);

}  // namespace shooters_edge::client
