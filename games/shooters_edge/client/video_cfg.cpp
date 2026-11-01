// video.cfg: the settings a demo video is made with (video_cfg.hpp).
//
// ge:summary video.cfg's cvars, its built-in defaults and the order it is read in
// ge:keywords video-cfg demovideo_size demovideo_fps demovideo_codec demovideo_text reset
// ge:skill shooter replay console
// ge:test tests/shooters_edge_demo_caption_test.cpp
#include "shooters_edge/client/video_cfg.hpp"

#include <system_error>
#include <utility>

namespace shooters_edge::client {

namespace con = ge::console;

void add_video_cvars(con::Console& c, VideoSettings* out) {
  con::Cvars& cv = c.cvars();
  cv.bind({.name = "demovideo_size",
           .fallback = "1080p",
           .help = "-demovideo's picture: WxH or 480p, 720p, 1080p, 1440p, 4k (--resolution wins)",
           .kind = con::CvarKind::text},
          &out->size);
  cv.bind({.name = "demovideo_fps",
           .fallback = "60",
           .help = "-demovideo's pictures a second (--fps wins)",
           .min = 1,
           .max = 240},
          &out->fps);
  cv.bind({.name = "demovideo_codec",
           .fallback = "auto",
           .help = "-demovideo's codec: auto, h264, hevc, av1, none (--codec wins)",
           .kind = con::CvarKind::text},
          &out->codec);
  cv.bind({.name = "demovideo_text",
           .fallback = "1",
           .help = "1: -demovideo writes the map, the clock, whose eyes and the score in the upper "
                   "left, in the packs' console font when they have one",
           .kind = con::CvarKind::boolean},
          &out->text);
  cv.bind({.name = "demovideo_messages",
           .fallback = "1",
           .help = "1: -demovideo shows what the recorded player read: the chat (say, say_team), "
                   "the pickups and deaths under the words, the centre prints in the middle",
           .kind = con::CvarKind::boolean},
          &out->messages);
  cv.bind({.name = "demovideo_hud",
           .fallback = "1",
           .help = "1: -demovideo draws the status bar off the demo's stats",
           .kind = con::CvarKind::boolean},
          &out->hud);
}

std::string video_defaults(const con::Cvars& cvars) {
  std::string out = "// video.cfg's built-in defaults: the newer look, as the engine ships it\n";
  for (std::string_view name : kVideoLookCvars)
    if (cvars.find(name) != nullptr) out += "reset " + std::string(name) + "\n";
  for (const con::Cvar* v : cvars.sorted())
    if (v->name.rfind("ce_", 0) == 0) out += "reset " + v->name + "\n";
  return out;
}

std::filesystem::path apply_video_cfg(con::Console& c,
                                      const std::vector<std::filesystem::path>& dirs,
                                      const std::vector<std::string>& plus) {
  c.execute(video_defaults(c.cvars()));
  std::filesystem::path read;
  for (const std::filesystem::path& d : dirs) {
    std::error_code ec;
    if (d.empty() || !std::filesystem::is_regular_file(d / kVideoCfgFile, ec)) continue;
    // Read through the console, so a mistake in it is named by file and line like any cfg's.
    std::vector<std::filesystem::path> search = c.search_path();
    c.set_search_path({d});
    c.exec_file(kVideoCfgFile);
    c.run();
    c.set_search_path(std::move(search));
    read = d / kVideoCfgFile;
    break;
  }
  c.append_command_line(plus);
  c.run();
  c.cvars().apply_latched();
  return read;
}

}  // namespace shooters_edge::client
