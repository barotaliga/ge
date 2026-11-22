// games/shooters_edge/net/q1_remote.hpp: the window joined to a Quake server of protocol 15 on
// the simulated network. Against a hand-driven server written from the transport's facts: the
// connect request's bytes, the accept's port taken, each `signonnum` answered with Quake's own
// commands (the player's name and colours at stage 2), a move a tick once the game is drawn with
// the view's angles, the speeds, the buttons and an impulse sent once, a line for the server as a
// string command, a reject said with its reason, silence said as "No Response", and the goodbye as
// one disconnect byte. Against the netgame face (net/q1_tv_netgame.hpp): signed on and drawn.
//
// ge:summary Test: the window joined to a Quake server of protocol 15: connect, signon, moves
// ge:keywords test netquake protocol-15 remote-client connect signon move stringcmd disconnect
// ge:skill net shooter testing
#include <cstring>
#include <string>
#include <vector>

#include "check.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/net/q1_remote.hpp"
#include "shooters_edge/net/q1_tv_netgame.hpp"

namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;
namespace gn = ge::net;

namespace {

constexpr uint16_t kClientPort = 27001;
constexpr uint32_t kData = 0x00010000u, kAck = 0x00020000u, kEom = 0x00080000u,
                   kUnreliable = 0x00100000u, kControl = 0x80000000u;

void be32(std::vector<uint8_t>& o, uint32_t v) {
  for (int s = 24; s >= 0; s -= 8) o.push_back(static_cast<uint8_t>(v >> s));
}
uint32_t rd32(const uint8_t* p) {
  return uint32_t{p[0]} << 24 | uint32_t{p[1]} << 16 | uint32_t{p[2]} << 8 | p[3];
}

// A server driven by hand: what the client sent, and messages sent to it.
struct Hand {
  gn::SimNetwork net;
  gn::Transport server_t, client_t;
  cn::Remote r;
  std::vector<std::string> said;
  int64_t now = 0;
  uint32_t out_seq = 0, out_unrel = 0;
  gn::Address client;
  // Everything the client sent since the last look: control packets, reliable string commands
  // (acknowledged), unreliable messages.
  std::vector<std::vector<uint8_t>> controls, unreliable;
  std::vector<std::string> commands;
  Hand() {
    net = gn::make_sim_network(2, gn::LinkConditions{});
    gn::open_sim_transport(server_t, net, gn::sim_address(0, cn::kPortNetQuake));
    gn::open_sim_transport(client_t, net, gn::sim_address(1, kClientPort));
    cn::ClientConfig cfg;
    cfg.name = "tester";
    cfg.topcolor = 3;
    cfg.bottomcolor = 4;
    cn::remote_start(r, client_t, gn::sim_address(0, cn::kPortNetQuake), cfg);
  }
  void send(const std::vector<uint8_t>& d) {
    gn::transport_send(server_t, client, d.data(), d.size(), now);
  }
  void reliable(const std::vector<uint8_t>& body) {
    std::vector<uint8_t> d;
    be32(d, kData | kEom | static_cast<uint32_t>(8 + body.size()));
    be32(d, out_seq++);
    d.insert(d.end(), body.begin(), body.end());
    send(d);
  }
  void unrel(const std::vector<uint8_t>& body) {
    std::vector<uint8_t> d;
    be32(d, kUnreliable | static_cast<uint32_t>(8 + body.size()));
    be32(d, out_unrel++);
    d.insert(d.end(), body.begin(), body.end());
    send(d);
  }
  void read() {
    uint8_t b[2048];
    gn::Address from;
    for (;;) {
      const int n = gn::transport_receive(server_t, b, sizeof b, from, now);
      if (n < 0) break;
      if (n < 5) continue;
      client = from;
      const uint32_t w = rd32(b);
      if ((w & kControl) != 0) {
        controls.emplace_back(b, b + n);
        continue;
      }
      const uint32_t seq = rd32(b + 4);
      if ((w & kData) != 0) {
        std::vector<uint8_t> ack;
        be32(ack, kAck | 8u);
        be32(ack, seq);
        send(ack);
        for (int i = 8; i < n;) {  // the string commands in it
          if (b[i] != 4) break;
          const std::string s(reinterpret_cast<const char*>(b + i + 1));
          commands.push_back(s);
          i += static_cast<int>(s.size()) + 2;
        }
      } else if ((w & kUnreliable) != 0) {
        unreliable.emplace_back(b + 8, b + n);
      }
    }
  }
  cn::RemoteStep step(int ticks = 1) {
    cn::RemoteStep last = cn::RemoteStep::waiting;
    for (int i = 0; i < ticks; i++) {
      now += cn::kTickUs;
      gn::sim_advance(net, now);
      if (r.active) last = cn::remote_step(r, cn::kTickUs * 1e-6, said);
      gn::sim_advance(net, now);
      read();
    }
    return last;
  }
  void accept() {
    step(2);
    std::vector<uint8_t> a;
    be32(a, kControl | 9u);
    a.push_back(0x81);
    for (int k = 0; k < 4; k++)
      a.push_back(static_cast<uint8_t>((cn::kPortNetQuake >> (8 * k)) & 255));
    send(a);
    step(2);
  }
};

bool has(const std::vector<std::string>& v, const std::string& s) {
  for (const std::string& x : v)
    if (x == s) return true;
  return false;
}

}  // namespace

TEST(the_client_signs_on_as_quakes_own_with_its_name_and_colours) {
  Hand h;
  h.step(2);
  CHECK(!h.controls.empty());
  if (!h.controls.empty()) {
    const std::vector<uint8_t>& c = h.controls[0];
    CHECK(c.size() == 12 && c[4] == 0x01 && std::memcmp(c.data() + 5, "QUAKE", 6) == 0 &&
          c[11] == 3);
  }
  h.accept();
  CHECK(h.r.nq != nullptr && h.r.nq->stage == cn::ClientStage::signon);
  h.reliable({25, 1});
  h.step(3);
  CHECK(has(h.commands, "prespawn"));
  h.reliable({25, 2});
  h.step(3);
  CHECK(has(h.commands, "name \"tester\"\n"));
  CHECK(has(h.commands, "color 3 4\n"));
  CHECK(has(h.commands, "spawn "));
  h.reliable({25, 3});
  h.step(3);
  CHECK(has(h.commands, "begin"));
  CHECK(h.r.nq != nullptr && h.r.nq->stage == cn::ClientStage::active);
  // A line for the server: the word, a space, the rest; a word alone ends in a newline.
  cn::remote_command(h.r, "say hello there");
  cn::remote_command(h.r, "kill");
  h.step(3);
  CHECK(has(h.commands, "say hello there"));
  CHECK(has(h.commands, "kill \n"));
  // A word the server does not take stays with the window.
  CHECK(!cn::remote_rules_word(h.r, "bogusword 1"));
  CHECK(cn::remote_rules_word(h.r, "tell bob hi"));
}

TEST(a_reject_is_said_with_its_reason_and_silence_as_no_response) {
  {
    Hand h;
    h.step(2);
    std::vector<uint8_t> d;
    const char why[] = "Server is full.\n";
    be32(d, kControl | static_cast<uint32_t>(5 + sizeof why));
    d.push_back(0x82);
    d.insert(d.end(), why, why + sizeof why);
    h.send(d);
    CHECK(h.step(2) == cn::RemoteStep::dropped);
    CHECK_MSG(h.r.why == "Server is full.", h.r.why);
  }
  {
    Hand h;
    h.step(static_cast<int>(8.0 / (cn::kTickUs * 1e-6)));
    CHECK(h.controls.size() == static_cast<size_t>(cn::kNqTries));
    CHECK(!h.r.active);
    CHECK_MSG(h.r.why == "No Response", h.r.why);
  }
}

TEST(the_game_is_joined_from_the_netgame_face_and_moves_go_out) {
  gn::SimNetwork net = gn::make_sim_network(2, gn::LinkConditions{});
  gn::Transport face_t, client_t;
  gn::open_sim_transport(face_t, net, gn::sim_address(0, cn::kPortNetQuake));
  gn::open_sim_transport(client_t, net, gn::sim_address(1, kClientPort));
  cn::NetgameTv tv;
  std::string error;
  CHECK_MSG(cn::udp_tv_netgame_open(
                tv, cl::write_single_demo(cl::loop_script("halls", {128, 192, 24}, 64, 400)), true,
                error),
            error);
  cn::udp_tv_netgame_start(tv, face_t, {});
  cn::Remote r;
  cn::ClientConfig cfg;
  cfg.name = "joiner";
  cn::remote_start(r, client_t, gn::sim_address(0, cn::kPortNetQuake), cfg);
  std::vector<std::string> said;
  int64_t now = 0;
  int new_levels = 0, frames = 0;
  while (now < 3000000) {
    now += cn::kTickUs;
    gn::sim_advance(net, now);
    cn::udp_tv_netgame_frame(tv, now);
    const cn::RemoteStep s = cn::remote_step(r, cn::kTickUs * 1e-6, said);
    new_levels += s == cn::RemoteStep::new_level ? 1 : 0;
    frames += s == cn::RemoteStep::frame ? 1 : 0;
  }
  CHECK(r.active && r.nq != nullptr);
  if (r.nq == nullptr) return;
  CHECK(r.nq->stage == cn::ClientStage::active);
  CHECK(new_levels == 1);
  CHECK(r.demo.state.map == "halls");
  CHECK_MSG(frames > 50, std::to_string(frames));
  CHECK(r.nq->moves > 50);
  CHECK(cn::udp_tv_netgame_viewers(tv) == 1);
  CHECK(has(said, "playing on " + r.server_name));
  cn::remote_disconnect(r);
  CHECK(!r.active);
}

TEST(a_move_carries_the_view_speeds_buttons_and_one_impulse_then_the_goodbye_is_one_byte) {
  Hand h;
  h.accept();
  for (const uint8_t n : {uint8_t{1}, uint8_t{2}, uint8_t{3}}) {
    h.reliable({25, n});
    h.step(3);
  }
  // The first moves wait for an entity update; then the first two are kept back.
  h.unreliable.clear();
  h.r.view = {10, 90, 0};
  h.r.hands.forward = 200;
  h.r.hands.side = -100;
  h.r.hands.buttons =
      static_cast<uint8_t>(1 | 2 | 8);  // attack, jump and a bit the protocol does not carry
  h.r.hands.impulse = 7;
  // A time, then an entity update with nothing but its number (the update's bit and entity 1).
  std::vector<uint8_t> frame = {7};
  const float t = 2.5f;
  uint8_t tb[4];
  std::memcpy(tb, &t, 4);
  frame.insert(frame.end(), tb, tb + 4);
  frame.push_back(0x80);
  frame.push_back(1);
  h.unrel(frame);
  h.step(1);
  h.step(4);
  CHECK(!h.unreliable.empty());
  bool impulse_once = false;
  int with_impulse = 0;
  for (const std::vector<uint8_t>& m : h.unreliable) {
    if (m.size() != 16 || m[0] != 3) continue;
    float at = 0;
    std::memcpy(&at, m.data() + 1, 4);
    CHECK(at == 2.5f);
    CHECK(m[5] == 7 && m[6] == 64 && m[7] == 0);  // 10, 90 and 0 degrees as bytes
    CHECK((m[8] | m[9] << 8) == 200);
    CHECK(static_cast<int16_t>(m[10] | m[11] << 8) == -100);
    CHECK(m[14] == 3);
    with_impulse += m[15] == 7 ? 1 : 0;
    impulse_once = true;
  }
  CHECK(impulse_once);
  CHECK(with_impulse <= 1);  // the impulse goes in one move (or is lost in one kept back)
  h.unreliable.clear();
  cn::remote_disconnect(h.r);
  h.step(1);
  CHECK(h.unreliable.size() == 1u);
  if (h.unreliable.size() == 1u) CHECK(h.unreliable[0] == std::vector<uint8_t>{2});
}
