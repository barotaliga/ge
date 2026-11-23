// Tests: Quake II's protocols 35 and 36 on the live connection (net/q2_net_ext.hpp): the connect's
// tail and its refusals, the packet length, the move without a checksum and 35's compact form, the
// batched moves both ways and their limits, and an extended server's refusal of older clients.
//
// ge:summary Tests: Quake II's protocols 35 and 36: connect tails, compact and batched moves
// ge:keywords test protocol-36 connect-tail batched-moves compact-move
// ge:skill net testing
#include <cstdint>
#include <string>
#include <vector>

#include "check.hpp"
#include "classic/protocol/wire.hpp"
#include "shooters_edge/net/q2_net_ext.hpp"

namespace sn = shooters_edge::net;
namespace gn = ge::net;

namespace {

std::vector<std::string> words(std::initializer_list<const char*> w) {
  std::vector<std::string> out;
  for (const char* s : w) out.emplace_back(s);
  return out;
}

sn::Q2Cmd cmd(int pitch, int yaw, int fwd, int side, int buttons, int msec) {
  sn::Q2Cmd c;
  c.angles[0] = static_cast<int16_t>(pitch);
  c.angles[1] = static_cast<int16_t>(yaw);
  c.forward = static_cast<int16_t>(fwd);
  c.side = static_cast<int16_t>(side);
  c.buttons = static_cast<uint8_t>(buttons);
  c.msec = static_cast<uint8_t>(msec);
  return c;
}

}  // namespace

TEST(a_connects_tail_is_read_and_held_to_its_ranges) {
  sn::Q2ConnectAsk a;
  std::string why;
  CHECK(sn::q2_connect_ask(words({"connect", "36", "12", "5", "\\name\\x"}), a, why));
  CHECK(a.protocol == 36 && a.msglen == 1390 && a.chan == 1 && a.zlib && a.minor == 1015);
  CHECK(sn::q2_connect_ask(words({"connect", "36", "12", "5", "\\name\\x", "0", "0", "0", "1016"}),
                           a, why));
  CHECK(a.msglen == 4086 && a.chan == 0 && !a.zlib && a.minor == 1015);
  CHECK(sn::q2_connect_ask(words({"connect", "36", "1", "5", "u", "1390", "1", "1", "2000"}), a,
                           why));
  CHECK(a.minor == 1026);
  CHECK(sn::q2_connect_ask(words({"connect", "35", "1", "5", "u", "1400", "1"}), a, why));
  CHECK(a.protocol == 35 && a.msglen == 1400 && a.zlib && a.minor == 1903 && a.chan == 0);
  CHECK(!sn::q2_connect_ask(words({"connect", "36", "1", "5", "u", "5000"}), a, why));
  CHECK(why == "Invalid maximum message length.");
  CHECK(!sn::q2_connect_ask(words({"connect", "36", "1", "5", "u", "1390", "2"}), a, why));
  CHECK(why == "Invalid netchan type.");
  CHECK(!sn::q2_connect_ask(words({"connect", "37", "1", "5", "u"}), a, why));
  CHECK(why == "Unsupported protocol version 37.");
  CHECK(!sn::q2_connect_ask(words({"connect", "31", "1", "5", "u"}), a, why));
  CHECK(why == "You need Quake 2 version 3.19 or higher.");
  CHECK(sn::q2_connect_ask(words({"connect", "34", "1", "5", "u"}), a, why));
  CHECK(a.protocol == 34 && a.minor == 0 && !a.zlib);
}

TEST(the_packet_length_is_capped_by_the_server) {
  sn::Q2ConnectAsk a;
  a.protocol = 36;
  a.msglen = 4086;
  CHECK(sn::q2_msglen(a, 1390, false) == 1390);
  CHECK(sn::q2_msglen(a, 0, false) == 4086);
  CHECK(sn::q2_msglen(a, 1390, true) == 4086);
  a.msglen = 100;
  CHECK(sn::q2_msglen(a, 1390, false) == 512);
  a.protocol = 34;
  CHECK(sn::q2_msglen(a, 0, false) == 1390);
}

TEST(moves_of_35_and_36_read_back_in_both_forms) {
  const sn::Q2Cmd cmds[3] = {cmd(64, 256, 400, -200, 1, 16), cmd(-640, 512, 0, 13, 0x80, 17),
                             cmd(1000, -3000, 401, 0, 3, 16)};
  for (const int minor : {1903, 1904}) {
    uint8_t buf[256];
    gn::Writer w = gn::make_writer(buf, sizeof buf);
    sn::write_q2_move_ext(w, 35, minor, 77, cmds);
    gn::Reader r = gn::make_reader(buf, w.size);
    CHECK(ge::classic::protocol::read_byte(r) == sn::kClcMove);
    sn::Q2Move m;
    CHECK(sn::read_q2_move_ext(r, 35, minor, m));
    CHECK(m.frame == 77);
    for (int k = 0; k < 3; k++) CHECK(m.cmds[k] == cmds[k]);
    CHECK(gn::remaining(r) == 0);
  }
}

TEST(batched_moves_read_back_and_carry_their_limits) {
  sn::Q2Batch b;
  b.frame = 120;
  b.light = 40;
  b.frames = {{cmd(10, 20, 400, 0, 1, 8), cmd(10, 20, 400, 0, 1, 8)},
              {cmd(-30000, 200, -512, 511, 0x80, 9)},
              {cmd(-29990, 210, 0, 0, 0, 9), cmd(5, 5, 5, 5, 2, 255)}};
  for (auto& f : b.frames)
    for (sn::Q2Cmd& c : f) c.light = 40;
  uint8_t buf[512];
  gn::Writer w = gn::make_writer(buf, sizeof buf);
  sn::write_q2_batch(w, b);
  gn::Reader r = gn::make_reader(buf, w.size);
  const auto id = static_cast<uint8_t>(ge::classic::protocol::read_byte(r));
  CHECK((id & 0x1F) == sn::kClcMoveBatched && (id >> 5) == 2);
  sn::Q2Batch got;
  std::string why;
  CHECK(sn::read_q2_batch(r, id, got, why));
  CHECK(got.frame == 120 && got.light == 40 && got.frames.size() == 3);
  for (size_t f = 0; f < got.frames.size(); f++) {
    CHECK(got.frames[f].size() == b.frames[f].size());
    for (size_t k = 0; k < got.frames[f].size() && k < b.frames[f].size(); k++)
      CHECK(got.frames[f][k] == b.frames[f][k]);
  }
  CHECK(gn::remaining(r) == 0);
  // Four frames beyond the newest are too many; a cut stream reads past the end.
  const uint8_t many[] = {static_cast<uint8_t>(sn::kClcMoveNoDelta | (4 << 5)), 0};
  gn::Reader rm = gn::make_reader(many + 1, 1);
  CHECK(!sn::read_q2_batch(rm, many[0], got, why));
  CHECK(why == "too many frames in packet");
  gn::Reader rc = gn::make_reader(buf + 1, 6);
  CHECK(!sn::read_q2_batch(rc, id, got, why));
  CHECK(why == "read past end of message");
}

TEST(an_extended_server_refuses_clients_short_of_its_limits) {
  sn::Q2ConnectAsk a;
  const std::string old =
      "This is a protocol limit removing enhanced server.\nYour client version is not "
      "compatible. Make sure you are running the latest client version.";
  // Not extended: every protocol joins, and the status says 34.
  a.protocol = 34;
  CHECK(sn::q2_extended_refusal(0, a).empty());
  CHECK(sn::q2_status_protocol(0) == 34);
  // Extended: 34, 35 and 36 short of minor 1024 are refused in those words.
  CHECK(sn::q2_extended_refusal(1, a) == old);
  a.protocol = 35;
  a.minor = 1905;
  CHECK(sn::q2_extended_refusal(1, a) == old);
  a.protocol = 36;
  a.minor = 1023;
  CHECK(sn::q2_extended_refusal(1, a) == old);
  a.minor = 1024;
  CHECK(sn::q2_extended_refusal(1, a).empty());
  // With the newer interface the least minor is 1025.
  CHECK(sn::q2_extended_refusal(2, a) == old);
  a.minor = 1025;
  CHECK(sn::q2_extended_refusal(2, a).empty());
  CHECK(sn::q2_status_protocol(1) == 36 && sn::q2_status_protocol(2) == 36);
}
