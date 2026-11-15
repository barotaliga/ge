// LAB `se-load`: THE GAME'S OWN TRAFFIC AS THE LOAD. Bots play as clients on our server over the
// simulated network, every move through the classic protocol as a person's would
// (classic_team.hpp), on many servers: for each count of players (`--players=2,8,16`, split into
// two teams), `--servers` matches on their own seeds. Each server's frame is timed on the wall
// clock, and the bots' thinking (the clients' ticks) apart from it, since a real server carries
// only its own frame. The table's counts (the frames, the frags, the bytes on the wire, each
// match's digest) are deterministic; the times are measured: the server's frame mean, p99 and
// worst against the players, and how many such servers one core holds at the frame's 13 ms.
//
// The map comes from `--basedir`'s game folder; without one, the fixture halls, which is what
// `make labs-smoke` runs.
//
// ge:summary Lab: bots as clients on many simulated servers, each server's frame timed
// ge:keywords lab se-load load bots clients servers frame-time capacity players multiplayer
// ge:skill net bots labs
// ge:test tests/shooters_edge_team_test.cpp
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/net/classic_team.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace cl = shooters_edge::client;
namespace cn = shooters_edge::net;
using ge::core::num::to_fixed;

int64_t wall_ns() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

std::vector<int> counts(const std::string& text) {
  std::vector<int> out;
  size_t at = 0;
  while (at < text.size()) {
    const size_t end = std::min(text.find(',', at), text.size());
    out.push_back(std::atoi(text.substr(at, end - at).c_str()));
    at = end + 1;
  }
  return out;
}

std::string hex(uint64_t h) {
  static const char* k = "0123456789abcdef";
  std::string s(16, '0');
  for (int i = 15; i >= 0; i--, h >>= 4) s[static_cast<size_t>(i)] = k[h & 15];
  return s;
}

void run(gl::Lab& lab) {
  std::string error;
  const std::string basedir = lab.arg("basedir");
  const std::string name = basedir.empty() ? "halls" : lab.arg("map");
  std::vector<uint8_t> bytes;
  if (!basedir.empty()) {
    cl::SearchPath files;
    if (!cl::open_game_dir(basedir, lab.arg("game"), files, error)) {
      lab.fail(error, "Pass --basedir=<the folder that holds the game folder>. Skill: shooter.");
      return;
    }
    std::vector<uint8_t> store;
    const std::span<const uint8_t> f = cl::find_file(files, "maps/" + name + ".bsp", store);
    if (f.empty()) {
      lab.fail("se-load: no maps/" + name + ".bsp in the game folder",
               "next: put the map in <basedir>/" + lab.arg("game") +
                   "/maps or name another with --map; skill: labs.");
      return;
    }
    bytes.assign(f.begin(), f.end());
  } else if (!ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v29, &bytes,
                                   &error)) {
    lab.fail("se-load: the fixture halls: " + error, "Run tests/brush_write_test.cpp.");
    return;
  }
  ge::progs::Progs progs;
  if (!cl::baseline_progs(progs, error)) {
    lab.fail("se-load: the baseline mod: " + error,
             "Run tests/shooters_edge_baseline_mod_test.cpp.");
    return;
  }
  const std::vector<int> players = counts(lab.arg("players"));
  const int servers = static_cast<int>(lab.arg_num("servers"));
  const double minutes = lab.arg_num("minutes");
  bool ok = !players.empty() && servers >= 1 && servers <= 64 && minutes > 0;
  for (const int p : players) ok = ok && p >= 2 && p <= 16;
  if (!ok) {
    lab.fail("se-load: --players, --servers or --minutes is out of range",
             "Pass --players as counts 2 to 16 comma-separated, --servers=1 to 64 and --minutes "
             "above 0. Skill: net.");
    return;
  }
  const double frame_us = 1000.0 * shooters_edge::bots::kPilotMsec;
  gl::LabTable played{"Each server: its players, what they did and what crossed the wire (" +
                          to_fixed(minutes, 1) + " minutes on " + name + ")",
                      {"players", "seed", "frames", "frags", "deaths", "wire KB/s", "digest"},
                      {}};
  gl::LabTable times{"The server's frame against its players, every server of that size together",
                     {"players", "servers", "frame mean us", "frame p99 us", "worst us",
                      "bots a frame us", "servers a core holds"},
                     {}};
  gl::Trace mean_trace{"the server's frame, mean", {}, {}};
  gl::Trace p99_trace{"the server's frame, p99 (worst server)", {}, {}};
  for (const int count : players) {
    double mean_sum = 0, p99 = 0, worst = 0, bots = 0;
    for (int k = 0; k < servers; k++) {
      cn::TeamMatchOptions o;
      o.map = bytes;
      o.map_name = name;
      if (basedir.empty()) o.entities = cn::kTeamHallsEntities;
      o.progs = &progs;
      o.seed = static_cast<uint32_t>(k + 1);
      o.record = false;
      o.timelimit = 0;
      o.frames = static_cast<int>(minutes * 60 * 1000 / shooters_edge::bots::kPilotMsec);
      o.clock_ns = wall_ns;
      const sh::AimSkill cycle[] = {sh::AimSkill::expert, sh::AimSkill::average,
                                    sh::AimSkill::novice};
      for (int t = 0; t < 2; t++) {
        o.teams[t].skills.clear();
        const int side = t == 0 ? (count + 1) / 2 : count / 2;
        for (int i = 0; i < side; i++) o.teams[t].skills.push_back(cycle[i % 3]);
      }
      const cn::TeamMatchReport r = cn::run_team_match(o);
      if (!r.ok) {
        lab.fail("se-load: " + r.error, "Run tests/shooters_edge_team_test.cpp.");
        return;
      }
      double frags = 0, deaths = 0;
      for (const cn::TeamPlayer& p : r.players) {
        frags += p.frags;
        deaths += p.deaths;
      }
      const double secs = r.frames * frame_us / 1e6;
      played.rows.push_back({std::to_string(count), std::to_string(o.seed),
                             std::to_string(r.frames), to_fixed(frags, 0), to_fixed(deaths, 0),
                             to_fixed(static_cast<double>(r.net_bytes) / 1024.0 / secs, 1),
                             hex(r.digest)});
      mean_sum += r.frame_us_mean;
      p99 = std::max(p99, r.frame_us_p99);
      worst = std::max(worst, r.frame_us_max);
      bots += r.bots_us_mean;
    }
    const double mean = mean_sum / servers;
    const std::string key = std::to_string(count);
    times.rows.push_back({key, std::to_string(servers), to_fixed(mean, 1), to_fixed(p99, 1),
                          to_fixed(worst, 1), to_fixed(bots / servers, 1),
                          mean > 0 ? to_fixed(frame_us / mean, 0) : "-"});
    lab.metric(key + "-players.frame-us-mean", mean, "us", gl::Det::measured);
    lab.metric(key + "-players.frame-us-p99", p99, "us", gl::Det::measured);
    lab.metric(key + "-players.servers-a-core", mean > 0 ? frame_us / mean : 0, "servers",
               gl::Det::measured);
    mean_trace.x.push_back(count);
    mean_trace.y.push_back(mean);
    p99_trace.x.push_back(count);
    p99_trace.y.push_back(p99);
  }
  lab.table(played);
  lab.table(times, gl::Det::measured);
  lab.note(
      "Each server is a match of bots on our server over the simulated network, every bot a "
      "classic-protocol client; the servers run one after another. frame: the server's own frame "
      "(the network read, the game's rules, the snapshots written), on the wall clock (measured, "
      "this machine, this build). bots a frame: every client's tick with the bot thinking in it, "
      "which a real server does not carry. servers a core holds: the 13 ms frame over the mean.");
  gl::LineChart chart{
      {"THE SERVER'S FRAME AGAINST ITS PLAYERS",
       std::to_string(servers) + " servers a size, " + to_fixed(minutes, 1) + " minutes on " + name,
       "players on the server", "us"},
      {mean_trace, p99_trace},
      {},
      {},
      {},
      false,
      false,
      false};
  const gl::Chart c = gl::line_chart(chart);
  lab.shot({"frame",
            c.title,
            "The server's frame (the mean over its servers and the worst server's p99) against "
            "the bots playing on it.",
            "a frame that grows with the players roughly in a line (each one's move, its "
            "snapshot and what it sees), far under the 13 ms a frame has.",
            {{"players", lab.arg("players")},
             {"servers", lab.arg("servers")},
             {"minutes", lab.arg("minutes")}}},
           gl::to_picture(c.plot), gl::Det::measured);
}

const gl::LabSpec kSpec{
    "se-load",
    gl::LabKind::bench,
    "game:shooters_edge",
    "bots as classic-protocol clients on many of our servers (games/shooters_edge/net/"
    "classic_team.hpp)",
    "net",
    "With the game's own bots as the players, what does one of our servers' frames cost as the "
    "players grow, and how many servers does one core hold?",
    {
        {"basedir", "", "a folder holding the game folder the map is read from"},
        {"game", "id1", "the game folder under --basedir"},
        {"map", "dm3", "with --basedir: the map"},
        {"players", "2,4,8,16", "the players a server, comma-separated (2 to 16)"},
        {"servers", "4", "servers of each size, each its own seed"},
        {"minutes", "2", "each match's length"},
    },
    "--players=2,4 --servers=1 --minutes=0.1",
    run,
};
GE_LAB(kSpec);

}  // namespace
