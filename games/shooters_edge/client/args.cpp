// The window's command line (args.hpp).
//
// ge:summary The shooters_edge window's command line: its flags read anywhere, unknown ones refused
// ge:keywords command-line args parse flags
// ge:skill shooter
// ge:test tests/shooters_edge_args_test.cpp
#include "shooters_edge/client/args.hpp"

#include "shooters_edge/client/demo_playback.hpp"

#include <cctype>
#include <cstdlib>

namespace shooters_edge::client {

// `~` or `~/...` as the home folder (a shell leaves a quoted one as it is).
std::string expand_home(const std::string& p) {
  if (p.empty() || p[0] != '~' || (p.size() > 1 && p[1] != '/')) return p;
#ifdef _WIN32
  const char* home = std::getenv("USERPROFILE");  // NOLINT(concurrency-mt-unsafe): at start
#else
  const char* home = std::getenv("HOME");  // NOLINT(concurrency-mt-unsafe): at start
#endif
  return home == nullptr ? p : std::string(home) + p.substr(1);
}

// A size as `WxH` (`1920x1080`) or a name: 480p, 720p, 1080p, 1440p, 4k (2160p). False for
// anything else, or a side under 16 or over 8192.
bool size_named(std::string_view v, int& w, int& h) {
  static constexpr struct {
    const char* name;
    int w, h;
  } kNamed[] = {{"480p", 854, 480},    {"720p", 1280, 720},   {"1080p", 1920, 1080},
                {"1440p", 2560, 1440}, {"2160p", 3840, 2160}, {"4k", 3840, 2160}};
  for (const auto& n : kNamed) {
    if (v == n.name) {
      w = n.w;
      h = n.h;
      return true;
    }
  }
  const size_t x = v.find('x');
  if (x == std::string_view::npos) return false;
  const int pw = std::atoi(std::string(v.substr(0, x)).c_str());
  const int ph = std::atoi(std::string(v.substr(x + 1)).c_str());
  if (pw < 16 || ph < 16 || pw > 8192 || ph > 8192) return false;
  w = pw;
  h = ph;
  return true;
}

// A word that reads as a flag: `-x` or `--x` with a letter after the dashes (a negative number,
// `-1` or `-0.5`, is a console line's word).
bool flag_like(std::string_view w) {
  const size_t dashes = w.rfind("--", 0) == 0 ? 2 : w.rfind('-', 0) == 0 ? 1 : 0;
  return dashes > 0 && w.size() > dashes &&
         std::isalpha(static_cast<unsigned char>(w[dashes])) != 0;
}

// False with `why` (the word refused and the next step) on a flag it does not know, wherever it
// stands: a flag inside a `+` console line is never taken as one of the line's words.
bool parse(int argc, char** argv, Args& a, std::string& why) {
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    if (arg == "-benchmark" || arg == "--benchmark") {
      why = std::string(arg) + " was renamed to -timedemo; next: rerun with -timedemo <demo>.";
      return false;
    }
    // The flags are read wherever they stand, a console line's words (`+r_look modern`) running
    // from a `+` to the next flag or `+`, so `+r_look modern -timedemo demo1` is a look and a
    // benchmark, not one console line of four words.
    if ((arg == "-basedir" || arg == "--basedir") && i + 1 < argc) {
      a.basedir = expand_home(argv[++i]);
    } else if ((arg == "-progs" || arg == "--progs") && i + 1 < argc) {
      a.progs = expand_home(argv[++i]);
    } else if ((arg == "-models" || arg == "--models") && i + 1 < argc) {
      a.models = expand_home(argv[++i]);
    } else if ((arg == "-timedemo" || arg == "--timedemo") && i + 1 < argc) {
      a.timedemo = expand_home(argv[++i]);
    } else if ((arg == "-demovideo" || arg == "--demovideo") && i + 2 < argc) {
      a.demovideo = expand_home(argv[++i]);
      a.video = expand_home(argv[++i]);
    } else if (arg.rfind("--fps=", 0) == 0) {
      a.fps = std::strtod(argv[i] + 6, nullptr);
    } else if (arg.rfind("--seconds=", 0) == 0) {
      a.seconds = std::strtod(argv[i] + 10, nullptr);
    } else if (arg.rfind("--from=", 0) == 0 || arg.rfind("--to=", 0) == 0) {
      const double t = parse_clock(std::string(arg.substr(arg.find('=') + 1)));
      if (t < 0) {
        why = "the time in " + std::string(arg) +
              " is not m:ss, h:mm:ss or seconds; next: rerun with one of those.";
        return false;
      }
      (arg[2] == 'f' ? a.from : a.to) = t;
    } else if (arg.rfind("--highlight=", 0) == 0) {
      a.highlight = static_cast<int>(std::strtol(argv[i] + 12, nullptr, 10));
    } else if (arg.rfind("--gif-width=", 0) == 0) {
      a.gif_width = static_cast<int>(std::strtol(argv[i] + 12, nullptr, 10));
    } else if (arg.rfind("--codec=", 0) == 0) {
      a.codec = std::string(arg.substr(8));
    } else if ((arg == "-playdemo" || arg == "--playdemo") && i + 1 < argc) {
      a.playdemo = expand_home(argv[++i]);
    } else if ((arg == "-game" || arg == "--game") && i + 1 < argc) {
      a.game = argv[++i];
    } else if (arg.rfind("--frames=", 0) == 0) {
      a.frames = std::strtol(argv[i] + 9, nullptr, 10);
    } else if (arg.rfind("--size=", 0) == 0 || arg.rfind("--resolution=", 0) == 0) {
      if (!size_named(arg.substr(arg.find('=') + 1), a.width, a.height)) {
        why = "the size in " + std::string(arg) +
              " is not WxH (16 to 8192 a side) or 480p, 720p, 1080p, 1440p, 2160p, 4k; next: "
              "rerun with one of those.";
        return false;
      }
    } else if (flag_like(arg)) {
      why = "unknown flag " + std::string(arg) +
            (a.plus.empty() ? std::string()
                            : " (after the console line starting " + a.plus.front() + ")") +
            "; next: rerun with the usage's flags only.";
      return false;
    } else if (!a.plus.empty() || (!arg.empty() && arg[0] == '+')) {
      a.plus.emplace_back(arg);
    } else {
      why = "unknown word " + std::string(arg) +
            "; next: put it after a +command, or rerun with the usage's flags only.";
      return false;
    }
  }
  return true;
}

}  // namespace shooters_edge::client
