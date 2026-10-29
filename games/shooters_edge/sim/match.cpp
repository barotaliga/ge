// The scripted match's timeline: the clients, the packets and replies in flight, the server's
// frames (match.hpp states the order).
//
// ge:summary The scripted match's millisecond timeline: clients, packets, replies, server frames
// ge:keywords run_match match_script client-sim packets-in-flight replies prediction tracks
// ge:skill shooter
// ge:test tests/shooters_edge_match_test.cpp
#include "shooters_edge/sim/match.hpp"

#include <algorithm>
#include <memory>

#include "shooter/frame.hpp"
#include "shooter/usercmd.hpp"

namespace shooters_edge::sim {

namespace {

using K = ScriptKey;
using A = ScriptAct;

struct InFlight {
  int arrive_ms = 0;
  sh::Packet packet;
};

struct Reply {
  int arrive_ms = 0;
  uint32_t ack = 0;
  sh::NetPlayer own, other;
  sh::MoveCmd other_cmd;
  int other_msec = 0;
  sh::Vec3f door, plat;
};

struct ClientSim {
  size_t script_at = 0;
  sh::CmdClock clock;
  std::vector<sh::MoveCmd> sent;  // [sequence - 1]
  uint32_t sequence = 0;
  sh::Vec3f view;
  sh::Smoothing smooth;
  Reply last;  // the newest reply heard
  uint8_t impulse = 0;
  bool acked = false;
  sh::MoveKeys keys;
};

sh::KeyState& key_of(sh::MoveKeys& k, ScriptKey key) {
  switch (key) {
    case K::forward:
      return k.forward;
    case K::back:
      return k.back;
    case K::left:
      return k.moveleft;
    case K::right:
      return k.moveright;
    case K::jump:
      return k.jump;
    case K::attack:
      return k.attack;
    case K::speed:
      return k.speed;
  }
  return k.forward;
}

void apply(ClientSim& c, const ScriptStep& s) {
  switch (s.act) {
    case A::press:
      sh::key_press(key_of(c.keys, s.key));
      break;
    case A::release:
      sh::key_release(key_of(c.keys, s.key));
      break;
    case A::yaw:
      c.view.y = s.value;
      break;
    case A::pitch:
      c.view.x = s.value;
      break;
    case A::impulse:
      c.impulse = static_cast<uint8_t>(s.value);
      break;
  }
}

struct World {
  const Arena* arena = nullptr;
  std::vector<sh::BrushModel> models;  // world, door, plat
};

// The client's own player predicted over its commands after the acknowledged one.
bool predict(const World& w, const sh::MoveSettings& set, ClientSim& c, int now_ms, int latency_ms,
             sh::Vec3f& out, int& unacked) {
  if (!c.acked) return false;
  const sh::ClientModel movers[2] = {{&w.models[1], c.last.door}, {&w.models[2], c.last.plat}};
  sh::MoveWorld bare;
  sh::client_world(w.models[0], movers, 2, nullptr, 0, bare);
  const int ms = sh::other_msec(c.last.arrive_ms * 0.001, c.last.other_msec, now_ms * 0.001,
                                2 * latency_ms * 0.001);
  const sh::MoveState other = sh::predict_other(c.last.other, c.last.other_cmd, ms, set, bare);
  sh::MoveWorld world;
  sh::client_world(w.models[0], movers, 2, &other.origin, 1, world);
  unacked = static_cast<int>(c.sequence - c.last.ack);
  const sh::MoveState s =
      sh::predict_player(c.last.own, c.sent.data() + c.last.ack, unacked, set, world);
  out = s.origin;
  return true;
}

}  // namespace

const std::vector<ScriptStep>& match_script() {
  static const std::vector<ScriptStep> script = [] {
    std::vector<ScriptStep> s = {
        // Player 1: a rocket at his own feet, the health box, east through the doorway.
        {100, 0, A::pitch, K::forward, 70},
        {150, 0, A::press, K::attack, 0},
        {260, 0, A::release, K::attack, 0},
        {420, 0, A::pitch, K::forward, 0},
        {900, 0, A::yaw, K::forward, 0},
        {900, 0, A::press, K::forward, 0},
        {1900, 0, A::press, K::jump, 0},
        {2000, 0, A::release, K::jump, 0},
        {3500, 0, A::release, K::forward, 0},
        // Back into the doorway as the door comes down: it meets him and goes back up.
        {6700, 0, A::press, K::back, 0},
        {7100, 0, A::release, K::back, 0},
        {8000, 0, A::press, K::forward, 0},
        {8400, 0, A::release, K::forward, 0},
        // Player 2: up on the plat, off onto the ledge, a grenade, then rockets.
        {3300, 1, A::yaw, K::forward, -90},
        {3300, 1, A::press, K::forward, 0},
        {3700, 1, A::release, K::forward, 0},
        {3800, 1, A::yaw, K::forward, 120},
        {3800, 1, A::pitch, K::forward, -20},
        {4000, 1, A::press, K::attack, 0},
        {4100, 1, A::release, K::attack, 0},
        {4600, 1, A::impulse, K::forward, kImpulseRocket},
        {4700, 1, A::yaw, K::forward, 90},
        {4700, 1, A::pitch, K::forward, 0},
        {5000, 1, A::press, K::attack, 0},
        {5100, 1, A::release, K::attack, 0},
        {5600, 1, A::yaw, K::forward, 100},
        {5600, 1, A::press, K::forward, 0},
        {5650, 1, A::press, K::jump, 0},
        {6100, 1, A::yaw, K::forward, 130},
        {6100, 1, A::press, K::left, 0},
        {6400, 1, A::yaw, K::forward, 90},
        {6400, 1, A::release, K::left, 0},
        {6400, 1, A::press, K::right, 0},
        {6700, 1, A::yaw, K::forward, 50},
        {7000, 1, A::release, K::right, 0},
        {7000, 1, A::release, K::jump, 0},
        {7600, 1, A::release, K::forward, 0},
    };
    std::stable_sort(s.begin(), s.end(),
                     [](const ScriptStep& a, const ScriptStep& b) { return a.t_ms < b.t_ms; });
    return s;
  }();
  return script;
}

MatchReport run_match(const MatchOptions& opt) {
  MatchReport rep;
  Arena arena;
  build_arena(arena);
  World w{&arena, arena.models()};

  sh::ServerSettings set;
  set.maxclients = kPlayers;
  set.max_edicts = 64;
  Rules rules;
  create_rules(rules, arena, set.max_edicts);
  sh::Program program = rules_program(rules);
  std::unique_ptr<ModHost> mod;
  if (opt.mod) {
    mod = std::make_unique<ModHost>();
    rep.mod_errors = create_mod_host(*mod, opt.mod, program, opt.mod_seed);
    if (!rep.mod_errors.empty()) return rep;
    program = mod_program(*mod);
  }
  sh::Server sv;
  sh::create_server(sv, set, w.models, program);
  spawn_arena_entities(sv, rules);
  if (mod) mod_match_start(*mod, sv);
  for (int p = 0; p < kPlayers; p++) sh::connect_client(sv, p, false, false);

  ClientSim clients[kPlayers];
  for (int p = 0; p < kPlayers; p++) {
    clients[p].view = {0, arena.spawn_yaw[p], 0};
    clients[p].sent.reserve(2048);
  }
  std::vector<InFlight> packets;
  std::vector<Reply> replies[kPlayers];
  std::vector<sh::Packet> due;
  rep.frames.reserve(static_cast<size_t>(opt.frames));
  const std::vector<ScriptStep>& script = match_script();
  const int end_ms = opt.frames * kServerFrameMs;
  size_t events_seen = 0;

  for (int t = 0; t <= end_ms; t++) {
    // Replies arriving now.
    for (int p = 0; p < kPlayers; p++) {
      ClientSim& c = clients[p];
      ClientReport& cr = rep.clients[p];
      while (!replies[p].empty() && replies[p].front().arrive_ms <= t) {
        sh::Vec3f before{}, after{};
        int n = 0;
        const bool had = predict(w, sv.set.move, c, t, opt.latency_ms, before, n);
        c.last = replies[p].front();
        c.acked = true;
        replies[p].erase(replies[p].begin());
        cr.replies++;
        if (had && opt.predict && predict(w, sv.set.move, c, t, opt.latency_ms, after, n)) {
          sh::smoothing_frame(c.smooth, before, after);
          const float err = sh::length(before - after);
          cr.corrections.push_back({t, err});
          if (err > cr.worst_correction) cr.worst_correction = err;
        }
      }
    }

    // The clients' frames.
    for (int p = 0; p < kPlayers; p++) {
      if (t % opt.client_frame_ms != 0) continue;
      ClientSim& c = clients[p];
      ClientReport& cr = rep.clients[p];
      while (c.script_at < script.size() && script[c.script_at].t_ms <= t) {
        if (script[c.script_at].player == p) apply(c, script[c.script_at]);
        c.script_at++;
      }
      if (sh::cmd_due(c.clock, opt.client_frame_ms * 0.001)) {
        const uint8_t msec = sh::next_msec(c.clock);
        c.sent.push_back(sh::build_cmd(c.keys, c.view, msec, c.impulse));
        c.impulse = 0;
        c.sequence++;
        cr.commands++;
        sh::Packet pk;
        pk.client = p;
        pk.sequence = c.sequence;
        for (int i = 0; i < 3; i++) {
          const int seq = static_cast<int>(c.sequence) - 2 + i;
          if (seq >= 1) pk.cmds[i] = c.sent[static_cast<size_t>(seq - 1)];
        }
        cr.packets_sent++;
        if (p == 1 && opt.lose_every > 0 && c.sequence % static_cast<uint32_t>(opt.lose_every) == 0)
          cr.packets_lost++;
        else
          packets.push_back({t + opt.latency_ms, pk});
      }
      sh::smoothing_decay(c.smooth, opt.client_frame_ms * 0.001);
      ViewSample v;
      v.t_ms = t;
      if (opt.predict && predict(w, sv.set.move, c, t, opt.latency_ms, v.predicted, v.unacked)) {
        v.shown = sh::smoothed(c.smooth, v.predicted);
        cr.views.push_back(v);
      } else if (c.acked) {
        v.predicted = sh::unquantize_player(c.last.own, sv.set.move).origin;
        v.shown = v.predicted;
        cr.views.push_back(v);
      }
    }

    // The server's frame.
    if (t == 0 || t % kServerFrameMs != 0) continue;
    due.clear();
    std::stable_sort(packets.begin(), packets.end(), [](const InFlight& a, const InFlight& b) {
      if (a.arrive_ms != b.arrive_ms) return a.arrive_ms < b.arrive_ms;
      if (a.packet.client != b.packet.client) return a.packet.client < b.packet.client;
      return a.packet.sequence < b.packet.sequence;
    });
    bool owed[kPlayers] = {false, false};
    size_t k = 0;
    while (k < packets.size() && packets[k].arrive_ms <= t) {
      due.push_back(packets[k].packet);
      owed[packets[k].packet.client] = true;
      k++;
    }
    packets.erase(packets.begin(), packets.begin() + static_cast<std::ptrdiff_t>(k));
    sh::server_frame(sv, due.data(), static_cast<int>(due.size()));

    const int frame_index = static_cast<int>(rep.frames.size());
    FrameRecord fr;
    fr.time = sv.time;
    for (int p = 0; p < kPlayers; p++) {
      const sh::Edict& pl = sv.edicts[static_cast<size_t>(p) + 1];
      fr.origin[p] = pl.origin;
      fr.velocity[p] = pl.velocity;
      fr.onground[p] = (pl.flags & sh::kFlagOnGround) != 0;
      fr.health[p] = pl.health;
    }
    fr.door_z = sv.edicts[static_cast<size_t>(rules.door)].origin.z;
    fr.plat_z = sv.edicts[static_cast<size_t>(rules.plat)].origin.z;
    fr.hash = mod ? mod_hash(mod->run, sh::server_hash(sv)) : sh::server_hash(sv);
    rep.frames.push_back(fr);

    // Missiles: new ones from this frame's events, each live one's place.
    for (; events_seen < rules.events.size(); events_seen++) {
      const GameEvent& ev = rules.events[events_seen];
      if (ev.kind == EventKind::rocket_fired || ev.kind == EventKind::grenade_thrown) {
        MissileTrack mt;
        mt.ent = ev.ent;
        mt.rocket = ev.kind == EventKind::rocket_fired;
        mt.first_frame = frame_index;
        mt.points.push_back(ev.where);
        rep.missiles.push_back(mt);
      } else if (ev.kind == EventKind::explosion) {
        for (MissileTrack& mt : rep.missiles)
          if (mt.ent == ev.ent && mt.ent > 0) {
            mt.points.push_back(ev.where);
            mt.ent = -mt.ent;  // ended
          }
      }
    }
    for (MissileTrack& mt : rep.missiles) {
      if (mt.ent <= 0) continue;
      const sh::Edict& m = sv.edicts[static_cast<size_t>(mt.ent)];
      if (m.free) {
        mt.ent = -mt.ent;
        continue;
      }
      mt.points.push_back(m.origin);
    }

    // Replies, one a frame to each client whose packet was read.
    for (int p = 0; p < kPlayers; p++) {
      if (!owed[p]) continue;
      Reply r;
      r.arrive_ms = t + opt.latency_ms;
      r.ack = sv.clients[static_cast<size_t>(p)].last_sequence;
      r.own = sh::net_player_of(sv, p);
      r.other = sh::net_player_of(sv, 1 - p);
      r.other_cmd = sv.clients[static_cast<size_t>(1 - p)].last_cmd;
      r.other_msec = sh::net_msec(sv, 1 - p);
      r.door = sv.edicts[static_cast<size_t>(rules.door)].origin;
      r.plat = sv.edicts[static_cast<size_t>(rules.plat)].origin;
      replies[p].push_back(r);
    }
  }
  for (MissileTrack& mt : rep.missiles)
    if (mt.ent < 0) mt.ent = -mt.ent;
  rep.events = rules.events;
  rep.counts = sv.counts;
  rep.final_hash = rep.frames.empty() ? 0 : rep.frames.back().hash;
  if (mod) {
    rep.mod_run = mod->run;
    rep.mod_log = mod->log;
  }
  return rep;
}

}  // namespace shooters_edge::sim
