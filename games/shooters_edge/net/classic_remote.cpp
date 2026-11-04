// The window joined to a server (classic_remote.hpp).
//
// ge:summary The window joined to a server: the classic client driven by the hands, drawn as a demo
// ge:keywords remote_connect remote_step remote_disconnect remote_status
// ge:skill net shooter
#include "shooters_edge/net/classic_remote.hpp"

#include <cstdlib>
#include <functional>

#include "shooters_edge/net/classic_wire.hpp"

namespace shooters_edge::net {

namespace {

// The live recording's reader: a client recording with nothing read yet, as open_demo would set
// one up, its command records known to be the 24-byte struct this client writes.
void open_live(client::Demo& d) {
  d = client::Demo{};
  d.state.format = client::DemoFormat::client;
  d.state.protocol = 28;
  d.state.view_height = 22;
  d.state.players.resize(32);
  d.ring.resize(64);
  d.cmd_size = 24;
}

void start(Remote& r, const ClientConfig& cfg) {
  ClientConfig c = cfg;
  c.brain = [&r](int) {
    sh::MoveCmd m = r.hands;
    m.angles = r.view;
    r.hands.impulse = 0;  // an impulse goes once
    return m;
  };
  client_start(r.end, *r.transport, r.server, c, 0);
  r.recording = ClientRecorder{};
  record_start(r.recording, false);
  r.end.record = &r.recording;
  open_live(r.demo);
  r.active = true;
  r.now_us = 0;
  r.owed_s = 0;
  r.levels = 0;
  r.setangles = 0;
  r.said = ClientStage::challenge;
  r.why.clear();
  r.world = nullptr;
  r.smooth = sh::Smoothing{};
  r.predicted = false;
  r.replayed = 0;
}

sh::MoveSettings settings_of(const ClassicClientEnd& c) {
  sh::MoveSettings m;
  const float* v = c.movevars;
  m.gravity = v[0];
  m.stopspeed = v[1];
  m.maxspeed = v[2];
  m.spectatormaxspeed = v[3];
  m.accelerate = v[4];
  m.airaccelerate = v[5];
  m.wateraccelerate = v[6];
  m.friction = v[7];
  m.waterfriction = v[8];
  return m;
}

sh::MoveType move_type_of(const PlayerState& p) {
  switch (p.pm) {
    case PmType::spectator:
      return sh::MoveType::spectator;
    case PmType::old_spectator:
      return sh::MoveType::old_spectator;
    case PmType::fly:
      return sh::MoveType::fly;
    case PmType::none:
      return sh::MoveType::none;
    case PmType::lock:
      return sh::MoveType::lock;
    default:
      return p.dead ? sh::MoveType::dead : sh::MoveType::normal;
  }
}

// The player's place `acked`'s reply put him (`own`), with every move sent since run through the
// move. False when there is nothing to predict from.
bool predict_from(const Remote& r, const PlayerState& own, uint32_t acked, sh::Vec3f& out,
                  int& replayed) {
  const ClassicClientEnd& c = r.end;
  if (r.nopred || r.world == nullptr || r.world->empty() || c.stage != ClientStage::active ||
      c.playernum < 0 || !c.player_seen[c.playernum] || acked == 0)
    return false;
  sh::NetPlayer n;
  const float at[3] = {own.origin.x, own.origin.y, own.origin.z};
  const float speed[3] = {own.velocity.x, own.velocity.y, own.velocity.z};
  for (int i = 0; i < 3; i++) {
    n.origin8[i] = static_cast<int32_t>(at[i] * 8);
    n.velocity[i] = static_cast<int16_t>(speed[i]);
  }
  n.type = move_type_of(own);
  n.onground = own.onground;
  n.jump_held = own.pm == PmType::normal_jump_held;
  sh::MoveCmd cmds[64];
  int count = 0;
  for (uint32_t q = acked + 1; q < c.ch.outgoing_sequence && count < 64; q++)
    if (c.sent_seq[q & 63] == q) cmds[count++] = c.sent[q & 63];
  // The solids where the last frame put them: the movers, then the other players.
  const std::vector<sh::BrushModel>& models = *r.world;
  const client::DemoState& st = r.demo.state;
  std::vector<sh::ClientModel> movers;
  for (const client::DemoEntity& e : st.entities) {
    if (!e.present || e.model <= 0 || static_cast<size_t>(e.model) >= st.models.size()) continue;
    const std::string& name = st.models[static_cast<size_t>(e.model)];
    if (name.size() < 2 || name[0] != '*') continue;
    const int k = std::atoi(name.c_str() + 1);
    if (k > 0 && static_cast<size_t>(k) < models.size())
      movers.push_back({&models[static_cast<size_t>(k)], e.origin});
  }
  std::vector<sh::Vec3f> others;
  for (size_t i = 0; i < st.players.size(); i++)
    if (static_cast<int>(i) != c.playernum && st.players[i].present && !st.players[i].dead)
      others.push_back(st.players[i].origin);
  sh::MoveWorld world;
  sh::client_world(models[0], movers.data(), static_cast<int>(movers.size()), others.data(),
                   static_cast<int>(others.size()), world);
  out = sh::predict_player(n, cmds, count, settings_of(c), world).origin;
  replayed = count;
  return true;
}

const char* stage_words(ClientStage s) {
  switch (s) {
    case ClientStage::challenge:
    case ClientStage::connect:
      return "connecting to ";
    case ClientStage::signon:
      return "signing on to ";
    case ClientStage::active:
      return "playing on ";
    case ClientStage::dropped:
      return "dropped by ";
  }
  return "";
}

}  // namespace

bool remote_connect(Remote& r, const std::string& address, const ClientConfig& cfg,
                    std::string& error) {
  remote_disconnect(r);
  std::string text = address;
  if (text.rfind("localhost", 0) == 0) text = "127.0.0.1" + text.substr(9);
  if (text.find(':') == std::string::npos) text += ":" + std::to_string(kPortServer);
  gn::Address to;
  if (!gn::parse_address(text, to)) {
    error = "'" + address +
            "' is not an address the client reads; next: give an IPv4 address with an optional "
            "port (connect 192.168.1.20, connect 192.168.1.20:27500) or localhost";
    return false;
  }
  if (!gn::open_udp_transport(r.own, gn::ipv4(0, 0, 0, 0, 0), &error)) {
    error = "the client's socket did not open: " + error +
            "; next: check that the machine has a network";
    return false;
  }
  r.transport = &r.own;
  r.server = to;
  r.server_name = text;
  start(r, cfg);
  return true;
}

void remote_start(Remote& r, gn::Transport& t, const gn::Address& server, const ClientConfig& cfg) {
  remote_disconnect(r);
  r.transport = &t;
  r.server = server;
  r.server_name = gn::to_string(server);
  start(r, cfg);
}

void remote_world(Remote& r, const std::vector<sh::BrushModel>* models) {
  r.world = models;
  r.smooth = sh::Smoothing{};
}

void remote_disconnect(Remote& r) {
  if (!r.active) return;
  if (r.end.stage == ClientStage::signon || r.end.stage == ClientStage::active) {
    client_command(r.end, "drop");
    for (int i = 0; i < 3; i++) {  // the reliable drop, sent until it is surely out
      r.now_us += kTickUs;
      client_tick(r.end, r.now_us);
    }
  }
  r.end.record = nullptr;
  if (r.transport == &r.own) gn::close_transport(r.own);
  r.transport = nullptr;
  r.active = false;
}

RemoteStep remote_step(Remote& r, double wall_s, std::vector<std::string>& said) {
  if (!r.active) return RemoteStep::waiting;
  r.owed_s += wall_s;
  // What the player's place was before this frame's packets, for the correction they make.
  const int me = r.end.playernum;
  const PlayerState own_before = me >= 0 ? r.end.players[me] : PlayerState{};
  const uint32_t acked_before = r.end.ch.incoming_acknowledged;
  int ticks = 0;
  while (r.owed_s >= kTickUs * 1e-6) {
    r.owed_s -= kTickUs * 1e-6;
    if (ticks++ >= kMostTicks) continue;  // a stall: the time is dropped, not paid down
    r.now_us += kTickUs;
    client_tick(r.end, r.now_us);
  }
  if (r.end.stage != r.said) {
    r.said = r.end.stage;
    std::string line = stage_words(r.said) + r.server_name;
    if (r.said == ClientStage::dropped && !r.end.refused.empty()) line += ": " + r.end.refused;
    said.push_back(line);
  }
  if (r.end.stage == ClientStage::dropped) {
    r.why = r.end.refused.empty() ? "the connection was lost" : r.end.refused;
    return RemoteStep::dropped;
  }
  if (r.end.setangles != r.setangles) {  // the server turned the player
    r.setangles = r.end.setangles;
    r.view = {r.end.set_angles[0], r.end.set_angles[1], 0};
  }
  // The recording read up to its newest packet, and what was read dropped.
  client::Demo& d = r.demo;
  d.bytes.insert(d.bytes.end(), r.recording.bytes.begin(), r.recording.bytes.end());
  r.recording.bytes.clear();
  bool any = false;
  std::string error;
  // Every frame read's own news, kept together (next_frame empties it for each frame).
  std::vector<client::DemoSound> started;
  std::vector<std::pair<int, int>> stopped;
  std::vector<client::DemoVoice> voice;
  std::vector<client::DemoPrint> prints;
  std::string centre;
  for (;;) {
    const client::DemoRead got = client::next_frame(d, error);
    if (got == client::DemoRead::error) {
      r.why = "the server's message did not read: " + error;
      said.push_back("dropped: " + r.why);
      remote_disconnect(r);
      return RemoteStep::dropped;
    }
    if (got != client::DemoRead::frame) break;
    any = true;
    client::DemoState& st = d.state;
    started.insert(started.end(), st.started.begin(), st.started.end());
    stopped.insert(stopped.end(), st.stopped.begin(), st.stopped.end());
    voice.insert(voice.end(), st.voice.begin(), st.voice.end());
    prints.insert(prints.end(), st.prints.begin(), st.prints.end());
    if (!st.centre.empty()) centre = st.centre;
  }
  d.state.started = std::move(started);
  d.state.stopped = std::move(stopped);
  d.state.voice = std::move(voice);
  d.state.prints = std::move(prints);
  d.state.centre = std::move(centre);
  if (d.ended) {
    r.why = "the server disconnected";
    said.push_back(r.why);
    remote_disconnect(r);
    return RemoteStep::dropped;
  }
  d.bytes.erase(d.bytes.begin(), d.bytes.begin() + static_cast<std::ptrdiff_t>(d.pos));
  d.pos = 0;
  d.state.angles = r.view;  // the drawn view is the hands', never a tick behind
  // The player's own place: predicted, the last correction decaying over it.
  sh::Vec3f now;
  r.predicted = me >= 0 && me == r.end.playernum &&
                predict_from(r, r.end.players[me], r.end.ch.incoming_acknowledged, now, r.replayed);
  if (r.predicted) {
    sh::Vec3f before;
    int unused = 0;
    if (r.end.ch.incoming_acknowledged != acked_before &&
        predict_from(r, own_before, acked_before, before, unused))
      sh::smoothing_frame(r.smooth, before, now);
    sh::smoothing_decay(r.smooth, wall_s);
    r.prediction = now;
    const sh::Vec3f drawn = sh::smoothed(r.smooth, now);
    if (static_cast<size_t>(me) < d.state.players.size())
      d.state.players[static_cast<size_t>(me)].origin = drawn;
    if (!d.state.intermission) d.state.eye = {drawn.x, drawn.y, drawn.z + d.state.view_height};
  }
  if (!any || d.state.map.empty()) return RemoteStep::waiting;
  if (d.state.levels != r.levels) {
    r.levels = d.state.levels;
    return RemoteStep::new_level;
  }
  return RemoteStep::frame;
}

std::string remote_status(const Remote& r) {
  if (!r.active) return "not connected";
  return stage_words(r.end.stage) + r.server_name;
}

void remote_hands(Remote& r, const client::Moves& m, const ge::input::InputState& in,
                  const client::ControlSettings& c, double dt) {
  client::turn_view(in, m, c, dt, r.view);
  sh::MoveCmd cmd = client::read_controls(m, in, c);
  if (cmd.impulse == 0) cmd.impulse = r.hands.impulse;
  r.hands = cmd;
}

void add_remote_commands(ge::console::Console& con, Remote& r) {
  using Con = ge::console::Console;
  using Args = ge::console::Args;
  const auto join = [&r](Con& c, const std::string& address) {
    ClientConfig cfg;
    const auto cvar = [&c](const char* name, const std::string& fallback) {
      const ge::console::Cvar* v = c.cvars().find(name);
      return v != nullptr && !v->value.empty() ? v->value : fallback;
    };
    cfg.name = cvar("name", "player");
    cfg.topcolor = std::atoi(cvar("topcolor", "0").c_str());
    cfg.bottomcolor = std::atoi(cvar("bottomcolor", "0").c_str());
    cfg.qport = static_cast<uint16_t>(1 + std::hash<std::string>{}(cfg.name) % 65000);
    std::string error;
    if (!remote_connect(r, address, cfg, error))
      c.print("connect: " + error);
    else
      c.print(remote_status(r));
  };
  con.add_command("connect", "connect <host>[:port]: join a server (27500 when no port is given)",
                  [join](Con& c, const Args& a) {
                    if (a.size() < 2)
                      c.print("connect: name a server (connect 192.168.1.20)");
                    else
                      join(c, a[1]);
                  });
  con.add_command("reconnect", "reconnect: join the last server again",
                  [&r, join](Con& c, const Args&) {
                    if (r.server_name.empty())
                      c.print("reconnect: no server joined yet; next: connect <host>");
                    else
                      join(c, r.server_name);
                  });
  con.add_command("disconnect", "disconnect: leave the server, back to your own game",
                  [&r](Con& c, const Args&) {
                    if (!r.active) {
                      c.print("disconnect: not connected");
                      return;
                    }
                    remote_disconnect(r);
                    c.print("disconnected from " + r.server_name);
                  });
  con.cvars().bind({.name = "cl_nopred",
                    .fallback = "0",
                    .help = "1: on a server joined, draw the player where the server last put him "
                            "(a round trip late); 0: his own moves predicted",
                    .kind = ge::console::CvarKind::boolean},
                   &r.nopred);
  con.add_command(
      "netstatus", "netstatus: the server joined, its stage and the traffic",
      [&r](Con& c, const Args&) {
        c.print(remote_status(r));
        if (!r.active) return;
        c.print("  in " + std::to_string(r.end.datagrams_in) + " packets, " +
                std::to_string(r.end.bytes_in) + " bytes; out " + std::to_string(r.end.bytes_out) +
                " bytes; " + std::to_string(r.end.ch.dropped) + " dropped");
        if (r.predicted)
          c.print("  predicting " + std::to_string(r.replayed) + " moves ahead of the server, " +
                  std::to_string(static_cast<int>(sh::length(r.smooth.correction))) +
                  " units of correction left");
        else
          c.print(r.nopred ? "  not predicting (cl_nopred 1)"
                           : "  not predicting yet (no level loaded or not playing)");
      });
}

}  // namespace shooters_edge::net
