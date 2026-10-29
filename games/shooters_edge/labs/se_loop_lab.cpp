// LAB `se-loop`: does the classic game loop play a match as specified, and do its thrown entities
// fly as recorded? Two parts, both off the real code. THE MATCH
// (games/shooters_edge/sim/match.hpp): two clients over latency and packet loss on the test arena
// (a door, its trigger, a plat, a health box), a rocket, a grenade bouncing, the door blocked by a
// player; tables of the program's events and the loop's counts, the arena from above and from the
// side with every path, the door's and the plat's height against the clock, and each client's
// prediction corrections. THE VECTORS (tests/golden/shooter_toss_recording.txt): every recorded
// flight (a grenade, a rocket, a gib, a nail, the grenade again at 100 ms frames) replayed through
// `physics_toss`; a table of each one's worst error, and each flight from the side, ours over the
// recording's.
//
// ge:summary Lab: the classic loop's scripted match and the thrown entities against their vectors
// ge:keywords lab se-loop match arena door plat rocket grenade bounce prediction correction
// ge:keywords toss-recording entity-vectors paths top-view side-view server-hash
// ge:skill shooter labs
// ge:test tests/shooters_edge_match_test.cpp tests/shooter_toss_recording_test.cpp
#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "shooter/toss_recording.hpp"
#include "shooters_edge/sim/match.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace se = shooters_edge::sim;
using ge::core::num::to_fixed;

constexpr double kNan = std::numeric_limits<double>::quiet_NaN();

std::string hex64(uint64_t v) {
  char buf[17];
  std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(v));
  return buf;
}

std::string upper(std::string s) {
  for (char& c : s)
    if (c >= 'a' && c <= 'z')
      c = static_cast<char>(c - 32);
    else if (c == '_')
      c = ' ';
  return s;
}

// A box brush's bounds off its axial planes.
void brush_bounds(const sh::Brush& b, sh::Vec3f& lo, sh::Vec3f& hi) {
  for (const sh::Plane& p : b.planes)
    for (int i = 0; i < 3; i++) {
      if (p.normal[i] == 1) hi[i] = p.dist;
      if (p.normal[i] == -1) lo[i] = -p.dist;
    }
}

// A rectangle in the chart's two axes (0 x, 1 y, 2 z), closed and broken off from the next.
void rect(gl::Trace& t, sh::Vec3f lo, sh::Vec3f hi, int a, int b) {
  const double xs[5] = {lo[a], hi[a], hi[a], lo[a], lo[a]};
  const double ys[5] = {lo[b], lo[b], hi[b], hi[b], lo[b]};
  for (int i = 0; i < 5; i++) {
    t.x.push_back(xs[i]);
    t.y.push_back(ys[i]);
  }
  t.x.push_back(lo[a]);
  t.y.push_back(kNan);
}

void path(gl::Trace& t, const std::vector<sh::Vec3f>& pts, int a, int b) {
  for (const sh::Vec3f& p : pts) {
    t.x.push_back(p[a]);
    t.y.push_back(p[b]);
  }
  if (!pts.empty()) {
    t.x.push_back(pts.back()[a]);
    t.y.push_back(kNan);
  }
}

void shoot(gl::Lab& lab, const std::string& id, const gl::Chart& chart, const std::string& caption,
           const std::string& look_for,
           const std::vector<std::pair<std::string, std::string>>& in) {
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({id, chart.title, caption, look_for, in}, gl::to_picture(chart.plot));
}

// The arena's walls, door, plat and trigger in two axes, and both players' and the missiles' paths.
void draw_arena(gl::Lab& lab, const se::Arena& arena, const se::MatchReport& r, int a, int b,
                const std::vector<std::pair<std::string, std::string>>& in) {
  const bool above = b == 1;
  gl::Trace walls{above ? "walls and ledge" : "walls, ledge and floor", {}, {}};
  for (size_t i = above ? 1 : 0; i < arena.world_brushes.size(); i++) {
    sh::Vec3f lo, hi;
    brush_bounds(arena.world_brushes[i], lo, hi);
    const bool ledge = i == arena.world_brushes.size() - 1;
    if (!above && !ledge && (lo.y > 0 || hi.y < 0))
      continue;  // from the side: what the line y = 0 cuts, and the ledge behind it
    rect(walls, lo, hi, a, b);
  }
  gl::Trace movers{"door and plat (closed, down)", {}, {}};
  for (const sh::Brush* br : {&arena.door_brush, &arena.plat_brush}) {
    sh::Vec3f lo, hi;
    brush_bounds(*br, lo, hi);
    rect(movers, lo, hi, a, b);
  }
  gl::Trace trigger{"door trigger", {}, {}};
  rect(trigger, arena.trigger_mins, arena.trigger_maxs, a, b);

  gl::Trace players[se::kPlayers] = {{"player 1", {}, {}}, {"player 2", {}, {}}};
  for (int p = 0; p < se::kPlayers; p++) {
    std::vector<sh::Vec3f> pts;
    for (const se::FrameRecord& f : r.frames) pts.push_back(f.origin[p]);
    path(players[p], pts, a, b);
  }
  gl::Trace rockets{"rockets", {}, {}}, grenades{"grenade", {}, {}};
  for (const se::MissileTrack& m : r.missiles) path(m.rocket ? rockets : grenades, m.points, a, b);

  gl::LineChart c;
  c.spikes = false;
  const char* axis[3] = {"x (units)", "y (units)", "z (units)"};
  c.frame = {above ? "THE ARENA FROM ABOVE" : "THE ARENA FROM THE SIDE",
             std::to_string(r.frames.size()) + " server frames of 13 ms, " +
                 std::to_string(r.missiles.size()) + " missiles",
             axis[a], axis[b]};
  c.traces = {walls, movers, trigger, players[0], players[1], rockets, grenades};
  if (above) {
    shoot(lab, "arena-top", gl::line_chart(c),
          "From above (x east, y north): the arena's walls and the ledge in the south-east, the "
          "door in the doorway at x = 128..144 and the plat beside the ledge, the door's trigger "
          "west of it; player 1's path from the west room through the doorway and back, player "
          "2's from the plat onto the ledge; each missile's path a server frame a point.",
          "player 1's line runs east along y = 0 through the trigger and the doorway and back "
          "into it; player 2's leaves the plat for the ledge and drops into the east room; the "
          "rocket at player 1's feet is a short dash near x = -410; the grenade glances off the "
          "dividing wall and the north wall; no path crosses a wall",
          in);
  } else {
    shoot(lab, "arena-side", gl::line_chart(c),
          "From the south (x east, z up): the floor, the walls and lintel across the doorway's "
          "line, the ledge (z = 104) and the door and the plat where they start; the players' "
          "origins (24 above their feet) and every missile.",
          "the players' origins ride at z = 24 on the floor; player 2 rises with the plat from 32 "
          "to 128 and jumps on the ledge; player 1's own rocket throws him up and back against "
          "the west wall; the grenade's arcs shrink at each bounce and it rests on the floor",
          in);
  }
}

void draw_movers(gl::Lab& lab, const se::MatchReport& r,
                 const std::vector<std::pair<std::string, std::string>>& in) {
  gl::Trace door{"door bottom z", {}, {}}, plat{"plat z", {}, {}};
  for (const se::FrameRecord& f : r.frames) {
    door.x.push_back(f.time);
    door.y.push_back(f.door_z);
    plat.x.push_back(f.time);
    plat.y.push_back(f.plat_z);
  }
  gl::LineChart c;
  c.spikes = false;
  c.frame = {"THE DOOR AND THE PLAT", "their origins' height against the server's time",
             "server time (s)", "z (units)"};
  c.traces = {door, plat};
  for (const se::GameEvent& e : r.events)
    if (e.kind == se::EventKind::door_blocked || e.kind == se::EventKind::door_opening)
      c.marks.push_back({e.time, se::event_name(e.kind)});
  shoot(lab, "movers", gl::line_chart(c),
        "The door's and the plat's origin height a server frame at a time: the door rises 120 at "
        "100 u/s when player 1 touches its trigger, waits 3 s and comes down; the plat rides 96 "
        "up at 64 u/s, waits 1.5 s at each end, forever. Marked: each time the door starts to "
        "open, and when it was blocked.",
        "straight ramps that stop exactly at 0, 96 and 120 (pushers stop at their think, never "
        "past it); the door's second descent turns back up at the 'door blocked' mark while "
        "player 1 stands in the doorway",
        in);
}

void draw_corrections(gl::Lab& lab, const se::MatchReport& r,
                      const std::vector<std::pair<std::string, std::string>>& in) {
  gl::Trace t[se::kPlayers] = {{"client 1", {}, {}}, {"client 2", {}, {}}};
  for (int p = 0; p < se::kPlayers; p++)
    for (const se::Correction& c : r.clients[p].corrections) {
      t[p].x.push_back(c.t_ms * 0.001);
      t[p].y.push_back(c.error);
    }
  gl::LineChart c;
  c.points = true;
  c.spikes = false;
  c.frame = {"PREDICTION CORRECTIONS",
             "worst client 1 " + to_fixed(r.clients[0].worst_correction, 3) + " u, client 2 " +
                 to_fixed(r.clients[1].worst_correction, 3) + " u",
             "client time (s)", "error (units)"};
  c.traces = {t[0], t[1]};
  c.rules = {static_cast<double>(sh::kSmoothMin)};
  shoot(lab, "corrections", gl::line_chart(c),
        "At each reply: how far the newest command's prediction moved when the server's state "
        "replaced the client's guess (the acknowledged state on the protocol's 1/8 grid, the newer "
        "commands run through the same move). The dashed rule is 4 units: above it the error is "
        "smoothed away instead of snapped.",
        "almost every dot under 1/8 unit (the grid); the large ones are what a client cannot "
        "know: at 0.2 s player 1 thrown by his own rocket (the client's clock starts at 0, the "
        "server's at 1 s), from 1.4 s player 2 carried by the plat the client draws where the "
        "last reply put it, near 3.1 s player 1 meeting the rising door",
        in);
}

void run_vectors(gl::Lab& lab) {
  const std::string& file = lab.arg("vectors");
  std::ifstream f(file);
  if (!f) {
    lab.fail("se-loop: cannot read the vectors " + file,
             "next: run the lab from the repository's root, or pass --vectors=<file>");
    return;
  }
  std::stringstream ss;
  ss << f.rdbuf();
  sh::TossRecording rec;
  std::string error;
  if (!sh::parse_toss_recording(ss.str(), rec, error)) {
    lab.fail("se-loop: " + file + ": " + error,
             "next: fix that line (format: plugins/shooter/toss_recording.hpp)");
    return;
  }
  gl::LabTable table{"every recorded flight against its vectors",
                     {"flight", "movetype", "frame s", "frames", "out", "exact", "worst position u",
                      "worst velocity u/s", "worst trace", "first out"},
                     {}};
  int frames = 0, out = 0, exact = 0;
  for (const sh::TossFlight& fl : rec.flights) {
    const sh::TossReplay r = sh::replay_toss(rec, fl);
    int same = 0;
    std::string first;
    for (const sh::TossCheck& c : r.frames) {
      if (c.ok && c.position_error == 0 && c.velocity_error == 0) same++;
      if (!c.ok && first.empty()) first = "frame " + std::to_string(c.index) + ": " + c.mismatch;
    }
    frames += static_cast<int>(r.frames.size());
    out += r.failed;
    exact += same;
    if (r.failed > 0)
      lab.fail("se-loop: flight " + fl.name + " leaves the tolerance on " +
                   std::to_string(r.failed) + " frames (first " + first + ")",
               "next: run tests/shooter_toss_recording_test.cpp and read the step it names in "
               "plugins/shooter/physics.cpp's physics_toss");
    const char* mt = fl.movetype == sh::kMovetypeBounce       ? "bounce"
                     : fl.movetype == sh::kMovetypeFlyMissile ? "flymissile"
                     : fl.movetype == sh::kMovetypeFly        ? "fly"
                                                              : "toss";
    table.rows.push_back({fl.name, mt, to_fixed(fl.frametime, 3), std::to_string(r.frames.size()),
                          std::to_string(r.failed), std::to_string(same),
                          to_fixed(r.max_position_error, 6), to_fixed(r.max_velocity_error, 6),
                          to_fixed(r.max_trace_error, 6), first.empty() ? "-" : first});
    lab.metric("vectors." + fl.name + ".worst-position", r.max_position_error, "u");
    lab.metric("vectors." + fl.name + ".worst-velocity", r.max_velocity_error, "u/s");
    lab.metric("vectors." + fl.name + ".frames-out", r.failed, "count");

    gl::Trace ours{"ours", {}, {}}, recorded{"recorded", {}, {}}, floor{"floor and wall", {}, {}};
    ours.x.push_back(fl.origin.x);
    ours.y.push_back(fl.origin.z);
    recorded.x.push_back(fl.origin.x);
    recorded.y.push_back(fl.origin.z);
    for (size_t i = 0; i < r.frames.size(); i++) {
      ours.x.push_back(r.frames[i].ours.origin.x);
      ours.y.push_back(r.frames[i].ours.origin.z);
      recorded.x.push_back(fl.frames[i].origin.x);
      recorded.y.push_back(fl.frames[i].origin.z);
    }
    floor.x = {-128, 256, 256, 320};  // the scene's floor at z = 0 and its wall's face at x = 256
    floor.y = {0, 0, 256, 256};
    gl::LineChart c;
    c.spikes = false;
    c.frame = {upper(fl.name) + " FROM THE SIDE",
               std::string(mt) + " at " + to_fixed(fl.frametime * 1000, 0) + " ms frames, worst " +
                   to_fixed(r.max_position_error, 6) + " u",
               "x (units)", "z (units)"};
    c.traces = {floor, recorded, ours};
    shoot(lab, "vectors-" + fl.name, gl::line_chart(c),
          "The " + fl.name +
              " flight from the side, a point a physics frame: ours drawn over "
              "the recorded vectors, so the recording shows only where the two part (a point-sized "
              "entity against the scene's floor at z = 0 and its "
              "wall at x = 256). " +
              std::to_string(same) + " of " + std::to_string(r.frames.size()) +
              " frames equal in every printed digit.",
          "one line (the recording's colour nowhere); a bounce rebounds at 1.5 and rests once "
          "under 60 "
          "up; a missile and a nail stop dead on the wall; the gib slides and stops on the floor",
          {{"vectors", file}, {"flight", fl.name}});
  }
  lab.table(table);
  lab.metric("vectors.frames", frames, "count");
  lab.metric("vectors.frames-out", out, "count");
  lab.metric("vectors.frames-exact", exact, "count");
}

void run(gl::Lab& lab) {
  se::MatchOptions opt;
  opt.frames = static_cast<int>(lab.arg_num("frames"));
  opt.latency_ms = static_cast<int>(lab.arg_num("latency"));
  opt.lose_every = static_cast<int>(lab.arg_num("lose-every"));
  opt.client_frame_ms = static_cast<int>(lab.arg_num("client-frame"));
  opt.predict = lab.arg("predict") != "0";
  if (opt.frames < 1 || opt.client_frame_ms < 1 || opt.latency_ms < 0) {
    lab.fail("se-loop: --frames and --client-frame must be at least 1 and --latency not negative",
             "next: make lab LAB=se-loop ARGS=--help");
    return;
  }
  const std::vector<std::pair<std::string, std::string>> in = {
      {"frames", lab.arg("frames")},
      {"latency ms", lab.arg("latency")},
      {"lose every", lab.arg("lose-every")},
      {"client frame ms", lab.arg("client-frame")},
      {"predict", lab.arg("predict")}};

  const se::MatchReport r = se::run_match(opt);
  se::Arena arena;
  se::build_arena(arena);

  // The program's events, and how many of each.
  gl::LabTable events{"the program's events in the order they happened",
                      {"server time s", "event", "entity", "other", "where", "amount"},
                      {}};
  std::map<std::string, int> by_kind;
  for (const se::GameEvent& e : r.events) {
    const std::string name = se::event_name(e.kind);
    by_kind[name]++;
    events.rows.push_back(
        {to_fixed(e.time, 3), name, std::to_string(e.ent), std::to_string(e.other),
         to_fixed(e.where.x, 1) + " " + to_fixed(e.where.y, 1) + " " + to_fixed(e.where.z, 1),
         to_fixed(e.amount, 1)});
  }
  lab.table(events);
  for (const auto& [name, n] : by_kind) {
    std::string key = name;
    for (char& ch : key)
      if (ch == ' ') ch = '-';
    lab.metric("match.events." + key, n, "count");
  }

  const sh::LoopCounts& k = r.counts;
  gl::LabTable counts{"what the loop did", {"count", "value"}, {}};
  const std::pair<const char*, int64_t> rows[] = {{"physics frames", k.physics_frames},
                                                  {"commands run", k.commands},
                                                  {"commands lost, replayed", k.commands_lost},
                                                  {"speed check cuts", k.speed_cuts},
                                                  {"thinks", k.thinks},
                                                  {"trigger touches", k.trigger_touches},
                                                  {"impacts", k.impacts},
                                                  {"player touches", k.player_touches},
                                                  {"pushes", k.pushes},
                                                  {"pushes blocked", k.pushes_blocked},
                                                  {"new missiles", k.new_missiles},
                                                  {"sounds dropped", k.sounds_dropped}};
  for (const auto& [name, v] : rows) {
    counts.rows.push_back({name, std::to_string(v)});
    std::string key = name;
    for (char& ch : key)
      if (ch == ' ' || ch == ',') ch = '-';
    lab.metric("match.counts." + key, static_cast<double>(v), "count");
  }
  for (int p = 0; p < se::kPlayers; p++) {
    const se::ClientReport& c = r.clients[p];
    const std::string who = "client " + std::to_string(p + 1);
    counts.rows.push_back({who + " packets sent", std::to_string(c.packets_sent)});
    counts.rows.push_back({who + " packets lost", std::to_string(c.packets_lost)});
    counts.rows.push_back({who + " replies", std::to_string(c.replies)});
    counts.rows.push_back({who + " worst correction u", to_fixed(c.worst_correction, 4)});
    const std::string key = "match.client-" + std::to_string(p + 1);
    lab.metric(key + ".packets-lost", c.packets_lost, "count");
    lab.metric(key + ".worst-correction", c.worst_correction, "u");
  }
  counts.rows.push_back({"server hash at the end", hex64(r.final_hash)});
  lab.table(counts);

  if (by_kind["door blocked"] == 0)
    lab.fail("se-loop: the door was never blocked",
             "next: run tests/shooters_edge_match_test.cpp and read push_move in "
             "plugins/shooter/push.cpp");
  if (k.new_missiles == 0)
    lab.fail("se-loop: no new missile ran ahead",
             "next: run tests/shooter_physics_test.cpp (a_new_missile_runs_005_s_ahead)");

  draw_arena(lab, arena, r, 0, 1, in);
  draw_arena(lab, arena, r, 0, 2, in);
  draw_movers(lab, r, in);
  draw_corrections(lab, r, in);
  run_vectors(lab);
}

const gl::LabSpec kSpec{
    "se-loop",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "games/shooters_edge/sim/match.hpp over plugins/shooter's loop (frame, physics, push, "
    "client, predict), and physics_toss against tests/golden/shooter_toss_recording.txt",
    "shooter",
    "Does the classic loop play a scripted match as specified (players, a rocket, a bouncing "
    "grenade, a door that is blocked, a plat, prediction over loss), and do its thrown entities "
    "fly as their recorded vectors?",
    {
        {"frames", "1100", "server frames of 13 ms to run the match for"},
        {"latency", "40", "one-way latency, ms"},
        {"lose-every", "23", "player 2 loses every Nth packet (0: none)"},
        {"client-frame", "8", "the clients' render frame, ms"},
        {"predict", "1", "1: the clients predict; 0: they show the server's state"},
        {"vectors", "tests/golden/shooter_toss_recording.txt", "the recorded flights"},
    },
    "--frames=700",
    run,
};
GE_LAB(kSpec);

}  // namespace
