// LAB `move-predict`: a moving body (engine/move) played over the network (engine/net's
// replication) under each condition preset: the server steps it from the client's commands, the
// client predicts it from the same commands and reconciles with each snapshot, the commands the
// server has not acknowledged replayed over its word and a correction smoothed out by the spring.
// Each movement scenario's scripted hand drives the client off its own prediction, as a player's
// would. For every command it compares what the client drew when it made it with where the server
// put the body when it stepped it: the prediction's error (zero while nothing is lost) and the
// drawn gap (what the player saw). Then one run with a knock-back the client could not predict
// (the server shoves the body), charted on its own path as the server, the raw prediction and the
// drawn body; and a long frame (a quarter second) stepped whole against split under the cap
// (wire.hpp), on the stairs.
//
// ge:summary Lab: a moving body predicted over each network preset: the gap, a correction charted
// ge:keywords lab move predict prediction reconcile correction knock-back network presets
// ge:keywords drawn-gap misprediction spring command-split long-frame hitch stairs move-predict
// ge:skill move net labs
// ge:test tests/move_wire_test.cpp
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/stats.hpp"
#include "move/box_world.hpp"
#include "move/push.hpp"
#include "move/scenarios.hpp"
#include "move/wire.hpp"
#include "net/conditions.hpp"
#include "net/node.hpp"
#include "net/replicate.hpp"
#include "net/sim_network.hpp"
#include "net/transport.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace mv = ge::move;

std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at <= s.size()) {
    const size_t comma = s.find(',', at);
    const std::string part =
        s.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
    if (!part.empty()) out.push_back(part);
    if (comma == std::string::npos) break;
    at = comma + 1;
  }
  return out;
}

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

// What the server and the predicting client step with: the scenario's knobs and world.
struct Body {
  const mv::MoveSpec* spec = nullptr;
  mv::World world;
  double max_dt = 0;
};

void step_body(void* ctx, double* fields, const uint8_t* input) {
  const auto* b = static_cast<const Body*>(ctx);
  mv::step_wire(*b->spec, b->world, b->max_dt, fields, input);
}

gn::NodeConfig node_config(bool host, uint64_t seed) {
  gn::NodeConfig c;
  c.protocol = gn::protocol_id("move-predict", 1);
  c.accept = host;
  c.max_peers = host ? 2 : 1;
  c.channels.count = 2;
  c.channels.delivery[0] = gn::Delivery::unreliable;
  c.channels.delivery[1] = gn::Delivery::unreliable;
  c.timeout_us = 3000000;
  c.seed = seed;
  return c;
}

struct RunOptions {
  const mv::MoveScenario* sc = nullptr;
  gn::LinkConditions link;
  int hz = 60;
  uint32_t seed = 1;
  double max_dt = 0.05;
  double push_at = -1;  // s into the script: the server shoves the body (-1: never)
  mv::Vec3 push_dv{0, 0, 0};
};

// Per command (by its number): the client's raw prediction and its drawn body when it made it,
// and the server's body when it stepped it.
struct RunResult {
  int commands = 0;
  std::vector<mv::Vec3> raw, drawn, server;
  std::vector<bool> made, stepped;
  std::vector<double> err, gap;  // m: each command's prediction error and drawn gap
  uint64_t corrections = 0, repeated = 0;
  double worst_correction = 0;  // m
  uint64_t digest = 0xcbf29ce484222325ull;
  bool owned = false;
};

void mix(uint64_t& h, double d) {
  uint64_t bits;
  std::memcpy(&bits, &d, sizeof bits);
  for (int i = 0; i < 8; i++) h = (h ^ ((bits >> (8 * i)) & 0xff)) * 0x100000001b3ull;
}

double dist(const mv::Vec3& a, const mv::Vec3& b) { return mv::length(mv::sub(a, b)); }

RunResult run_one(const RunOptions& k) {
  const mv::MoveScenario& sc = *k.sc;
  const mv::MoveSpec& spec = *mv::find_preset(sc.preset);
  const mv::BoxWorld bw = mv::scenario_world(sc, spec);
  Body body{&spec, mv::as_world(bw), k.max_dt};
  gn::Schema schema;
  schema.fields = mv::kWireFields;
  for (int i = 0; i < mv::kWireFields; i++) {
    const mv::WireField& w = mv::wire_fields()[i];
    schema.spec[i] = {w.min, w.max, w.bits, false, std::strcmp(w.name, "flags") != 0};
  }
  schema.input_size = mv::kWireCmdBytes;
  schema.step = step_body;
  schema.ctx = &body;
  gn::ReplicateConfig config;
  config.tick_hz = k.hz;
  config.max_entities = 4;
  config.ring = k.hz;

  gn::SimNetwork net = gn::make_sim_network(k.seed, k.link);
  gn::Transport host_t, client_t;
  gn::open_sim_transport(host_t, net, gn::sim_address(0, 1));
  gn::open_sim_transport(client_t, net, gn::sim_address(1, 1));
  auto host = std::make_unique<gn::Node>();
  auto client = std::make_unique<gn::Node>();
  gn::node_init(*host, node_config(true, uint64_t{k.seed} * 7 + 1), host_t);
  gn::node_init(*client, node_config(false, uint64_t{k.seed} * 1000 + 1), client_t);
  auto server = std::make_unique<gn::ReplicateServer>();
  gn::replicate_server_init(*server, schema, config, *host);
  const int peer = gn::node_connect(*client, gn::sim_address(0, 1), 0);
  auto rc = std::make_unique<gn::ReplicateClient>();
  gn::replicate_client_init(*rc, schema, config, *client, peer);

  mv::MoveState start =
      mv::stand_at(spec, body.world, {sc.start_x * spec.unit, 0, sc.start_z * spec.unit});
  start.mode = sc.mode;
  RunResult o;
  const int n = static_cast<int>(sc.seconds * k.hz + 0.5);
  o.commands = n;
  o.raw.resize(static_cast<size_t>(n) + 1);
  o.drawn.resize(static_cast<size_t>(n) + 1);
  o.server.resize(static_cast<size_t>(n) + 1);
  o.made.assign(static_cast<size_t>(n) + 1, false);
  o.stepped.assign(static_cast<size_t>(n) + 1, false);
  const int64_t tick_us = 1000000 / k.hz;
  const double dt = 1.0 / k.hz;
  const int push_seq = k.push_at < 0 ? -1 : static_cast<int>(k.push_at * k.hz);
  bool pushed = false;
  mv::ScriptMemory mem;
  double acc = 0;
  int slot = -1;
  int64_t now = 0;
  uint64_t seen_corrections = 0;
  const int most = n + 10 * k.hz;  // the handshake and the slowest link's catching up
  for (int t = 1; t <= most; t++) {
    now += tick_us;
    gn::sim_advance(net, now);
    gn::node_update(*host, now);
    for (const gn::Event& e : host->events) {
      if (e.type != gn::EventType::connected) continue;
      double row[mv::kWireFields];
      mv::pack_body(start, row);
      gn::replicate_spawn(*server, e.peer, row);
    }
    gn::replicate_server_receive(*server);
    // The knock-back the client never sees coming: the server's own shove, between commands.
    if (slot >= 0 && !pushed && push_seq > 0 &&
        server->clients[static_cast<size_t>(slot)].applied >= static_cast<uint32_t>(push_seq)) {
      const uint16_t id = server->clients[static_cast<size_t>(slot)].entity;
      mv::MoveState s = mv::unpack_body(gn::replicate_get(*server, id));
      mv::push(s, k.push_dv);
      double row[mv::kWireFields];
      mv::pack_body(s, row);
      gn::replicate_set(*server, id, row);
      pushed = true;
    }
    gn::replicate_server_tick(*server);
    gn::node_flush(*host, now);
    if (slot >= 0) {
      const gn::ReplicateClientSlot& sl = server->clients[static_cast<size_t>(slot)];
      if (sl.fresh && sl.applied >= 1 && sl.applied <= static_cast<uint32_t>(n)) {
        const double* v = gn::replicate_get(*server, sl.entity);
        o.server[sl.applied] = {v[0], v[1], v[2]};
        o.stepped[sl.applied] = true;
        for (int i = 0; i < 6; i++) mix(o.digest, v[i]);
      }
    }
    gn::node_update(*client, now);
    gn::replicate_client_receive(*rc);
    if (rc->stats.corrections != seen_corrections) {
      // The correction on the feet alone: the prediction of the newest command, replayed over
      // the server's word, against what it was when it was made (the flags and timers moved too,
      // but they are not a distance).
      seen_corrections = rc->stats.corrections;
      const auto at = static_cast<size_t>(rc->seq);
      if (rc->predicting && at < o.made.size() && o.made[at]) {
        const mv::Vec3 now_at{rc->predicted[0], rc->predicted[1], rc->predicted[2]};
        const double m = dist(now_at, o.raw[at]) / spec.unit;
        o.worst_correction = m > o.worst_correction ? m : o.worst_correction;
      }
    }
    acc += gn::replicate_client_dilation(*rc);
    while (acc >= 1) {
      acc -= 1;
      if (!rc->predicting || rc->seq >= static_cast<uint32_t>(n)) continue;
      // The hand reads the body as the client predicts it, as a player reads the screen.
      const mv::MoveState s = mv::unpack_body(rc->predicted);
      mv::MoveCmd cmd = sc.script(spec, s, dt, mem);
      if (cmd.yaw == 0 && sc.start_yaw != 0) cmd.yaw = sc.start_yaw;
      const uint32_t next = rc->seq + 1;
      // The command's milliseconds, a whole number each, summing to the seconds stepped.
      const auto msec = static_cast<int>(next * 1000 / static_cast<uint32_t>(k.hz) -
                                         (next - 1) * 1000 / static_cast<uint32_t>(k.hz));
      uint8_t bytes[mv::kWireCmdBytes];
      mv::pack_cmd(cmd, msec, bytes);
      gn::replicate_client_input(*rc, bytes);
      double drawn[gn::kMaxFields];
      gn::replicate_client_predicted(*rc, drawn);
      const auto at = static_cast<size_t>(rc->seq);
      o.raw[at] = {rc->predicted[0], rc->predicted[1], rc->predicted[2]};
      o.drawn[at] = {drawn[0], drawn[1], drawn[2]};
      o.made[at] = true;
    }
    gn::node_flush(*client, now);
    if (slot < 0 && rc->own != gn::kNoEntity) {
      for (int p = 0; p < static_cast<int>(server->clients.size()); p++)
        if (server->clients[static_cast<size_t>(p)].entity == rc->own) slot = p;
    }
    if (slot >= 0 && server->clients[static_cast<size_t>(slot)].applied >= static_cast<uint32_t>(n))
      break;
  }
  o.owned = slot >= 0;
  o.corrections = rc->stats.corrections;
  o.repeated = server->stats.repeated;
  for (int i = 1; i <= n; i++) {
    const auto at = static_cast<size_t>(i);
    if (!o.made[at] || !o.stepped[at]) continue;
    o.err.push_back(dist(o.raw[at], o.server[at]) / spec.unit);
    o.gap.push_back(dist(o.drawn[at], o.server[at]) / spec.unit);
  }
  return o;
}

double worst(const std::vector<double>& v) {
  double w = 0;
  for (const double x : v) w = x > w ? x : w;
  return w;
}

// The knock-back, charted on its own path: the server, the raw prediction, the drawn body.
void run_push(gl::Lab& lab, const RunOptions& base, const std::string& preset) {
  RunOptions k = base;
  k.sc = mv::find_move_scenario(lab.arg("push-scenario").c_str());
  if (k.sc == nullptr) {
    lab.fail("no movement scenario " + lab.arg("push-scenario"),
             "Name one of engine/move/scenarios.cpp's ids with --push-scenario; skill: move.");
    return;
  }
  gn::conditions_preset(preset, k.link);
  const mv::MoveSpec& spec = *mv::find_preset(k.sc->preset);
  const double side = lab.arg_num("push-side"), up = lab.arg_num("push-up");
  k.push_at = lab.arg_num("push-at");
  k.push_dv = {side * spec.unit, up * spec.unit, 0};
  const RunResult o = run_one(k);
  gl::LineChart c;
  c.frame = {"A KNOCK-BACK THE CLIENT COULD NOT PREDICT, ON " + preset,
             k.sc->id + std::string(", ") + fmt(side, 0) + " m/s sideways and " + fmt(up, 0) +
                 " m/s up at " + fmt(k.push_at, 2) + " s, " + std::to_string(k.hz) + " Hz",
             "t (s, by command)", "x (m, sideways)"};
  gl::Trace server{"server", {}, {}}, raw{"prediction", {}, {}}, drawn{"drawn", {}, {}};
  const double t0 = k.push_at - 0.15, t1 = k.push_at + 0.6;
  for (int i = 1; i <= o.commands; i++) {
    const auto at = static_cast<size_t>(i);
    const double t = static_cast<double>(i) / k.hz;
    if (t < t0 || t > t1 || !o.made[at] || !o.stepped[at]) continue;
    server.x.push_back(t), server.y.push_back(o.server[at].x / spec.unit);
    raw.x.push_back(t), raw.y.push_back(o.raw[at].x / spec.unit);
    drawn.x.push_back(t), drawn.y.push_back(o.drawn[at].x / spec.unit);
  }
  c.traces = {server, raw, drawn};
  c.marks = {{k.push_at, "shove"}};
  c.spikes = false;
  const gl::Chart chart = gl::line_chart(c);
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach("correction.svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({"correction",
            c.frame.title,
            "The body's sideways place by command: where the server stepped it, where the client "
            "predicted it when it made that command, and where it drew it. The server shoves it; "
            "the client hears a round trip later, replays its commands over the shoved body and "
            "draws the jump across through the spring.",
            "the prediction jumping onto the server's line about a round trip after the shove and "
            "staying on it; the drawn line bending over to it within a few tenths of a second, "
            "with no step and no overshoot.",
            {{"preset", preset},
             {"scenario", k.sc->id},
             {"push", lab.arg("push-side") + "," + lab.arg("push-up")},
             {"seed", lab.arg("seed")}}},
           gl::to_picture(chart.plot));
  // How long the drawn body took to come within a tenth of the bound of the server's.
  const double bound = lab.arg_num("bound");
  double settled = -1;
  for (int i = static_cast<int>(k.push_at * k.hz); i <= o.commands; i++) {
    const auto at = static_cast<size_t>(i);
    if (!o.made[at] || !o.stepped[at]) continue;
    const double g = dist(o.drawn[at], o.server[at]) / spec.unit;
    if (g > bound / 10)
      settled = -1;
    else if (settled < 0)
      settled = static_cast<double>(i) / k.hz - k.push_at;
  }
  lab.metric("push-" + preset + "-correction", o.worst_correction, "m");
  lab.metric("push-" + preset + "-settled", settled, "s");
  if (settled < 0)
    lab.fail("the drawn body never came back within " + fmt(bound / 10, 3) +
                 " m of the server's after the shove on " + preset,
             "Read reconcile and the spring in engine/net/replicate_client.cpp; skill: net.");
}

// A long frame (a hitch) on the stairs: stepped whole, split under the cap, and at the step rate.
void run_hitch(gl::Lab& lab) {
  const mv::MoveScenario* sc = mv::find_move_scenario("stairs");
  if (sc == nullptr) return;
  const mv::MoveSpec& spec = *mv::find_preset(sc->preset);
  const mv::BoxWorld bw = mv::scenario_world(*sc, spec);
  const mv::World world = mv::as_world(bw);
  const double frame = lab.arg_num("hitch"), cap = lab.arg_num("max-step");
  struct Way {
    const char* label;
    double dt, cap;
  };
  const Way ways[] = {{"every 1/120 s", 1.0 / 120, 0},
                      {"a long frame stepped whole", frame, 0},
                      {"a long frame split under the cap", frame, cap}};
  gl::LineChart c;
  c.frame = {"A LONG FRAME ON THE STAIRS: WHOLE AGAINST SPLIT",
             fmt(frame * 1000, 0) + " ms frames, the cap " + fmt(cap * 1000, 1) + " ms",
             "along the run (m)", "height of the feet (m)"};
  gl::LabTable t{"long frames on the stairs",
                 {"way", "steps", "pieces a frame", "end along (m)", "end height (m)"},
                 {}};
  for (const Way& w : ways) {
    mv::MoveState s =
        mv::stand_at(spec, world, {sc->start_x * spec.unit, 0, sc->start_z * spec.unit});
    mv::ScriptMemory mem;
    gl::Trace tr{w.label, {s.pos.z / spec.unit}, {s.pos.y / spec.unit}};
    const int steps = static_cast<int>(sc->seconds / w.dt + 0.5);
    for (int i = 0; i < steps; i++) {
      const mv::MoveCmd cmd = sc->script(spec, s, w.dt, mem);
      // A piece at a time, as step_move_split steps them (tests/move_wire_test.cpp holds the
      // two equal), so the split line shows every piece's place and the whole one a frame's.
      const int pieces = mv::split_count(w.dt, w.cap);
      for (int p = 0; p < pieces; p++) {
        mv::step_move(spec, world, s, cmd, w.dt / pieces);
        tr.x.push_back(s.pos.z / spec.unit);
        tr.y.push_back(s.pos.y / spec.unit);
      }
    }
    c.traces.push_back(tr);
    t.rows.push_back({w.label, std::to_string(steps), std::to_string(mv::split_count(w.dt, w.cap)),
                      fmt(s.pos.z / spec.unit, 3), fmt(s.pos.y / spec.unit, 3)});
    lab.metric(std::string("hitch-") +
                   (w.cap > 0     ? "split"
                    : w.dt > 0.05 ? "whole"
                                  : "fine") +
                   "-height",
               s.pos.y / spec.unit, "m");
  }
  c.spikes = false;
  lab.table(std::move(t));
  const gl::Chart chart = gl::line_chart(c);
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach("hitch.svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({"hitch",
            c.frame.title,
            "The stairs scenario's feet from the side when every frame is long: each frame's "
            "command stepped whole, and split in equal pieces under the cap, against the fine "
            "step rate.",
            "the split line climbing riser by riser like the fine one and reaching as far; the "
            "whole line a point a frame, cutting each corner and falling short (one long step "
            "gains a short step's speed).",
            {{"hitch", lab.arg("hitch")}, {"max-step", lab.arg("max-step")}}},
           gl::to_picture(chart.plot));
}

void run(gl::Lab& lab) {
  const auto hz = static_cast<int>(lab.arg_num("hz"));
  const double bound = lab.arg_num("bound");
  RunOptions base;
  base.hz = hz;
  base.seed = static_cast<uint32_t>(lab.arg_num("seed"));
  base.max_dt = lab.arg_num("max-step");
  gl::LabTable table{
      "each scenario over each preset",
      {"preset", "scenario", "commands", "repeated", "corrections", "worst correction (m)",
       "worst error (m)", "drawn gap p95 (m)", "worst drawn gap (m)", "server path"},
      {}};
  std::vector<gl::Sample> gaps;
  for (const std::string& preset : split(lab.arg("presets"))) {
    if (!gn::conditions_preset(preset, base.link)) {
      lab.fail("no condition preset " + preset,
               "Name one of engine/net/conditions.cpp's presets with --presets; skill: net.");
      continue;
    }
    gl::Sample all{preset, {}};
    double preset_worst = 0;
    for (const std::string& id : split(lab.arg("scenarios"))) {
      RunOptions k = base;
      k.sc = mv::find_move_scenario(id.c_str());
      if (k.sc == nullptr || k.sc->animate != nullptr) {
        lab.fail("no static movement scenario " + id,
                 "Name one of engine/move/scenarios.cpp's ids whose world stands still with "
                 "--scenarios; skill: move.");
        continue;
      }
      const RunResult o = run_one(k);
      if (!o.owned || o.gap.empty()) {
        lab.fail(preset + " " + id + ": the client never stepped its body",
                 "Read the handshake in engine/net/node.cpp under the preset; skill: net.");
        continue;
      }
      const gl::Summary g = gl::summarize(o.gap);
      const double w = worst(o.gap), e = worst(o.err);
      preset_worst = w > preset_worst ? w : preset_worst;
      all.values.insert(all.values.end(), o.gap.begin(), o.gap.end());
      char hash[20];
      std::snprintf(hash, sizeof hash, "%016llx", static_cast<unsigned long long>(o.digest));
      table.rows.push_back({preset, id, std::to_string(o.commands), std::to_string(o.repeated),
                            std::to_string(o.corrections), fmt(o.worst_correction, 4), fmt(e, 4),
                            fmt(g.p95, 4), fmt(w, 4), hash});
      if (preset == "lan" && (o.corrections > 0 || e > 0))
        lab.fail("lan " + id + ": a prediction on a clean link was corrected (" +
                     std::to_string(o.corrections) + ", worst " + fmt(e, 6) + " m)",
                 "The server and the client stepped the same command apart: read step_wire and "
                 "pack_body in engine/move/wire.cpp (a field the step reads left out); skill: "
                 "move.");
    }
    lab.metric("gap-" + preset + "-worst", preset_worst, "m");
    if (preset_worst > bound)
      lab.fail(preset + ": the drawn body strayed " + fmt(preset_worst, 3) +
                   " m from the server's, past the bound of " + fmt(bound, 3) + " m",
               "Chart the run with --presets=" + preset +
                   " and read the corrections; the spring is in engine/net/replicate_client.cpp; "
                   "skill: net.");
    gaps.push_back(std::move(all));
  }
  lab.table(std::move(table));
  if (!gaps.empty()) {
    const gl::Chart chart = gl::box_chart(
        {"THE DRAWN BODY AGAINST THE SERVER'S, EACH PRESET",
         "every command of every scenario, " + std::to_string(hz) + " Hz", "drawn gap (m)", ""},
        gaps);
    const std::string svg = gl::to_svg(chart.plot);
    lab.attach("gaps.svg", std::vector<uint8_t>(svg.begin(), svg.end()));
    lab.shot({"gaps",
              chart.title,
              "How far the body the client drew when it made each command was from where the "
              "server stepped it with that command, one box a preset, every scenario together.",
              "lan at zero (nothing lost, nothing corrected); every other preset's box and "
              "whisker under the --bound (1 m), the whiskers the few "
              "commands the redundancy could not save.",
              {{"presets", lab.arg("presets")},
               {"scenarios", lab.arg("scenarios")},
               {"seed", lab.arg("seed")}}},
             gl::to_picture(chart.plot));
  }
  run_push(lab, base, lab.arg("push-preset"));
  run_hitch(lab);
  lab.note(
      "error: the client's raw prediction when it made a command against the server's body "
      "after stepping it (zero unless a command was lost past the redundancy and the server "
      "stepped the one before it again). drawn gap: what the client drew then (the prediction "
      "plus what was left of a correction) against the same. repeated: server ticks that "
      "stepped the last command again. server path: a hash of every body the server stepped. "
      "Bound " +
      fmt(bound, 3) + " m.");
}

const gl::LabSpec kSpec{
    "move-predict",
    gl::LabKind::sim,
    "engine",
    "engine/move (wire) over engine/net (replicate)",
    "move",
    "Played over each network preset, how far does a predicted moving body stray from the "
    "server's, how is a misprediction corrected, and does a long frame split under the cap step "
    "as the fine rate does?",
    {
        {"presets", "lan,broadband,wifi,mobile,bad,awful", "condition presets, comma-separated"},
        {"scenarios", "strafe-jump,stairs,ramp-steep,crouch-tunnel,slide,double-jump,ladder",
         "movement scenarios whose world stands still, comma-separated"},
        {"hz", "60", "the step's rate and the commands' (10 to 240)"},
        {"max-step", "0.02", "the longest piece a command is stepped in, s"},
        {"bound", "1", "the farthest the drawn body may stray from the server's, m"},
        {"push-preset", "wifi", "the preset the knock-back is charted on"},
        {"push-scenario", "strafe-jump", "the scenario the knock-back interrupts"},
        {"push-at", "1.5", "when the server shoves the body, s into the script"},
        {"push-side", "8", "the shove sideways, m/s"},
        {"push-up", "6", "the shove up, m/s"},
        {"hitch", "0.25", "the long frame stepped whole and split, s"},
        {"seed", "1", "the network's stream"},
    },
    "--presets=lan,bad --scenarios=strafe-jump,stairs",
    run,
};
GE_LAB(kSpec);

}  // namespace
