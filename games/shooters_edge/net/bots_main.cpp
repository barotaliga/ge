// THE BOT TEAM, HEADLESS: `shooters_edge_bots --server=<host:port> --bots=4 --team=<name>
// --basedir=<the player's packs>` joins a classic server with a team of bots
// (classic_bot_team.hpp) and plays until stopped: a tick every 13 ms on the wall clock, every line
// typed on stdin a console line for the bots (`bot2 skill novice`, `ready`, `status`, `quit`),
// what the server prints and the bots say written to stdout, and the team's report every
// `--report` seconds. With `--out=<folder>` the match folder is written when it stops (each bot's
// client recording, the team's lines, the log). `--seconds` stops it after that long; `--until-end`
// when the match mod says the match is over. `--view=<file.mp4>` records the team's picture
// (classic_team_view.hpp: the map, the bots, their routes, goals, beliefs and plans) as a video at
// `--view-fps`; `--snapshot=<file.png>` rewrites one picture of it every `--snapshot-every` s.
//
// ge:summary The bot team headless: N bots join a classic server, a stdin console, a match folder
// ge:keywords shooters_edge_bots bots-cli headless-bots live-server stdin-console match-folder
// ge:skill bots net
#include <poll.h>
#include <unistd.h>

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>

#include "lab/png.hpp"
#include "shooters_edge/net/classic_bot_team.hpp"
#include "shooters_edge/net/classic_team_view.hpp"

namespace sn = shooters_edge::net;

namespace {

volatile std::sig_atomic_t g_stop = 0;
void on_signal(int) { g_stop = 1; }

bool flag(const std::string& a, const char* name, std::string& out) {
  const std::string p = std::string("--") + name + "=";
  if (a.rfind(p, 0) != 0) return false;
  out = a.substr(p.size());
  return true;
}

std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at <= s.size()) {
    const size_t end = std::min(s.find(',', at), s.size());
    if (end > at) out.push_back(s.substr(at, end - at));
    at = end + 1;
  }
  return out;
}

void usage() {
  std::printf(
      "shooters_edge_bots --server=<host:port> [--bots=4] [--team=bots] [--colour=4]\n"
      "  [--basedir=<folder holding id1>] [--skills=expert,average,...] [--traits=<a,b,...>]\n"
      "  [--name=gebot] [--out=<match folder>] [--seconds=<n>] [--until-end] [--report=10]\n"
      "  [--no-ready] [--seed=1] [--qport=2100] [--view=<file.mp4>] [--view-fps=10] "
      "[--view-size=1280x720]\n"
      "  [--snapshot=<file.png>] [--snapshot-every=5] [--locs=<folder with locs/>]\n"
      "Console (stdin): a line goes to every bot; 'bot<N> <line>' to one. Bot settings: skill,\n"
      "temper, traits, <trait> <1..10>; 'status'; 'autoready 0|1'; 'quit'. Anything else is sent\n"
      "to the server as the bot's console line (ready, break, say ...).\n");
}

}  // namespace

int run(int argc, char** argv) {
  sn::BotTeamConfig cfg;
  double seconds = 0, report_s = 10, view_fps = 10, snapshot_every = 5;
  bool until_end = false;
  std::string view_file, snapshot_file, locs_folder = "ezquake";
  int view_w = 1280, view_h = 720;
  for (int i = 1; i < argc; i++) {
    const std::string a = argv[i];
    std::string v;
    if (a == "--help" || a == "-h") {
      usage();
      return 0;
    } else if (flag(a, "server", v)) {
      cfg.server = v;
    } else if (flag(a, "bots", v)) {
      cfg.bots = std::atoi(v.c_str());
    } else if (flag(a, "team", v)) {
      cfg.team = v;
    } else if (flag(a, "colour", v) || flag(a, "color", v)) {
      cfg.colour = std::atoi(v.c_str());
    } else if (flag(a, "basedir", v)) {
      cfg.basedir = v;
    } else if (flag(a, "skills", v)) {
      cfg.skills = split(v);
    } else if (flag(a, "traits", v)) {
      cfg.traits = split(v);
    } else if (flag(a, "name", v)) {
      cfg.name_prefix = v;
    } else if (flag(a, "out", v)) {
      cfg.out = v;
    } else if (flag(a, "seconds", v)) {
      seconds = std::atof(v.c_str());
    } else if (flag(a, "report", v)) {
      report_s = std::atof(v.c_str());
    } else if (flag(a, "locs", v)) {
      locs_folder = v;
    } else if (flag(a, "qport", v)) {
      cfg.first_qport = static_cast<uint16_t>(std::atoi(v.c_str()));
    } else if (flag(a, "seed", v)) {
      cfg.seed = static_cast<uint32_t>(std::atoi(v.c_str()));
    } else if (flag(a, "view", v)) {
      view_file = v;
    } else if (flag(a, "view-fps", v)) {
      view_fps = std::atof(v.c_str());
    } else if (flag(a, "view-size", v)) {
      if (std::sscanf(v.c_str(), "%dx%d", &view_w, &view_h) != 2) {
        std::fprintf(stderr, "shooters_edge_bots: --view-size=%s is not WxH\n", v.c_str());
        return 2;
      }
    } else if (flag(a, "snapshot", v)) {
      snapshot_file = v;
    } else if (flag(a, "snapshot-every", v)) {
      snapshot_every = std::atof(v.c_str());
    } else if (a == "--until-end") {
      until_end = true;
    } else if (a == "--no-ready") {
      cfg.ready = false;
    } else {
      std::fprintf(stderr, "shooters_edge_bots: unknown argument '%s'; next: --help\n", a.c_str());
      return 2;
    }
  }
  if (!cfg.out.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(cfg.out, ec);
  }
  // The maps' place names: `locs/<map>.loc` in a game folder of the player's (a client's own).
  sn::bot_team_locs_from(cfg, locs_folder);
  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);
  sn::BotTeam team;
  std::string error;
  if (!sn::bot_team_start(team, cfg, error)) {
    std::fprintf(stderr, "%s\n", error.c_str());
    return 1;
  }
  sn::TeamView view;
  sn::TeamViewVideo video;
  if (!view_file.empty() &&
      !sn::team_view_video_open(video, view_file, view_w & ~1, view_h & ~1, view_fps, error)) {
    std::fprintf(stderr, "shooters_edge_bots: --view=%s: %s\n", view_file.c_str(), error.c_str());
    return 1;
  }
  double next_view = 0, next_snapshot = 0;
  const auto start = std::chrono::steady_clock::now();
  int64_t next_tick = 0;
  double next_report = report_s;
  std::string pending;  // stdin not yet a whole line
  bool ended = false;
  while (!g_stop) {
    const int64_t now = std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::steady_clock::now() - start)
                            .count();
    if (now < next_tick) {
      std::this_thread::sleep_for(std::chrono::microseconds(next_tick - now));
      continue;
    }
    next_tick += 13000;
    if (now - next_tick > 200000) next_tick = now;  // a stall: never a flood of ticks after it
    sn::bot_team_tick(team, now);
    // The console.
    pollfd p{0, POLLIN, 0};
    while (poll(&p, 1, 0) > 0 && (p.revents & POLLIN)) {
      char buf[512];
      const ssize_t n = read(0, buf, sizeof buf);
      if (n <= 0) break;
      pending.append(buf, static_cast<size_t>(n));
      size_t nl;
      while ((nl = pending.find('\n')) != std::string::npos) {
        const std::string line = pending.substr(0, nl);
        pending.erase(0, nl + 1);
        if (line == "quit" || line == "/quit")
          g_stop = 1;
        else
          sn::bot_team_command(team, line);
      }
    }
    for (; team.log_read < team.log.size(); team.log_read++) {
      const std::string& l = team.log[team.log_read];
      std::printf("%s\n", l.c_str());
      if (until_end &&
          (l.find("match over") != std::string::npos || l.find("Match over") != std::string::npos ||
           l.find("The match is over") != std::string::npos))
        ended = true;
    }
    std::fflush(stdout);
    const double t = static_cast<double>(now) / 1e6;
    if (report_s > 0 && t >= next_report) {
      next_report += report_s;
      std::printf("%s", sn::bot_team_report(team).c_str());
    }
    if (video.open && t >= next_view) {
      next_view += 1.0 / view_fps;
      if (!sn::team_view_video_frame(video, sn::team_view(view, team, view_w & ~1, view_h & ~1),
                                     error))
        std::fprintf(stderr, "shooters_edge_bots: the view's video: %s\n", error.c_str());
    }
    if (!snapshot_file.empty() && t >= next_snapshot) {
      next_snapshot += snapshot_every;
      ge::lab::write_png(snapshot_file, sn::team_view(view, team, view_w & ~1, view_h & ~1));
    }
    if (ended) {
      std::printf("bots: the match is over; stopping in 5 s\n");
      until_end = false;
      ended = false;
      seconds = t + 5;
    }
    if (seconds > 0 && t >= seconds) break;
  }
  if (!sn::team_view_video_close(video, error))
    std::fprintf(stderr, "shooters_edge_bots: the view's video: %s\n", error.c_str());
  else if (!view_file.empty())
    std::printf("bots: the team's view is %s (%lld pictures)\n", view_file.c_str(),
                static_cast<long long>(video.frames));
  if (!sn::bot_team_stop(team, error)) {
    std::fprintf(stderr, "%s\n", error.c_str());
    return 1;
  }
  std::printf("%s", sn::bot_team_report(team).c_str());
  if (!cfg.out.empty()) std::printf("bots: the match folder is %s\n", cfg.out.c_str());
  return 0;
}

int main(int argc, char** argv) {
  try {
    return run(argc, argv);
  } catch (const std::exception& e) {
    std::fprintf(stderr,
                 "shooters_edge_bots: stopped by an error: %s; next: run it again, and report it "
                 "with the command line\n",
                 e.what());
    return 1;
  }
}
