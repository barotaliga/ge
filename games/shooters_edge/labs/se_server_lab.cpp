// LAB `se-server`: does the server's host run a full server of bots on the classic loop, and does
// its input log replay the run to the bit? The host (games/shooters_edge/server/host.hpp) on the
// test arena with every slot a stand-in bot (server/stand_in.hpp) for `--seconds` of server time:
// a table per player (how far it ran, its share of frames in the air, its commands holding
// attack, the health it lost, how it ended), the loop's counts, the server's hash every 10 s;
// then the input log written to bytes, read back and replayed, and the first frame that leaves it
// named (the lab fails on any); the arena from above with every player's trail. The wall time of
// the run and of the replay is measured, never hashed.
//
// ge:summary Lab: a full server of stand-in bots on the host, and its input log replayed
// ge:keywords lab se-server dedicated-server host bots stand-in input-log replay trails
// ge:keywords server-hash players-table frame-time
// ge:skill shooter labs
// ge:test tests/shooters_edge_host_test.cpp
#include <chrono>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/server/host.hpp"
#include "shooters_edge/server/stand_in.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace se = shooters_edge::server;
namespace sim = shooters_edge::sim;
using ge::core::num::to_fixed;

constexpr double kNan = std::numeric_limits<double>::quiet_NaN();

std::string hex64(uint64_t v) {
  char buf[17];
  std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(v));
  return buf;
}

// A stand-in whose commands holding attack are counted on the way through.
struct Counted {
  se::StandIn bot;
  sh::CmdSource inner;
  int attacks = 0;
};

sh::MoveCmd counted_next(void* ctx, const sh::Server& sv, int slot, int msec) {
  Counted& c = *static_cast<Counted*>(ctx);
  const sh::MoveCmd cmd = c.inner.next(c.inner.ctx, sv, slot, msec);
  if ((cmd.buttons & sh::kButtonAttack) != 0) c.attacks++;
  return cmd;
}

struct PlayerTrack {
  std::vector<sh::Vec3f> points;  // a point every 10 frames
  double distance = 0;            // units, on the plan
  int airborne = 0;               // frames off the ground
  double hurt = 0;                // health lost
  int downed = 0;                 // times its health reached 0
};

void brush_bounds(const sh::Brush& b, sh::Vec3f& lo, sh::Vec3f& hi) {
  for (const sh::Plane& p : b.planes)
    for (int i = 0; i < 3; i++) {
      if (p.normal[i] == 1) hi[i] = p.dist;
      if (p.normal[i] == -1) lo[i] = -p.dist;
    }
}

void rect(gl::Trace& t, sh::Vec3f lo, sh::Vec3f hi) {
  const double xs[5] = {lo.x, hi.x, hi.x, lo.x, lo.x};
  const double ys[5] = {lo.y, lo.y, hi.y, hi.y, lo.y};
  for (int i = 0; i < 5; i++) {
    t.x.push_back(xs[i]);
    t.y.push_back(ys[i]);
  }
  t.x.push_back(lo.x);
  t.y.push_back(kNan);
}

void draw_trails(gl::Lab& lab, const sim::Arena& arena, const std::vector<PlayerTrack>& tracks,
                 double seconds, const std::vector<std::pair<std::string, std::string>>& in) {
  gl::Trace walls{"walls and ledge", {}, {}};
  const size_t walls_end = arena.world_brushes.size() - (arena.roofed ? 1 : 0);
  for (size_t i = 1; i < walls_end; i++) {  // not the floor, nor the roof
    sh::Vec3f lo, hi;
    brush_bounds(arena.world_brushes[i], lo, hi);
    rect(walls, lo, hi);
  }
  gl::Trace movers{"door and plat", {}, {}};
  for (const sh::Brush* br : {&arena.door_brush, &arena.plat_brush}) {
    sh::Vec3f lo, hi;
    brush_bounds(*br, lo, hi);
    rect(movers, lo, hi);
  }
  gl::Trace spawns{"spawn points", {}, {}};
  for (const sh::Vec3f& s : arena.spawn) {
    rect(spawns, {s.x - 16, s.y - 16, 0}, {s.x + 16, s.y + 16, 0});
  }
  gl::LineChart c;
  c.spikes = false;
  c.frame = {"THE SERVER'S PLAYERS FROM ABOVE",
             std::to_string(tracks.size()) + " stand-in bots, " + to_fixed(seconds, 0) +
                 " s of server time",
             "x (units)", "y (units)"};
  c.traces = {walls, movers, spawns};
  for (size_t p = 0; p < tracks.size(); p++) {
    gl::Trace t{"player " + std::to_string(p + 1), {}, {}};
    for (const sh::Vec3f& pt : tracks[p].points) {
      t.x.push_back(pt.x);
      t.y.push_back(pt.y);
    }
    c.traces.push_back(t);
  }
  const gl::Chart chart = gl::line_chart(c);
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach("trails.svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({"trails", chart.title,
            "From above (x east, y north): the arena's walls, the door and the plat, the eight "
            "spawn points as small squares, and every player's origin every tenth server frame. "
            "The players are stand-in wanderers: they run, turn at walls, jump now and then, and "
            "fire at whoever is ahead and near.",
            "every line starts in its own spawn square; the lines fill both rooms and cross the "
            "doorway; no line crosses a wall or leaves the arena (the roof keeps a player a blast "
            "throws inside)",
            in},
           gl::to_picture(chart.plot));
}

void run(gl::Lab& lab) {
  const int bots = static_cast<int>(lab.arg_num("bots"));
  const double seconds = lab.arg_num("seconds");
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));
  if (bots < 1 || bots > sh::kMaxClients || seconds <= 0) {
    lab.fail("se-server: --bots must be 1.." + std::to_string(sh::kMaxClients) +
                 " and --seconds more than 0",
             "next: make lab LAB=se-server ARGS=--help");
    return;
  }
  const std::vector<std::pair<std::string, std::string>> in = {
      {"bots", lab.arg("bots")}, {"seconds", lab.arg("seconds")}, {"seed", lab.arg("seed")}};

  se::Host h;
  se::HostSettings s;
  s.maxclients = bots > 8 ? bots : 8;
  s.seed = seed;
  std::string error;
  if (!se::host_create(h, s, error)) {
    lab.fail("se-server: " + error,
             "next: read host_create in games/shooters_edge/server/host.cpp");
    return;
  }
  std::vector<Counted> counted(static_cast<size_t>(bots));
  for (int b = 0; b < bots; b++) {
    Counted& c = counted[static_cast<size_t>(b)];
    c.bot = se::make_stand_in(seed, b);
    c.inner = se::stand_in_source(c.bot);
    se::host_add_bot(h, {&c, counted_next});
  }

  std::vector<PlayerTrack> tracks(static_cast<size_t>(bots));
  std::vector<float> last_health(static_cast<size_t>(bots), 0);
  for (int b = 0; b < bots; b++) {
    const sh::Edict& e = h.sv.edicts[static_cast<size_t>(b) + 1];
    tracks[static_cast<size_t>(b)].points.push_back(e.origin);
    last_health[static_cast<size_t>(b)] = e.health;
  }
  gl::LabTable hashes{"the server's hash every 10 s of server time", {"server time s", "hash"}, {}};
  const double step = h.sv.set.mintic;
  const auto frames = static_cast<int64_t>(seconds / step + 0.5);
  const auto every_10s = static_cast<int64_t>(10 / step + 0.5);
  const auto t0 = std::chrono::steady_clock::now();
  for (int64_t n = 0; n < frames; n++) {
    std::vector<sh::Vec3f> before(static_cast<size_t>(bots));
    for (int b = 0; b < bots; b++) before[static_cast<size_t>(b)] = h.sv.edicts[b + 1].origin;
    se::host_frame(h, nullptr, 0);
    for (int b = 0; b < bots; b++) {
      const sh::Edict& e = h.sv.edicts[static_cast<size_t>(b) + 1];
      PlayerTrack& t = tracks[static_cast<size_t>(b)];
      const sh::Vec3f& p = before[static_cast<size_t>(b)];
      t.distance += ge::core::hypot(e.origin.x - p.x, e.origin.y - p.y);
      if ((e.flags & sh::kFlagOnGround) == 0) t.airborne++;
      float& lh = last_health[static_cast<size_t>(b)];
      if (e.health < lh) t.hurt += lh - e.health;
      if (lh > 0 && e.health <= 0) t.downed++;
      lh = e.health;
      if ((n + 1) % 10 == 0) t.points.push_back(e.origin);
    }
    if ((n + 1) % every_10s == 0)
      hashes.rows.push_back(
          {to_fixed(h.sv.time - h.sv.set.start_time, 1), hex64(sh::server_hash(h.sv))});
  }
  const double run_wall =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  const uint64_t digest = sh::server_hash(h.sv);

  gl::LabTable players{
      "every player over the run (the test program floors health at 1: no one dies "
      "before the game's own program, M18p)",
      {"player", "ran u", "airborne %", "commands attacking", "health lost", "downed",
       "health at the end"},
      {}};
  int downed = 0;
  int64_t attacks = 0;
  for (int b = 0; b < bots; b++) {
    const PlayerTrack& t = tracks[static_cast<size_t>(b)];
    const sh::Edict& e = h.sv.edicts[static_cast<size_t>(b) + 1];
    const Counted& c = counted[static_cast<size_t>(b)];
    players.rows.push_back({std::to_string(b + 1), to_fixed(t.distance, 0),
                            to_fixed(100.0 * t.airborne / static_cast<double>(frames), 1),
                            std::to_string(c.attacks), to_fixed(t.hurt, 0),
                            std::to_string(t.downed), to_fixed(e.health, 0)});
    const std::string key = "players." + std::to_string(b + 1);
    lab.metric(key + ".ran", t.distance, "u");
    lab.metric(key + ".health-lost", t.hurt, "hp");
    downed += t.downed;
    attacks += c.attacks;
  }
  lab.table(players);
  lab.table(hashes);

  const sh::LoopCounts& k = h.sv.counts;
  gl::LabTable counts{"what the loop did", {"count", "value"}, {}};
  const std::pair<const char*, int64_t> rows[] = {{"server frames", h.sv.frame},
                                                  {"physics frames", k.physics_frames},
                                                  {"commands run", k.commands},
                                                  {"commands attacking", attacks},
                                                  {"thinks", k.thinks},
                                                  {"impacts", k.impacts},
                                                  {"player touches", k.player_touches},
                                                  {"pushes", k.pushes},
                                                  {"new missiles", k.new_missiles},
                                                  {"players downed", downed}};
  for (const auto& [name, v] : rows) {
    counts.rows.push_back({name, std::to_string(v)});
    std::string key = name;
    for (char& ch : key)
      if (ch == ' ') ch = '-';
    lab.metric("server." + key, static_cast<double>(v), "count");
  }
  counts.rows.push_back({"server hash at the end", hex64(digest)});
  lab.table(counts);
  lab.metric("server.wall", run_wall, "s", gl::Det::measured);
  lab.metric("server.frame-cost", run_wall / static_cast<double>(frames) * 1e6, "us",
             gl::Det::measured);

  // The input log: to bytes, back, replayed.
  const std::vector<uint8_t> bytes = se::write_input_log(h.log);
  se::InputLog back;
  if (!se::read_input_log(bytes, back, error)) {
    lab.fail("se-server: the input log does not read back: " + error,
             "next: run tests/shooters_edge_host_test.cpp (the round trip) and read "
             "games/shooters_edge/server/input_log.cpp");
    return;
  }
  int diverged = -1;
  std::vector<uint64_t> replayed;
  const auto t1 = std::chrono::steady_clock::now();
  if (!se::replay_input_log(back, diverged, error, &replayed)) {
    lab.fail("se-server: the input log does not replay: " + error,
             "next: read replay_input_log in games/shooters_edge/server/host.cpp");
    return;
  }
  const double replay_wall =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - t1).count();
  if (diverged >= 0)
    lab.fail("se-server: the replay leaves the run at frame " + std::to_string(diverged) + " of " +
                 std::to_string(back.frames.size()),
             "next: run tests/shooters_edge_host_test.cpp and look for a draw or a read the log "
             "does not carry (a bot writing the server, a draw off the server's own state)");
  gl::LabTable log{"the input log", {"what", "value"}, {}};
  log.rows.push_back({"frames", std::to_string(back.frames.size())});
  log.rows.push_back({"packets", std::to_string(back.packets.size())});
  log.rows.push_back({"slot events", std::to_string(back.events.size())});
  log.rows.push_back({"bytes", std::to_string(bytes.size())});
  log.rows.push_back(
      {"bytes per second", to_fixed(static_cast<double>(bytes.size()) / seconds, 0)});
  log.rows.push_back(
      {"frames the replay leaves", diverged < 0 ? "none" : std::to_string(diverged)});
  lab.table(log);
  lab.metric("log.bytes", static_cast<double>(bytes.size()), "bytes");
  lab.metric("log.frames", static_cast<double>(back.frames.size()), "count");
  lab.metric("log.first-diverged", diverged, "frame");
  lab.metric("log.replay-wall", replay_wall, "s", gl::Det::measured);

  draw_trails(lab, *h.arena, tracks, seconds, in);
}

const gl::LabSpec kSpec{
    "se-server",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/server: the host, its bot slots, the stand-in bot and the input log, over "
    "plugins/shooter's loop",
    "shooter",
    "Does the server's host run a full server of bots on the classic loop, how do the players "
    "move and fight, and does the run's input log replay it to the bit?",
    {
        {"bots", "8", "bot slots to fill with stand-ins"},
        {"seconds", "60", "seconds of server time to run"},
        {"seed", "1", "the run's seed (the bots' draws)"},
    },
    "--seconds=10",
    run,
};
GE_LAB(kSpec);

}  // namespace
