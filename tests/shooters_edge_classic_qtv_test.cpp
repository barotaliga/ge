// Tests: the server's stream port for classic relays (games/shooters_edge/net/classic_qtv.hpp).
// SHA3-512 against the standard's vectors; the request, the answer and an up message written and
// read back; then a match of two clients over the simulated network served to relays over
// loopback TCP: a relay is let in with BEGIN and sent the match, which the demo reader plays to its
// end with the signon's stuffed lines, the relay's chat and its viewers' comings and goings; a
// password asks for SHA3-512 and refuses a wrong answer; the stream limit refuses a second relay;
// a connection that is not the stream protocol is closed without a word.
//
// ge:summary Tests: the classic relay stream port: handshake, auth, the stream read back, chat
// ge:keywords test qtv stream-port relay handshake auth sha3-512 up-message qul say_game
// ge:skill net shooter testing
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "net/address.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "net/tcp_socket.hpp"
#include "progs/file.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_qtv.hpp"
#include "shooters_edge/net/classic_server.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"

namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;
namespace gn = ge::net;
namespace sv = shooters_edge::server;

namespace {

constexpr int kClients = 2;

struct Relay {
  gn::TcpStream tcp;
  std::vector<uint8_t> got;
};

struct Match {
  ge::progs::Progs progs;
  std::unique_ptr<sv::QcServer> qc;
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st, ct[kClients];
  cn::ClassicServer server;
  cn::ClassicClientEnd c[kClients];
  cn::QtvPort port;
  int64_t now = 0;
  std::vector<Relay*> relays;

  bool start(const cn::QtvConfig& qc_cfg, std::string& error) {
    if (!sv::build_test_mod(progs, error)) return false;
    sv::QcServerOptions so;
    so.loop.max_edicts = 1024;
    qc = sv::make_qc_server(progs, so, error);
    if (!qc) return false;
    sv::MapSpec map;
    map.name = "arena";
    map.models = sv::arena_models();
    map.entities = sv::kArenaEntities;
    if (!sv::load_map(*qc, map, error)) return false;
    gn::LinkConditions lan;
    gn::conditions_preset("lan", lan);
    net = gn::make_sim_network(1, lan);
    gn::open_sim_transport(st, net, gn::sim_address(0, cn::kPortServer));
    rules = sv::qc_runner_over(*qc);
    if (!cn::classic_start(server, rules, st, cn::ServerConfig{}, error)) return false;
    for (int i = 0; i < kClients; i++) {
      gn::open_sim_transport(ct[i], net,
                             gn::sim_address(static_cast<uint32_t>(i + 1), cn::kPortClient));
      cn::ClientConfig cc;
      cc.name = "p" + std::to_string(i);
      cc.qport = static_cast<uint16_t>(100 + i);
      cc.seed = i + 1;
      cn::client_start(c[i], ct[i], gn::sim_address(0, cn::kPortServer), cc, 0);
    }
    return cn::qtv_open(port, gn::loopback_v4(0), qc_cfg, error);
  }

  void tick() {
    now += 13000;
    gn::sim_advance(net, now);
    cn::classic_frame(server, now);
    for (cn::ClassicClientEnd& k : c) cn::client_tick(k, now);
    cn::qtv_update(port, server);
    for (Relay* r : relays) {
      gn::tcp_pump(r->tcp);
      gn::tcp_take_bytes(r->tcp, r->got);
    }
  }
  void run(int ticks) {
    for (int i = 0; i < ticks; i++) tick();
  }
  // Ticks until `r` has an answer block or the stream closed (at most `ticks`).
  bool answered(Relay& r, cn::QtvAnswer& a, size_t& used, int ticks = 400) {
    for (int i = 0; i < ticks; i++) {
      const std::string text(r.got.begin(), r.got.end());
      if (cn::qtv_parse_answer(text, a, used) == cn::QtvParse::ok) return true;
      tick();
    }
    return false;
  }
  bool dial(Relay& r, const std::string& request) {
    std::string error;
    if (!gn::tcp_connect(r.tcp, port.listener.local, &error)) return false;
    relays.push_back(&r);
    return gn::tcp_send_bytes(r.tcp, reinterpret_cast<const uint8_t*>(request.data()),
                              request.size());
  }
};

cn::QtvRequest request(const std::string& name) {
  cn::QtvRequest q;
  q.version = 1;
  q.receive = true;
  q.userinfo = "\\name\\" + name;
  return q;
}

bool has(const std::vector<uint8_t>& bytes, const std::string& text) {
  return std::search(bytes.begin(), bytes.end(), text.begin(), text.end()) != bytes.end();
}

std::unique_ptr<Match> make_match(const cn::QtvConfig& cfg) {
  auto m = std::make_unique<Match>();
  std::string error;
  if (!m->start(cfg, error)) {
    ge::test::skip("no match or no loopback TCP here: " + error);
    return nullptr;
  }
  return m;
}

}  // namespace

TEST(sha3_512_matches_the_standard_vectors) {
  CHECK(cn::sha3_512_hex("") ==
        "a69f73cca23a9ac5c8b567dc185a756e97c982164fe25859e0d1dcc1475c80a6"
        "15b2123af1f5f94c11e3e9402c3ac558f500199d95b6d3e301758586281dcd26");
  CHECK(cn::sha3_512_hex("abc") ==
        "b751850b1a57168a5693cd924b6b096e08f621827444f70d884f5d0240d2712e"
        "10e116e9192af3c91a7ec57647e3934057340b4cf408d5a56592f8274eec53f0");
  // Over one block (72 bytes): 200 bytes of 0xa3, the standard's long example.
  CHECK(cn::sha3_512_hex(std::string(200, static_cast<char>(0xa3))) ==
        "e76dfad22084a8b1467fcf2ffa58361bec7628edf5f3fdc0e4805dc48caeeca8"
        "1b7c13c30adf52a3659584739a2df46be589c51ca1a4a8416df6545a1ce8ba00");
}

TEST(the_handshake_text_round_trips) {
  cn::QtvRequest q = request("relay one");
  q.offers = {cn::QtvAuth::sha3_512, cn::QtvAuth::plain};
  q.has_password = true;
  q.password = "abc";
  q.raw = 1;
  const std::string text = cn::qtv_request_text(q) + "tail";
  cn::QtvRequest back;
  size_t used = 0;
  CHECK(cn::qtv_parse_request(text, back, used) == cn::QtvParse::ok);
  CHECK(used == text.size() - 4);
  CHECK(back.version == 1 && back.receive && back.userinfo == q.userinfo && back.raw == 1);
  CHECK(back.offers == q.offers && back.has_password && back.password == "abc");
  CHECK(cn::qtv_parse_request(text.substr(0, 10), back, used) == cn::QtvParse::more);
  CHECK(cn::qtv_parse_request("GET / HTTP/1.1\n\n", back, used) == cn::QtvParse::bad);

  cn::QtvAnswer a;
  a.kind = cn::QtvAnswerKind::auth;
  a.method = cn::QtvAuth::sha3_512;
  a.challenge = "1.2.3.4:5xyz";
  cn::QtvAnswer ab;
  CHECK(cn::qtv_parse_answer(cn::qtv_answer_text(a), ab, used) == cn::QtvParse::ok);
  CHECK(ab.kind == cn::QtvAnswerKind::auth && ab.method == a.method && ab.challenge == a.challenge);
  a = {};
  a.kind = cn::QtvAnswerKind::terror;
  a.text = "later";
  CHECK(cn::qtv_parse_answer(cn::qtv_answer_text(a), ab, used) == cn::QtvParse::ok);
  CHECK(ab.kind == cn::QtvAnswerKind::terror && ab.text == "later");

  std::vector<uint8_t> up;
  cn::qtv_put_up(up, "say_game hello");
  cn::qtv_put_up(up, "qul 1 7 \"eve\"");
  std::string cmd;
  CHECK(cn::qtv_next_up(up.data(), up.size(), cmd, used) == cn::QtvParse::ok);
  CHECK(cmd == "say_game hello" && used == 3 + 15);
  CHECK(cn::qtv_next_up(up.data() + used, up.size() - used, cmd, used) == cn::QtvParse::ok);
  CHECK(cmd == "qul 1 7 \"eve\"");
  CHECK(cn::qtv_next_up(up.data(), 5, cmd, used) == cn::QtvParse::more);
  up[2] = 2;  // a kind a server takes from no relay
  CHECK(cn::qtv_next_up(up.data(), up.size(), cmd, used) == cn::QtvParse::bad);
}

TEST(a_relay_is_sent_the_match_and_its_chat_reaches_the_players) {
  auto owned = make_match(cn::QtvConfig{});
  if (!owned) return;
  Match& m = *owned;
  m.run(150);  // the clients spawn
  Relay r;
  CHECK(m.dial(r, cn::qtv_request_text(request("tv one"))));
  cn::QtvAnswer a;
  size_t used = 0;
  CHECK(m.answered(r, a, used));
  CHECK(a.kind == cn::QtvAnswerKind::begin);
  CHECK(cn::qtv_streaming(m.port) == 1 && m.server.streams.size() == 1);
  m.run(100);
  std::vector<uint8_t> up;
  cn::qtv_put_up(up, "say_game \"#3:eve: hello players\"");
  cn::qtv_put_up(up, "qul 1 3 \"eve\"");
  CHECK(gn::tcp_send_bytes(r.tcp, up.data(), up.size()));
  m.run(200);
  CHECK(m.port.counts.chat == 1 && m.port.counts.qul == 1);
  CHECK(m.port.relays[0]->viewers.size() == 1 && m.port.relays[0]->viewers[0].name == "eve");
  const size_t before_close = r.got.size();
  cn::qtv_close(m.port, m.server);
  for (int i = 0; i < 2000 && !r.tcp.closed; i++) {
    gn::tcp_pump(r.tcp);
    gn::tcp_take_bytes(r.tcp, r.got);
  }
  CHECK(r.got.size() > before_close);
  CHECK(m.server.streams.empty());

  // The stream after the answer: a multiview recording the reader plays to its end.
  std::vector<uint8_t> stream(r.got.begin() + static_cast<std::ptrdiff_t>(used), r.got.end());
  CHECK(has(stream, "fullserverinfo \"") && has(stream, "skins\n"));
  // The sound list comes before the model list: an existing client playing the stream refuses the
  // entities' model indexes when it comes after.
  const std::string models_at = std::string("\x2d\x00maps/", 7);
  const auto model_list =
      std::search(stream.begin(), stream.end(), models_at.begin(), models_at.end());
  const uint8_t sounds_at[2] = {0x2e, 0x00};
  const auto sound_list = std::search(stream.begin(), stream.end(), sounds_at, sounds_at + 2);
  CHECK(model_list != stream.end() && sound_list < model_list);
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(stream, d, error), error);
  CHECK(d.state.format == cl::DemoFormat::multiview);
  int frames = 0, most_players = 0;
  bool chat = false, joined = false;
  for (;;) {
    const cl::DemoRead st = cl::next_frame(d, error);
    CHECK_MSG(st != cl::DemoRead::error, error);
    if (st != cl::DemoRead::frame) break;
    frames++;
    int players = 0;
    for (const cl::DemoEntity& p : d.state.players) players += p.present ? 1 : 0;
    most_players = std::max(most_players, players);
    for (const cl::DemoPrint& p : d.state.prints) {
      chat = chat || p.text.find("#0:qtv_say_game:#3:eve: hello players") != std::string::npos;
      joined = joined || p.text.find("QTV: tv one: eve join") != std::string::npos;
    }
  }
  CHECK(frames > 250);
  CHECK(most_players == kClients);
  CHECK(chat && joined);
}

TEST(a_password_asks_for_sha3_and_refuses_a_wrong_answer) {
  cn::QtvConfig cfg;
  cfg.password = "secret";
  auto owned = make_match(cfg);
  if (!owned) return;
  Match& m = *owned;
  m.run(20);
  for (const bool right : {false, true}) {
    Relay r;
    cn::QtvRequest q = request(right ? "right" : "wrong");
    q.offers = {cn::QtvAuth::plain, cn::QtvAuth::sha3_512};
    CHECK(m.dial(r, cn::qtv_request_text(q)));
    cn::QtvAnswer a;
    size_t used = 0;
    CHECK(m.answered(r, a, used));
    CHECK(a.kind == cn::QtvAnswerKind::auth && a.method == cn::QtvAuth::sha3_512);
    CHECK(a.challenge.size() == 63);
    r.got.erase(r.got.begin(), r.got.begin() + static_cast<std::ptrdiff_t>(used));
    q.offers = {cn::QtvAuth::sha3_512};
    q.has_password = true;
    q.password =
        cn::qtv_auth_response(cn::QtvAuth::sha3_512, a.challenge, right ? "secret" : "guess");
    const std::string again = cn::qtv_request_text(q);
    CHECK(gn::tcp_send_bytes(r.tcp, reinterpret_cast<const uint8_t*>(again.data()), again.size()));
    CHECK(m.answered(r, a, used));
    if (right) {
      CHECK(a.kind == cn::QtvAnswerKind::begin);
    } else {
      CHECK(a.kind == cn::QtvAnswerKind::perror && a.text == "Bad password.");
    }
    m.relays.clear();
    m.run(5);
    gn::tcp_close(r.tcp);
  }
  CHECK(cn::qtv_streaming(m.port) == 1);
  cn::qtv_close(m.port, m.server);
}

TEST(the_stream_limit_refuses_a_second_relay_and_strangers_are_closed) {
  auto owned = make_match(cn::QtvConfig{});
  if (!owned) return;
  Match& m = *owned;
  m.run(20);
  Relay one, two, stranger;
  CHECK(m.dial(one, cn::qtv_request_text(request("first"))));
  cn::QtvAnswer a;
  size_t used = 0;
  CHECK(m.answered(one, a, used) && a.kind == cn::QtvAnswerKind::begin);
  CHECK(m.dial(two, cn::qtv_request_text(request("second"))));
  CHECK(m.answered(two, a, used) && a.kind == cn::QtvAnswerKind::error);
  CHECK(a.text.find("limit on the number of proxies") != std::string::npos);
  CHECK(m.dial(stranger, "GET / HTTP/1.1\r\n\r\n"));
  for (int i = 0; i < 400 && !stranger.tcp.closed; i++) m.tick();
  CHECK(stranger.tcp.closed && stranger.got.empty());
  CHECK(cn::qtv_streaming(m.port) == 1 && m.port.counts.refused == 2);
  m.relays.clear();
  cn::qtv_close(m.port, m.server);
  for (Relay* r : {&one, &two, &stranger}) gn::tcp_close(r->tcp);
}
