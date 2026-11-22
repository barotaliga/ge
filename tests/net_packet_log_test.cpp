// The packet log (engine/net/packet_log.hpp): an entry's header and hex rows, the file opened new
// or added to with its lines, a transport's datagrams written both ways.
//
// ge:summary Tests the packet log: the entry text, the file and its modes, the datagrams
// ge:keywords test packet-log net_log hex-dump
// ge:skill net
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "check.hpp"
#include "net/packet_log.hpp"
#include "net/sim_network.hpp"
#include "net/transport.hpp"

namespace gn = ge::net;
namespace fs = std::filesystem;

namespace {

std::string slurp(const fs::path& p) {
  std::ifstream f(p);
  std::stringstream s;
  s << f.rdbuf();
  return s.str();
}

}  // namespace

TEST(an_entry_is_a_header_then_rows_of_sixteen_bytes_then_an_empty_line) {
  gn::Address a;
  CHECK(gn::parse_address("1.2.3.4:27910", a));
  const uint8_t bytes[18] = {0xff, 0xff, 0xff, 0xff, 'p', 'i', 'n', 'g', 0,
                             1,    2,    3,    'a',  'b', 'c', 'd', 'e', 0x7f};
  CHECK(gn::packet_log_text(1234, false, false, a, bytes, sizeof bytes) ==
        "1234 : UDP recv : 1.2.3.4:27910 : 18 bytes\n"
        "0000 : ff ff ff ff 70 69 6e 67 00 01 02 03 61 62 63 64 : ....ping....abcd\n"
        "0010 : 65 7f                                           : e.              \n"
        "\n");
  CHECK(gn::packet_log_text(5, true, true, a, bytes, 0) ==
        "5 : TCP send : 1.2.3.4:27910 : 0 bytes\n\n");
}

TEST(the_log_file_is_opened_new_or_added_to_and_reopened_on_a_change) {
  const fs::path root = fs::temp_directory_path() / "ge_net_packet_log_test";
  fs::remove_all(root);
  const fs::path file = root / "logs" / "network.log";
  {
    gn::PacketLog log;
    std::string said;
    CHECK(gn::packet_log_update(log, 1, root.string(), "network", 1, said));
    CHECK(said == "Logging network packets to logs/network.log\n");
    // A transport's datagrams both ways.
    gn::SimNetwork net = gn::make_sim_network(1, gn::LinkConditions{});
    gn::Transport a, b;
    CHECK(gn::open_sim_transport(a, net, gn::sim_address(0, 1)));
    CHECK(gn::open_sim_transport(b, net, gn::sim_address(1, 2)));
    a.log = &log;
    const uint8_t hi[2] = {'h', 'i'};
    CHECK(gn::transport_send(a, gn::sim_address(1, 2), hi, 2, 0));
    gn::sim_advance(net, 1000000);
    uint8_t buf[64];
    gn::Address from;
    CHECK(gn::transport_receive(b, buf, sizeof buf, from, 1000000) == 2);
    CHECK(gn::transport_send(b, from, hi, 2, 1000000));
    gn::sim_advance(net, 2000000);
    CHECK(gn::transport_receive(a, buf, sizeof buf, from, 2000000) == 2);
    const std::string text = slurp(file);
    CHECK_MSG(
        text.starts_with("0 : UDP send : " + gn::to_string(gn::sim_address(1, 2)) + " : 2 bytes\n"),
        text);
    CHECK_MSG(text.find("2000 : UDP recv : ") != std::string::npos, text);
    said.clear();
    CHECK(gn::packet_log_update(log, 2, root.string(), "network.log", 1, said));
    CHECK(said == "Closing network log.\nLogging network packets to logs/network.log\n");
    said.clear();
    CHECK(!gn::packet_log_update(log, 1, root.string(), "", 1, said));
    CHECK(log.file == nullptr && log.mode == 0);
  }
  fs::remove_all(root);
}
