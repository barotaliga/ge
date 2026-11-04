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
  con.add_command("netstatus", "netstatus: the server joined, its stage and the traffic",
                  [&r](Con& c, const Args&) {
                    c.print(remote_status(r));
                    if (!r.active) return;
                    c.print("  in " + std::to_string(r.end.datagrams_in) + " packets, " +
                            std::to_string(r.end.bytes_in) + " bytes; out " +
                            std::to_string(r.end.bytes_out) + " bytes; " +
                            std::to_string(r.end.ch.dropped) + " dropped");
                  });
}

}  // namespace shooters_edge::net
