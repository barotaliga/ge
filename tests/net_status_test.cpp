// engine/net/status.hpp: a record survives its bytes, one too big for its datagram leaves out
// rows from the end and says so, an ask without a cookie is answered with a small cookie, the
// cookie's ask with the record, a wrong cookie or protocol gets no record, and the same exchange
// runs over the simulated network.
//
// ge:summary Test: the status record's bytes, truncation, the cookie, the answer
// ge:keywords test net status record cookie truncated amplification answer
// ge:skill net testing
#include "net/status.hpp"

#include <string>

#include "check.hpp"
#include "net/transport.hpp"

using namespace ge::net;

namespace {

Status sample(int players) {
  Status s;
  s.kind = NodeKind::game;
  s.protocol = 0x1234;
  s.version = 7;
  s.name = "north arena";
  s.map = "tower";
  s.mode = "2on2";
  s.phase = StatusPhase::playing;
  s.time_left_s = 312;
  s.max_players = 8;
  s.max_spectators = 4;
  s.country[0] = 's';
  s.country[1] = 'e';
  s.locked = true;
  for (int i = 0; i < players; i++) {
    StatusPlayer p;
    p.name = "player" + std::to_string(i);
    p.team = i % 2 ? "blue" : "red";
    p.score = 10 * i - 3;
    p.ping_ms = static_cast<uint16_t>(20 + i);
    p.minutes = static_cast<uint16_t>(i);
    p.flags = i == 3 ? kPlayerBot : 0;
    p.colour = 0x112233u * static_cast<uint32_t>(i);
    s.players.push_back(p);
  }
  s.teams = {{"red", 40, 0xff0000}, {"blue", 37, 0x0000ff}};
  StatusStream st;
  st.node = sim_address(9, 4400);
  st.stream = 3;
  st.delay_s = 10;
  st.viewers = 12;
  s.streams.push_back(st);
  s.keys = {{"timelimit", "20"}, {"fraglimit", "0"}};
  return s;
}

}  // namespace

TEST(status_record_round_trips) {
  const Status s = sample(4);
  uint8_t buf[kMaxDatagram];
  const size_t n = status_write(s, buf, sizeof buf);
  CHECK(n > 0);
  Status back;
  CHECK(status_read(buf, n, back));
  CHECK(back.name == "north arena" && back.map == "tower" && back.mode == "2on2");
  CHECK(back.phase == StatusPhase::playing && back.time_left_s == 312);
  CHECK(back.max_players == 8 && back.max_spectators == 4 && back.locked && !back.truncated);
  CHECK(std::string(back.country) == "se");
  CHECK(back.players.size() == 4 && back.players[2].name == "player2");
  CHECK(back.players[0].score == -3 && back.players[3].flags == kPlayerBot);
  CHECK(back.players[1].team == "blue" && back.players[1].ping_ms == 21);
  CHECK(back.teams.size() == 2 && back.teams[1].score == 37);
  CHECK(back.streams.size() == 1 && back.streams[0].node == sim_address(9, 4400));
  CHECK(back.streams[0].viewers == 12 && back.streams[0].delay_s == 10);
  CHECK(back.keys.size() == 2 && back.keys[0].second == "20");
  CHECK(!status_read(buf, n - 1, back));  // a cut record is refused
}

TEST(status_too_big_leaves_out_from_the_end) {
  const Status s = sample(40);
  uint8_t buf[400];
  const size_t n = status_write(s, buf, sizeof buf);
  CHECK(n > 0 && n <= sizeof buf);
  Status back;
  CHECK(status_read(buf, n, back));
  CHECK(back.truncated);
  CHECK(!back.players.empty() && back.players.size() < 40);
  CHECK(back.players[0].name == "player0");
  CHECK(back.teams.empty() && back.streams.empty() && back.keys.empty());
}

TEST(status_names_are_cut_at_their_limits) {
  Status s = sample(1);
  s.name = std::string(200, 'x');
  s.players[0].name = std::string(80, 'p');
  uint8_t buf[kMaxDatagram];
  const size_t n = status_write(s, buf, sizeof buf);
  Status back;
  CHECK(status_read(buf, n, back));
  CHECK(back.name.size() == kStatusName);
  CHECK(back.players[0].name.size() == kStatusPlayer);
}

TEST(status_ask_needs_a_cookie_before_the_record) {
  const Status s = sample(4);
  const Address asker = sim_address(1, 5000);
  uint8_t ask[kStatusAskSize], out[kMaxDatagram];
  CHECK(status_ask(0x1234, 77, 0, ask) == kStatusAskSize);
  CHECK(is_status_ask(ask, sizeof ask));
  size_t n = status_answer(ask, sizeof ask, asker, 0x1234, 999, 0, s, out, sizeof out);
  CHECK(n > 0 && n < kStatusAskSize);  // the cookie never outweighs the ask
  uint64_t cookie = 0;
  Status got;
  CHECK(status_read_reply(out, n, 77, cookie, got) == StatusReply::cookie);
  CHECK(cookie != 0);
  CHECK(status_read_reply(out, n, 78, cookie, got) == StatusReply::none);  // another nonce

  status_ask(0x1234, 77, cookie, ask);
  n = status_answer(ask, sizeof ask, asker, 0x1234, 999, 15000000, s, out, sizeof out);
  CHECK(status_read_reply(out, n, 77, cookie, got) == StatusReply::status);
  CHECK(got.players.size() == 4 && got.map == "tower");

  // Another address with the same cookie, or the cookie two buckets later: a cookie again.
  n = status_answer(ask, sizeof ask, sim_address(2, 5000), 0x1234, 999, 0, s, out, sizeof out);
  CHECK(status_read_reply(out, n, 77, cookie, got) == StatusReply::cookie);
  status_ask(0x1234, 77, cookie, ask);
  n = status_answer(ask, sizeof ask, asker, 0x1234, 999, 40000000, s, out, sizeof out);
  uint64_t again = 0;
  CHECK(status_read_reply(out, n, 77, again, got) == StatusReply::cookie);

  // Another game's ask, or an unpadded one: no answer at all.
  status_ask(0x9999, 77, 0, ask);
  CHECK(status_answer(ask, sizeof ask, asker, 0x1234, 999, 0, s, out, sizeof out) == 0);
  CHECK(status_answer(ask, 40, asker, 0x9999, 999, 0, s, out, sizeof out) == 0);
}

TEST(status_over_the_simulated_network) {
  SimNetwork net = make_sim_network(1, LinkConditions{});
  Transport server, client;
  CHECK(open_sim_transport(server, net, sim_address(10, 7000)));
  CHECK(open_sim_transport(client, net, sim_address(1, 7001)));
  const Status s = sample(6);
  uint64_t cookie = 0;
  Status got;
  bool have = false;
  int64_t now = 0;
  for (int round = 0; round < 4 && !have; round++) {
    uint8_t ask[kStatusAskSize];
    status_ask(s.protocol, 5, cookie, ask);
    transport_send(client, server.local, ask, sizeof ask, now);
    now += 20000;
    sim_advance(net, now);
    uint8_t buf[kMaxDatagram], out[kMaxDatagram];
    Address from;
    int n = 0;
    while ((n = transport_receive(server, buf, sizeof buf, from, now)) >= 0) {
      const size_t a =
          status_answer(buf, static_cast<size_t>(n), from, s.protocol, 42, now, s, out, sizeof out);
      if (a) transport_send(server, from, out, a, now);
    }
    now += 20000;
    sim_advance(net, now);
    while ((n = transport_receive(client, buf, sizeof buf, from, now)) >= 0) {
      if (status_read_reply(buf, static_cast<size_t>(n), 5, cookie, got) == StatusReply::status) {
        have = true;
      }
    }
  }
  CHECK(have);
  CHECK(got.players.size() == 6 && got.name == "north arena");
}
