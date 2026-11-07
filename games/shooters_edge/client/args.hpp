// THE WINDOW'S COMMAND LINE: the flags `shooters_edge` takes, read into `Args`. The flags are read
// wherever they stand; a `+` word starts a console line that runs to the next flag or `+`, so
// `+r_look modern -timedemo demo1` is a look and a benchmark, and the first line needs no `+`
// (`coopgame 2 expert`). A word that reads as a flag (`-x`,
// `--x`: a dash and a letter; `-1` is a word) and is not one of the flags is refused, with the
// console line it followed named, rather than taken as one of that line's words; `-benchmark`
// names its new name, `-timedemo`.
//
// ge:summary The shooters_edge window's command line: its flags read anywhere, unknown ones refused
// ge:keywords command-line args parse flags plus-commands console-line unknown-flag expand_home
// ge:keywords size_named flag_like timedemo benchmark-renamed
// ge:skill shooter
// ge:test tests/shooters_edge_args_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "classicenhance/profile.hpp"

namespace shooters_edge::client {

struct Args {
  std::string basedir = ".";
  std::string game{ge::classicenhance::classic_profile().base_dir};  // the base game's folder
  bool basedir_set = false, game_set = false;  // given on the command line (not the defaults)
  std::string progs;                           // a mod's progs file ("": the baseline mod)
  std::string models;                          // the held weapons' folder ("": the tree's)
  std::string timedemo;                        // a demo to benchmark, then quit ("": none)
  std::string demovideo, video;  // a demo to turn into the video file `video`, then quit
  double fps = 30, seconds = 0;
  double from = 0, to = -1;  // -demovideo's clip on the demo's timeline, s (to < 0: the end)
  int highlight = 0;         // -demovideo's clip: the tracked player's Nth highlight
  bool autotrack = false;    // -demovideo of a multiview recording: follow the hottest player
  // The bot client (`--bots=N --server=host:port`): a team of bots joins the server and the window
  // shows the team's world (net/classic_team_view.hpp), the console driving the bots.
  int bots = 0;
  std::string server = "127.0.0.1:27500", team = "bots", skills, bots_out, bots_view;
  int colour = 4;
  bool bots_paths = true;  // `--bots-paths=0`: the bots' overview draws no paths
  int gif_width = 480;     // -demovideo to a .gif: its width in pixels
  std::string codec = "auto";
  std::string playdemo;  // a demo to play at its own pace ("": none)
  long frames = 0;       // 0: until quit
  int width = 1280, height = 720;
  // Whether --fps, --codec and --size (--resolution) were given: -demovideo takes video.cfg's
  // demovideo_fps, demovideo_codec and demovideo_size for those that were not (video_cfg.hpp).
  bool fps_given = false, codec_given = false, size_given = false;
  std::vector<std::string> plus;  // the command line from the first `+` word on
};

// `~` or `~/...` as the home folder (a shell leaves a quoted one as it is).
std::string expand_home(const std::string& p);
// A size as `WxH` (`1920x1080`) or a name: 480p, 720p, 1080p, 1440p, 4k (2160p). False for
// anything else, or a side under 16 or over 8192.
bool size_named(std::string_view v, int& w, int& h);
// A word that reads as a flag: `-x` or `--x` with a letter after the dashes.
bool flag_like(std::string_view w);
// The command line into `a`. False with `why` (the word refused and the next step) on a flag it
// does not know, wherever it stands, or a bare word before any `+`.
bool parse(int argc, char** argv, Args& a, std::string& why);

}  // namespace shooters_edge::client
