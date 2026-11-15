// VIDEO.CFG: THE SETTINGS A DEMO VIDEO IS MADE WITH, for any game that exports its demos as video.
// The export reads its own size, rate, codec and words from cvars a `video.cfg` sets, so a player
// keeps one file for how their videos are made, apart from how they play.
//
// THE ORDER, after the game's own startup (its cfg files and the command line's `+` words):
// 1. the built-in defaults (`video_defaults`): every cvar of the game's look (`VideoLook`: the
//    names it lists and every cvar under its prefix) reset, so a video shows the look as the game
//    ships it, whatever the player's config chose for play;
// 2. `video.cfg`, the player's, from the first of the folders given that has one;
// 3. the command line's `+` words again, so one video can still set a cvar its own way;
// 4. the export's own flags over the cvars below (the game's part).
//
// THE CVARS (`add_video_cvars`, not archived: they belong in video.cfg, not the player's config):
// `demovideo_size` (`1080p`, or `WxH`, or 480p, 720p, 1440p, 4k), `demovideo_fps` (60),
// `demovideo_codec` (auto, h264, hevc, av1, none), `demovideo_text` (1: the words in the upper
// left), `demovideo_messages` (1: the chat, the notify lines and the centre prints) and
// `demovideo_hud` (1: the status bar).
//
// ge:summary video.cfg for any game: a demo video's size, rate, codec, words, the look reset
// ge:keywords video-cfg video_cfg demovideo demovideo_size demovideo_fps demovideo_codec
// ge:keywords demovideo_text demovideo_hud export settings video-look reset
// ge:skill replay console
// ge:test tests/shooters_edge_demo_caption_test.cpp
#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"

namespace ge::capture {

inline constexpr std::string_view kVideoCfgFile = "video.cfg";

struct VideoSettings {
  std::string size = "1080p";  // demovideo_size
  double fps = 60;             // demovideo_fps
  std::string codec = "auto";  // demovideo_codec
  bool text = true;            // demovideo_text
  bool messages = true;        // demovideo_messages
  bool hud = true;             // demovideo_hud
};

// The cvars of a game's look that a video's defaults put back: these names (when the game has
// them), then every cvar whose name starts with `prefix` (none when empty).
struct VideoLook {
  std::span<const std::string_view> cvars;
  std::string_view prefix;
};

// Registers the demovideo_ cvars over `*out` (it must outlive `con`).
void add_video_cvars(console::Console& con, VideoSettings* out);

// The built-in defaults as cfg text: a `reset` for every cvar of `look` that `cvars` has.
std::string video_defaults(const console::Cvars& cvars, const VideoLook& look);

// Steps 1 to 3 above on `con`, video.cfg looked for in `dirs` in order; the path read, or "" when
// no folder had one.
std::filesystem::path apply_video_cfg(console::Console& con, const VideoLook& look,
                                      const std::vector<std::filesystem::path>& dirs,
                                      const std::vector<std::string>& plus);

}  // namespace ge::capture
