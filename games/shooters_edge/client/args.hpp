// THE WINDOW'S COMMAND LINE: the flags `shooters_edge` takes, read into `Args`. The flags are read
// wherever they stand; a `+` word starts a console line that runs to the next flag or `+`, so
// `+r_look modern -timedemo demo1` is a look and a benchmark. A word that reads as a flag (`-x`,
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
  std::string progs;             // a mod's progs file ("": the baseline mod)
  std::string models;            // the held weapons' folder ("": the tree's)
  std::string timedemo;          // a demo to benchmark, then quit ("": none)
  std::string demovideo, video;  // a demo to turn into the video file `video`, then quit
  double fps = 30, seconds = 0;
  double from = 0, to = -1;  // -demovideo's clip on the demo's timeline, s (to < 0: the end)
  int highlight = 0;         // -demovideo's clip: the tracked player's Nth highlight
  int gif_width = 480;       // -demovideo to a .gif: its width in pixels
  std::string codec = "auto";
  std::string playdemo;  // a demo to play at its own pace ("": none)
  long frames = 0;       // 0: until quit
  int width = 1280, height = 720;
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
