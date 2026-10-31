// LAB `se-lobby`: the lobby before the match, played on the listen server the window runs
// (client/listen.hpp with the lobby on, client/lobby_room.hpp). The built-in arena, the baseline
// mod, the player in slot 0 and three bots (a novice, an average and an expert fighter) seated in
// the warmup; the player runs a slow circle while the bots roam and fight. On a script of the
// player's console words: `team switch` (red to blue), `ready` (the countdown starts), `break`
// (it stops), `ready` again, the countdown runs out and the map restarts for the match, which is
// played for `--match` seconds.
//
// The pictures: the lobby panel (hud/lobby_panel.hpp) at each moment (the warmup, after the
// switch, the countdown, the break) at 1280x720 and 390x844 over a painted stand-in for the
// game; the players from above, warmup and match apart; the ready count and every player's frags
// over time, the warmup's frags thrown away at the match's start. The metrics (countdowns,
// stopped, frames to the match, the frags at the start and at the end) are deterministic.
//
// ge:summary Lab: the lobby on the listen server: bots, teams, ready, break, countdown, warmup
// ge:keywords lab se-lobby lobby warmup countdown ready break team bots panel free-roam
// ge:skill shooter labs
// ge:test tests/shooters_edge_lobby_test.cpp
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "progs/layout.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/hud/lobby_panel.hpp"
#include "shooters_edge/labs/arena_chart.hpp"
#include "shooters_edge/sim/arena.hpp"
#include "ui/raster.hpp"

namespace {

namespace gl = ge::lab;
namespace ui = ge::ui;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace cl = shooters_edge::client;
namespace sl = shooters_edge::labs;
using ge::core::num::to_fixed;

constexpr int kPlayers = 4;

// A dim hall with a lit doorway: the game under the panel.
ui::Image backdrop(int w, int h) {
  ui::DrawList dl;
  dl.begin(w, h);
  const float fw = static_cast<float>(w), fh = static_cast<float>(h);
  ui::Box b;
  const ui::Stop wall[2] = {{0, ui::hex(0x2a2420)}, {1, ui::hex(0x4a3e34)}};
  b.rect = {0, 0, fw, fh * 0.6f};
  b.fill = ui::linear(0, 0, 0, b.rect.h, wall, 2);
  dl.box(b);
  const ui::Stop floor[2] = {{0, ui::hex(0x3a3028)}, {1, ui::hex(0x16120e)}};
  b.rect = {0, fh * 0.6f, fw, fh * 0.4f};
  b.fill = ui::linear(0, 0, 0, b.rect.h, floor, 2);
  dl.box(b);
  const ui::Stop door[2] = {{0, ui::hex(0xffd9a0)}, {1, ui::hex(0xa86a30)}};
  b.rect = {fw * 0.3f, fh * 0.24f, fw * 0.12f, fh * 0.36f};
  b.fill = ui::linear(0, 0, 0, b.rect.h, door, 2);
  b.radius = fw * 0.02f;
  dl.box(b);
  ui::Image img = ui::make_image(w, h, ui::hex(0x000000));
  ui::render(dl, img);
  return img;
}

gl::Picture panel_picture(const shooters_edge::hud::LobbyView& v, int w, int h) {
  ui::DrawList dl;
  dl.begin(w, h);
  shooters_edge::hud::draw_lobby(dl, v, w, h);
  ui::Image img = backdrop(w, h);
  ui::render(dl, img);
  gl::Picture p;
  p.width = img.width, p.height = img.height, p.rgba = std::move(img.rgba);
  return p;
}

float frags(const cl::Listen& l, int slot) {
  return ge::progs::as_float(
      *ge::progs::entity_field(l.server->q.vm, slot + 1, ge::progs::kFFrags));
}

struct Step {
  double at_s;  // server seconds into the warmup
  const char* line;
  const char* shot;  // the panel's picture after it ("" none)
  const char* caption;
};

void run(gl::Lab& lab) {
  const double countdown = lab.arg_num("countdown");
  const double match_s = lab.arg_num("match");
  ge::progs::Progs progs;
  std::string error;
  if (!cl::baseline_progs(progs, error)) {
    lab.fail("se-lobby: " + error, "next: fix the QC sources it names. Skill: progs.");
    return;
  }
  cl::Listen l;
  cl::ListenOptions opt;
  opt.console = {"deathmatch 1", "fraglimit 30", "timelimit 10"};
  opt.player = "player";
  opt.lobby = true;
  opt.lobby_settings.countdown_s = countdown;
  cl::SearchPath none;
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(none, "arena", error);
  if (!map || !cl::listen_start(l, progs, std::move(map), opt, error)) {
    lab.fail("se-lobby: the arena did not start: " + error,
             "next: run tests/shooters_edge_lobby_test.cpp. Skill: shooter.");
    return;
  }
  for (const char* skill : {"novice", "average", "expert"})
    cl::listen_lobby(l, std::string("addbot ") + skill);
  if (sv::lobby_seated(l.room->lobby) != kPlayers) {
    lab.fail("se-lobby: the bots were not seated (" +
                 std::to_string(sv::lobby_seated(l.room->lobby)) + " of 4)",
             "next: read the lines addbot printed; run tests/shooters_edge_lobby_test.cpp");
    return;
  }

  const Step steps[] = {
      {1.0, "lobby", "warmup",
       "The warmup: three bots seated and ready as they joined, the player not yet. The banner "
       "says 3 of 4 ready and the keys; the roster has both teams, the player's row lit, the host "
       "tagged."},
      {4.0, "team switch", "switched",
       "After `team switch` (F2): the player has moved from red to blue."},
      {6.0, "ready", "countdown",
       "After `ready` (F1): everyone is ready, so the countdown runs, its seconds large."},
      {7.5, "break", "break",
       "After `break` (F1 again): the countdown stopped, back to warmup, the player not ready."},
      {10.0, "ready", "", ""},
  };
  const double tic = cl::listen_tic(l);
  const double t0 = l.server->sv.time;
  sh::MoveCmd cmd;
  cmd.forward = 200;
  size_t next = 0;
  int frame = 0, match_frame = -1, after_match = 0;
  const int match_frames = static_cast<int>(match_s / tic);
  // Over time: the ready count, every player's frags; from above: each one's path.
  std::vector<gl::Trace> frag_lines(kPlayers);
  gl::Trace ready_line{"ready (of 4)", {}, {}};
  std::vector<std::vector<sh::Vec3f>> warm_paths(kPlayers), match_paths(kPlayers);
  std::vector<std::string> names(kPlayers);
  float warm_frags = 0, start_frags = -1;
  const std::vector<std::pair<std::string, std::string>> in = {
      {"countdown", to_fixed(countdown, 1) + " s"}, {"map", "arena"}, {"bots", "3"}};
  while (after_match < match_frames && frame < 100000) {
    const double t = static_cast<double>(frame) * tic;  // the run's own seconds
    while (next < std::size(steps) && match_frame < 0 &&
           l.server->sv.time - t0 >= steps[next].at_s) {
      cl::listen_lobby(l, steps[next].line);
      if (*steps[next].shot) {
        for (const auto& [w, h] : {std::pair{1280, 720}, std::pair{390, 844}}) {
          const shooters_edge::hud::LobbyView v =
              cl::lobby_view(*l.room, l.server->sv.time, 0, true);
          const std::string size = std::to_string(w) + "x" + std::to_string(h);
          lab.shot({std::string("panel-") + steps[next].shot + "-" + size,
                    std::string("THE LOBBY, ") + steps[next].shot + ", AT " + size,
                    steps[next].caption,
                    "the banner's count and keys, the teams' rows and their marks, nothing off "
                    "the screen or under another",
                    {{"viewport", size}, {"after", steps[next].line}}},
                   panel_picture(v, w, h));
        }
      }
      next++;
    }
    l.view.y += 0.6f;  // a slow circle
    cl::listen_frame(l, cmd);
    if (!l.failed.empty()) {
      lab.fail("se-lobby: the match did not start: " + l.failed,
               "next: run tests/shooters_edge_lobby_test.cpp");
      return;
    }
    if (l.restarted) {
      l.restarted = false;
      match_frame = frame;
      start_frags = 0;
      for (int s = 0; s < kPlayers; s++) start_frags += frags(l, s);
    }
    if (match_frame >= 0) after_match++;
    for (int s = 0; s < kPlayers; s++) {
      const sh::Vec3f p = l.server->sv.edicts[static_cast<size_t>(s) + 1].origin;
      if (frame % 10 == 0) (match_frame < 0 ? warm_paths : match_paths)[s].push_back(p);
      frag_lines[s].x.push_back(t);
      frag_lines[s].y.push_back(frags(l, s));
      if (match_frame < 0) warm_frags = std::max(warm_frags, frags(l, s));
    }
    ready_line.x.push_back(t);
    ready_line.y.push_back(sv::lobby_ready_count(l.room->lobby));
    frame++;
  }
  const sv::Lobby& lobby = l.room->lobby;
  for (int s = 0; s < kPlayers; s++) {
    const sv::LobbySeat& seat = lobby.seats[static_cast<size_t>(s)];
    names[s] = seat.name + " (" + sv::team_name(seat.team) + ")";
    frag_lines[s].label = names[s];
  }
  float end_frags = 0;
  for (int s = 0; s < kPlayers; s++) end_frags += frags(l, s);

  gl::LabTable timeline{"the lobby's lines", {"line"}, {}};
  for (const std::string& p : l.prints)
    if (p.find("ready") != std::string::npos || p.find("team") != std::string::npos ||
        p.find("countdown") != std::string::npos || p.find("match") != std::string::npos ||
        p.find("joins") != std::string::npos || p.find("lobby") != std::string::npos ||
        p.find("  ") == 0)
      timeline.rows.push_back({p});
  lab.table(timeline);
  const double match_at = match_frame * tic;
  lab.metric("countdowns", lobby.countdowns, "count");
  lab.metric("countdowns-stopped", lobby.stopped, "count");
  lab.metric("matches", lobby.matches, "count");
  lab.metric("warmup.frames", match_frame, "frames");
  lab.metric("warmup.most-frags", warm_frags, "frags");
  lab.metric("match.frags-at-start", start_frags, "frags");
  lab.metric("match.frags-at-end", end_frags, "frags");
  if (match_frame < 0 || start_frags != 0)
    lab.fail("se-lobby: the match did not start clean (frame " + std::to_string(match_frame) +
                 ", " + to_fixed(start_frags, 0) + " frags at its start)",
             "next: run tests/shooters_edge_lobby_test.cpp; read listen_frame's restart");

  // Over time.
  {
    gl::LineChart c;
    c.spikes = false;
    c.frame = {"READY AND FRAGS THROUGH THE WARMUP AND THE MATCH",
               "the match starts at " + to_fixed(match_at, 1) + " s; the warmup's frags go",
               "time (s)", "count"};
    c.traces = frag_lines;
    c.traces.push_back(ready_line);
    c.marks = {{match_at, "match"}};
    const gl::Chart chart = gl::line_chart(c);
    sl::shoot(lab, "timeline", chart,
              "Every player's frags and the count of ready players by server time. The bots are "
              "ready from the start (3), the player's ready makes 4 and the countdown runs, his "
              "break stops it, his second ready runs it out, and the map restarts: every line of "
              "frags drops to 0 and the match's own count begins.",
              "the ready count's steps at the script's words; every frag line at 0 at the match "
              "mark; frags rising again only after it",
              in);
  }
  // From above.
  shooters_edge::sim::Arena arena;
  shooters_edge::sim::build_arena(arena);
  for (int part = 0; part < 2; part++) {
    gl::Trace walls{"walls", {}, {}};
    for (size_t i = 1; i < arena.world_brushes.size(); i++) {
      sh::Vec3f lo, hi;
      sl::brush_bounds(arena.world_brushes[i], lo, hi);
      sl::rect(walls, lo, hi, 0, 1);
    }
    gl::LineChart c;
    c.spikes = false;
    c.frame = {part == 0 ? "THE WARMUP FROM ABOVE: FREE ROAM" : "THE MATCH FROM ABOVE",
               part == 0 ? to_fixed(match_at, 1) + " s before the countdown ran out"
                         : to_fixed(match_s, 1) + " s of the match",
               "x (units)", "y (units)"};
    c.traces = {walls};
    for (int s = 0; s < kPlayers; s++) {
      gl::Trace t{names[s], {}, {}};
      sl::path(t, (part == 0 ? warm_paths : match_paths)[static_cast<size_t>(s)], 0, 1);
      c.traces.push_back(t);
    }
    sl::shoot(lab, part == 0 ? "warmup-above" : "match-above", gl::line_chart(c),
              part == 0 ? "Every player's origin every tenth frame of the warmup: the player's "
                          "slow circle and the bots roaming and fighting over both rooms while "
                          "nothing counts."
                        : "The same after the map restarted for the match: everyone from a "
                          "spawn point again.",
              "the lines inside the walls; in the match every line starting at a spawn", in);
  }
}

const gl::LabSpec kSpec{
    "se-lobby",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/server/lobby and client/lobby_room: the lobby before the match on the "
    "listen server, its bots, its panel",
    "shooter",
    "Does the lobby seat bots on teams, run the warmup free roam, start the countdown when "
    "everyone is ready, stop it on a break, and restart the map for a clean match?",
    {
        {"countdown", "3", "the countdown's length, s"},
        {"match", "10", "seconds of the match to play after the restart"},
    },
    "--match=2",
    run,
};
GE_LAB(kSpec);

}  // namespace
