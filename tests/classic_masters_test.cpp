// THE CLASSIC MASTERS (plugins/classic/protocol/masters.hpp): Quake's setmaster pings and its
// heartbeats on the first frame and every 300 s, Quake II's public rounds one master a frame, the
// shutdown each sends, and the datagrams' bytes.
//
// ge:summary Tests: classic master lists, heartbeats and shutdowns for Quake and Quake II
// ge:keywords test setmaster heartbeat master-server shutdown
// ge:skill classic net testing
#include <string>
#include <vector>

#include "check.hpp"
#include "classic/protocol/masters.hpp"

namespace cp = ge::classic::protocol;
namespace gn = ge::net;

namespace {

// "sim:<node>" resolves to that simulated node; anything else does not.
bool resolve(std::string_view text, uint16_t port, gn::Address& out) {
  if (!text.starts_with("sim:")) return false;
  std::string t(text);
  if (t.find(':', 4) == std::string::npos) t += ":" + std::to_string(port);
  return gn::parse_address(t, out);
}

std::string text(const std::vector<uint8_t>& b) { return {b.begin(), b.end()}; }

}  // namespace

TEST(qw_heartbeat_bytes_and_the_shutdown) {
  CHECK(text(cp::qw_heartbeat(1, 3)) == "a\n1\n3\n");
  CHECK(text(cp::qw_master_shutdown()) == "C\n");
  const std::vector<uint8_t> ping = cp::qw_master_ping();
  CHECK(ping.size() == 2 && ping[0] == 'k' && ping[1] == 0);
  CHECK(text(cp::q2_heartbeat("\\hostname\\x\n")) == "\xff\xff\xff\xffheartbeat\n\\hostname\\x\n");
  CHECK(text(cp::q2_master_shutdown()) == "\xff\xff\xff\xffshutdown");
}

TEST(qw_setmaster_pings_each_and_beats_at_once_then_every_300_s) {
  cp::Masters m;
  cp::masters_start(m, cp::MasterFamily::qw, 0);
  std::vector<cp::MasterSend> out;
  std::vector<std::string> said;
  cp::masters_frame(m, 0, 2, "", true, out, said);  // the first frame: a beat, to nobody
  CHECK(out.empty() && m.sequence == 1);
  cp::set_masters(m, {"sim:5", "sim:6:27001"}, resolve, said, out);
  CHECK(m.list.size() == 2 && out.size() == 2);
  CHECK(said.size() == 4 && said[0].starts_with("Master server at ") &&
        said[1] == "Sending a ping.");
  CHECK(m.list[0].port == cp::kQwMasterPort && m.list[1].port == 27001);
  out.clear();
  cp::masters_frame(m, 1, 2, "", true, out, said);  // setmaster asks for one at once
  CHECK(out.size() == 2 && text(out[0].bytes) == "a\n2\n2\n");
  out.clear();
  cp::masters_frame(m, 200, 2, "", true, out, said);
  CHECK(out.empty());
  cp::masters_frame(m, 301, 3, "", true, out, said);
  CHECK(out.size() == 2 && text(out[1].bytes) == "a\n3\n3\n");
  // `none` (or a name that does not resolve) stops there: what came before it stays.
  said.clear();
  cp::set_masters(m, {"sim:7", "none", "sim:8"}, resolve, said, out);
  CHECK(m.list.size() == 1 && said.back() == "Setting nomaster mode.");
  out.clear();
  cp::masters_shutdown(m, out, said);
  CHECK(out.size() == 1 && text(out[0].bytes) == "C\n");
}

TEST(q2_masters_beat_only_when_public_one_a_frame_from_5_s_on) {
  cp::Masters m;
  cp::masters_start(m, cp::MasterFamily::q2, 0);
  std::vector<cp::MasterSend> out;
  std::vector<std::string> said;
  cp::masters_frame(m, 10, 0, "status\n", true, out, said);
  CHECK(out.empty());  // not public: nothing
  cp::set_masters(m, {"sim:5", "nowhere", "sim:6"}, resolve, said, out);
  CHECK(m.is_public && m.list.size() == 2 && out.empty());  // no ping
  CHECK(said[1] == "Couldn't resolve master: nowhere" && said[0].back() == '.');
  CHECK(m.list[0].port == cp::kQ2MasterPort);
  cp::masters_frame(m, 11, 0, "status\n", false, out, said);
  CHECK(out.empty());  // a listen server never beats
  cp::masters_frame(m, 11, 0, "status\n", true, out, said);
  CHECK(out.size() == 1 && text(out[0].bytes) == "\xff\xff\xff\xffheartbeat\nstatus\n");
  cp::masters_frame(m, 11.1, 0, "status\n", true, out, said);
  CHECK(out.size() == 2 && out[1].to == m.list[1]);
  cp::masters_frame(m, 11.2, 0, "status\n", true, out, said);
  CHECK(out.size() == 2);  // the round done
  std::vector<std::string> args(9, "sim:9");
  said.clear();
  cp::set_masters(m, args, resolve, said, out);
  CHECK(m.list.size() == 8 && said.back() == "Too many masters.");
  out.clear();
  cp::masters_shutdown(m, out, said);
  CHECK(out.size() == 8 && text(out[0].bytes) == "\xff\xff\xff\xffshutdown");
}

TEST(q2_first_round_waits_5_s) {
  cp::Masters m;
  cp::masters_start(m, cp::MasterFamily::q2, 100);
  m.is_public = true;
  std::vector<std::string> said;
  std::vector<cp::MasterSend> out;
  gn::Address a;
  CHECK(resolve("sim:4", 0, a));
  m.list.push_back(a);
  cp::masters_frame(m, 104, 0, "s", true, out, said);
  CHECK(out.empty());
  cp::masters_frame(m, 105, 0, "s", true, out, said);
  CHECK(out.size() == 1);
}
