// The window joined to a server of the single-player protocol (q1_remote.hpp): the connect and its
// tries, the reliable fragments joined and acknowledged, the signon answered, moves from the hands,
// string commands, the goodbye.
//
// ge:summary The window joined to a Quake server of protocol 15: transport, signon, moves, words
// ge:keywords netquake protocol-15 remote-client signonnum clc-move stringcmd disconnect-byte
// ge:skill net shooter
// ge:test tests/shooters_edge_q1_remote_test.cpp
#include "shooters_edge/net/q1_remote.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "core/num.hpp"

namespace shooters_edge::net {

namespace {

namespace cl = shooters_edge::client;

// The header's flags and the message ids (porting/shooters-edge/netgame-transport.md).
constexpr uint32_t kData = 0x00010000u, kAck = 0x00020000u, kEom = 0x00080000u,
                   kUnreliable = 0x00100000u, kControl = 0x80000000u;
constexpr uint8_t kSvcNop = 1, kSvcStuffText = 9, kSvcSetAngle = 10, kSvcSignonNum = 25;
constexpr uint8_t kCcConnect = 0x01, kCcAccept = 0x81, kCcReject = 0x82;
constexpr uint8_t kClcDisconnect = 2, kClcMove = 3, kClcStringCmd = 4;
constexpr int kMovesHeldBack = 2;  // the first moves of a level are built, not sent

void be32(std::vector<uint8_t>& o, uint32_t v) {
  for (int s = 24; s >= 0; s -= 8) o.push_back(static_cast<uint8_t>(v >> s));
}
uint32_t rd32(const uint8_t* p) {
  return uint32_t{p[0]} << 24 | uint32_t{p[1]} << 16 | uint32_t{p[2]} << 8 | p[3];
}
void le16(std::vector<uint8_t>& o, int v) {
  o.push_back(static_cast<uint8_t>(v & 255));
  o.push_back(static_cast<uint8_t>((v >> 8) & 255));
}

void send_raw(NqClientEnd& c, const std::vector<uint8_t>& d, int64_t now) {
  gn::transport_send(*c.t, c.server, d.data(), d.size(), now);
}

void send_unreliable(NqClientEnd& c, const std::vector<uint8_t>& body, int64_t now) {
  std::vector<uint8_t> d;
  be32(d, kUnreliable | static_cast<uint32_t>(8 + body.size()));
  be32(d, c.unrel_out++);
  d.insert(d.end(), body.begin(), body.end());
  send_raw(c, d, now);
}

void connect_request(NqClientEnd& c, int64_t now) {
  std::vector<uint8_t> d;
  const char game[] = {'Q', 'U', 'A', 'K', 'E', 0};  // the game's name, a wire constant
  be32(d, kControl | static_cast<uint32_t>(4 + 1 + sizeof(game) + 1));
  d.push_back(kCcConnect);
  d.insert(d.end(), game, game + sizeof(game));
  d.push_back(3);  // the net version
  send_raw(c, d, now);
  c.tries++;
  c.tried_us = now;
}

// The reliable string commands waiting, as one message; one in flight at a time, resent after a
// second unanswered.
void flush(NqClientEnd& c, int64_t now) {
  if (c.pending && now - c.flight_us >= kNqResendUs) {
    send_raw(c, c.in_flight, now);
    c.flight_us = now;
  }
  if (c.pending || c.outbox.empty()) return;
  std::vector<uint8_t> body;
  for (const std::string& s : c.outbox) {
    body.push_back(kClcStringCmd);
    body.insert(body.end(), s.begin(), s.end());
    body.push_back(0);
  }
  c.outbox.clear();
  std::vector<uint8_t> d;
  be32(d, kData | kEom | static_cast<uint32_t>(8 + body.size()));
  be32(d, c.send_seq);
  d.insert(d.end(), body.begin(), body.end());
  send_raw(c, d, now);
  c.in_flight = std::move(d);
  c.flight_us = now;
  c.pending = true;
}

// The view's angle as the protocol's one byte (degrees × 256 / 360, whole, the low 8 bits).
uint8_t angle_byte(float degrees) {
  return static_cast<uint8_t>(ge::core::num::to_int32(degrees * 256.0f / 360.0f) & 255);
}

void send_move(Remote& r, NqClientEnd& c, int64_t now) {
  const sh::MoveCmd& h = r.hands;
  std::vector<uint8_t> b;
  b.push_back(kClcMove);
  uint8_t t[4];
  std::memcpy(t, &c.server_time, 4);
  b.insert(b.end(), t, t + 4);
  b.push_back(angle_byte(r.view.x));
  b.push_back(angle_byte(r.view.y));
  b.push_back(angle_byte(r.view.z));
  le16(b, h.forward);
  le16(b, h.side);
  le16(b, h.up);
  b.push_back(static_cast<uint8_t>(h.buttons & (sh::kButtonAttack | sh::kButtonJump)));
  b.push_back(h.impulse);
  r.hands.impulse = 0;  // an impulse goes in one move
  if (c.held_back < kMovesHeldBack) {
    c.held_back++;
    return;
  }
  send_unreliable(c, b, now);
  c.moves++;
}

// One whole message read into the drawn recording; the signon answered from it.
void message(Remote& r, NqClientEnd& c, const std::vector<uint8_t>& m, bool reliable) {
  if (m.size() == 1 && m[0] == kSvcNop) return;  // a keepalive
  cl::Demo& d = r.demo;
  cl::DemoMsg dm;
  dm.b = m;
  std::vector<uint32_t> marks;
  dm.marks = &marks;
  std::string why;
  const int levels = d.state.levels;
  if (!cl::read_single_messages(d, dm, why) && c.refused.empty())
    c.refused = "the server sent what the client cannot read" + (why.empty() ? "" : ": " + why);
  if (d.ended) {  // the server's disconnect
    c.stage = ClientStage::dropped;
    if (c.refused.empty()) c.refused = "the server ended the game";
    return;
  }
  if (d.state.levels != levels) {  // a new level: its moves start again
    c.updates = false;
    c.held_back = 0;
  }
  c.server_time = static_cast<float>(d.state.time);  // read as a float off the wire
  for (const uint32_t at : marks) {
    const uint8_t id = m[at];
    if (id == kSvcSetAngle) {
      r.view = d.state.angles;
      c.setangles++;
    }
    if ((id & 0x80) != 0) c.updates = true;  // an entity update: the game is drawn now
    if (id == kSvcStuffText) {
      const char* text = reinterpret_cast<const char*>(m.data() + at + 1);
      const size_t room = m.size() - at - 1;
      const std::string s(text, strnlen(text, room));
      if (s.rfind("reconnect", 0) == 0) {  // the level changes: the signon again
        c.signon = 0;
        c.updates = false;
        c.held_back = 0;
        c.stage = ClientStage::signon;
      }
    }
  }
  if (!reliable) {
    c.frames++;
    return;
  }
  if (m.size() >= 2 && m[m.size() - 2] == kSvcSignonNum) {
    const int n = m[m.size() - 1];
    if (n > c.signon) {
      c.signon = n;
      if (n == 1) c.outbox.emplace_back("prespawn");
      if (n == 2) {
        c.outbox.push_back("name \"" + c.name + "\"\n");
        c.outbox.push_back("color " + std::to_string(c.top) + " " + std::to_string(c.bottom) +
                           "\n");
        c.outbox.emplace_back("spawn ");
      }
      if (n == 3) {
        c.outbox.emplace_back("begin");
        c.stage = ClientStage::active;
      }
    }
  }
}

void tick(Remote& r, NqClientEnd& c, int64_t now) {
  if (c.stage == ClientStage::dropped) return;
  if (c.stage == ClientStage::challenge) {
    if (c.tried_us < 0 || now - c.tried_us >= kNqTryUs) {
      if (c.tries >= kNqTries) {
        c.stage = ClientStage::dropped;
        c.refused = "No Response";
        return;
      }
      connect_request(c, now);
    }
  }
  uint8_t buf[8192];
  gn::Address from;
  for (;;) {
    const int n = gn::transport_receive(*c.t, buf, sizeof buf, from, now);
    if (n < 0) break;
    if (n < 5) continue;
    const uint32_t word = rd32(buf);
    if ((word & kControl) != 0) {
      if (c.stage != ClientStage::challenge || from != c.server) continue;
      if (buf[4] == kCcAccept && n >= 7) {
        c.server.port = static_cast<uint16_t>(buf[5] | buf[6] << 8);
        c.stage = ClientStage::signon;
      } else if (buf[4] == kCcReject) {
        const char* text = reinterpret_cast<const char*>(buf + 5);
        std::string why(text, strnlen(text, static_cast<size_t>(n - 5)));
        while (!why.empty() && (why.back() == '\n' || why.back() == '\r')) why.pop_back();
        c.stage = ClientStage::dropped;
        c.refused = why.empty() ? "refused" : why;
        return;
      }
      continue;
    }
    if (c.stage == ClientStage::challenge || from != c.server || n < 8) continue;
    const uint32_t seq = rd32(buf + 4);
    const size_t len = std::min<size_t>(word & 0xFFFF, static_cast<size_t>(n));
    if (len < 8) continue;
    const std::vector<uint8_t> payload(buf + 8, buf + len);
    if ((word & kAck) != 0) {
      if (c.pending && seq == c.send_seq) c.pending = false, c.send_seq++;
    } else if ((word & kData) != 0) {
      std::vector<uint8_t> ack;
      be32(ack, kAck | 8u);
      be32(ack, seq);
      send_raw(c, ack, now);
      if (seq != c.recv_seq) continue;  // a fragment seen before, acknowledged again
      c.recv_seq++;
      c.partial.insert(c.partial.end(), payload.begin(), payload.end());
      if ((word & kEom) != 0) {
        const std::vector<uint8_t> msg = std::move(c.partial);
        c.partial.clear();
        message(r, c, msg, true);
      }
    } else if ((word & kUnreliable) != 0 && !payload.empty()) {
      message(r, c, payload, false);
    }
    if (c.stage == ClientStage::dropped) return;
  }
  flush(c, now);
  if (c.stage == ClientStage::active && c.signon == 3 && c.updates) send_move(r, c, now);
}

}  // namespace

bool nq_server_word(const std::string& word) {
  static const char* const kWords[] = {"status", "god",      "notarget", "fly",   "name", "noclip",
                                       "say",    "say_team", "tell",     "color", "kill", "pause",
                                       "kick",   "ping",     "give",     "ban"};
  for (const char* w : kWords)
    if (word == w) return true;
  return false;
}

void nq_command(NqClientEnd& c, const std::string& line) {
  const size_t cut = line.find_first_of(" \t");
  const std::string word = line.substr(0, cut);
  if (word.empty()) return;
  std::string rest = cut == std::string::npos ? "" : line.substr(cut + 1);
  rest.erase(0, std::min(rest.find_first_not_of(" \t"), rest.size()));
  c.outbox.push_back(word + " " + (rest.empty() ? std::string("\n") : rest));
}

void nq_stop(NqClientEnd& c, int64_t now) {
  if (c.stage != ClientStage::signon && c.stage != ClientStage::active) return;
  send_unreliable(c, {kClcDisconnect}, now);  // once: the connection is closed after it
  c.stage = ClientStage::dropped;
}

void nq_remote_start(Remote& r, const ClientConfig& cfg) {
  r.nq = std::make_unique<NqClientEnd>();
  NqClientEnd& c = *r.nq;
  c.t = r.transport;
  c.server = r.server;
  c.name = cfg.name.empty() ? "player" : cfg.name;
  c.top = std::clamp(cfg.topcolor & 15, 0, 13);
  c.bottom = std::clamp(cfg.bottomcolor & 15, 0, 13);
  r.demo = client::Demo{};  // a single-player recording's state, read as the game comes
  r.active = true;
  r.now_us = 0;
  r.owed_s = 0;
  r.levels = 0;
  r.setangles = 0;
  r.said = ClientStage::challenge;
  r.why.clear();
  r.world = nullptr;
  r.predicted = false;
  r.lerped = 0;
  r.ents_old.clear();
  r.ents_new.clear();
  r.frame_old_s = r.frame_new_s = -1;
}

RemoteStep nq_remote_step(Remote& r, double wall_s, std::vector<std::string>& said) {
  NqClientEnd& c = *r.nq;
  client::DemoState& st = r.demo.state;
  // What the last frame showed is gone: this frame's news only.
  st.prints.clear();
  st.started.clear();
  st.stopped.clear();
  st.effects.clear();
  st.voice.clear();
  st.centre.clear();
  // The player's name and colours as the window's settings now have them, told when they move.
  if (r.setting && c.stage == ClientStage::active) {
    const std::string name = r.setting("name");
    if (!name.empty() && name != c.name) {
      c.name = name;
      nq_command(c, "name \"" + name + "\"");
    }
    const std::string top_text = r.setting("topcolor"), bottom_text = r.setting("bottomcolor");
    const int top = top_text.empty() ? c.top : std::clamp(std::atoi(top_text.c_str()) & 15, 0, 13);
    const int bottom =
        bottom_text.empty() ? c.bottom : std::clamp(std::atoi(bottom_text.c_str()) & 15, 0, 13);
    if (top != c.top || bottom != c.bottom) {
      c.top = top;
      c.bottom = bottom;
      nq_command(c, "color " + std::to_string(top) + " " + std::to_string(bottom));
    }
  }
  const int64_t frames_before = c.frames;
  r.owed_s += wall_s;
  int ticks = 0;
  while (r.owed_s >= kTickUs * 1e-6) {
    r.owed_s -= kTickUs * 1e-6;
    if (ticks++ >= kMostTicks) continue;  // a stall: the time is dropped, not paid down
    r.now_us += kTickUs;
    tick(r, c, r.now_us);
  }
  if (c.stage != r.said) {
    r.said = c.stage;
    std::string line = remote_stage_words(r.said) + r.server_name;
    if (r.said == ClientStage::dropped && !c.refused.empty()) line += ": " + c.refused;
    said.push_back(line);
  }
  if (c.stage == ClientStage::dropped) {
    r.why = c.refused.empty() ? "the connection was lost" : c.refused;
    remote_disconnect(r);
    return RemoteStep::dropped;
  }
  st.angles = r.view;  // the hands' view, never a tick behind
  if (c.frames == frames_before || st.map.empty() || !c.updates) return RemoteStep::waiting;
  if (st.levels != r.levels) {
    r.levels = st.levels;
    return RemoteStep::new_level;
  }
  return RemoteStep::frame;
}

}  // namespace shooters_edge::net
