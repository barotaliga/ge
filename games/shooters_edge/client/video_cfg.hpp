// VIDEO.CFG IN THIS GAME: the engine's (`capture/video_cfg.hpp`: the demovideo_ cvars, the
// defaults and the order video.cfg is read in) with this game's look as the defaults put it back:
// `r_look modern`, the look's other `r_` cvars and every classic-enhance switch (`ce_<id>`), the
// newer look whatever config.cfg chose for play. video.cfg is looked for in the game folder
// (the install's `-game` folder), then the current folder; `-demovideo` (demo_video.hpp) applies
// it, and its flags (`--resolution`, `--fps`, `--codec`) win over the cvars.
//
// ge:summary video.cfg in this game: the engine's video settings with the newer look as defaults
// ge:keywords video-cfg video_cfg demovideo newer-look modern ce-defaults video-look
// ge:skill shooter replay console
// ge:test tests/shooters_edge_demo_caption_test.cpp
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "capture/video_cfg.hpp"
#include "console/console.hpp"

namespace shooters_edge::client {

using ge::capture::add_video_cvars;
using ge::capture::kVideoCfgFile;
using ge::capture::VideoSettings;

// The look's cvars besides the ce_ switches that video.cfg's defaults put back (app/look.hpp).
inline constexpr std::string_view kVideoLookCvars[] = {"r_look",  "r_fx_lights", "r_fx_glow",
                                                       "r_bloom", "r_lava_glow", "r_slime_glow"};
inline constexpr ge::capture::VideoLook kVideoLook{kVideoLookCvars, "ce_"};

// The built-in defaults as cfg text for this game's look.
inline std::string video_defaults(const ge::console::Cvars& cvars) {
  return ge::capture::video_defaults(cvars, kVideoLook);
}

// The engine's order on `con` with this game's look; the path read, or "" when none.
inline std::filesystem::path apply_video_cfg(ge::console::Console& con,
                                             const std::vector<std::filesystem::path>& dirs,
                                             const std::vector<std::string>& plus) {
  return ge::capture::apply_video_cfg(con, kVideoLook, dirs, plus);
}

}  // namespace shooters_edge::client
