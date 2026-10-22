// engine/net/capture.hpp: what a capture writes it reads back, every family, and a file it cannot
// take it says so.
//
// ge:summary Test: pcap captures written and read back, IPv4, IPv6 and simulated addresses
// ge:keywords test net capture pcap round-trip
// ge:skill net testing
#include "net/capture.hpp"

#include <string>
#include <vector>

#include "check.hpp"

using namespace ge::net;

TEST(capture_reads_back_what_it_wrote) {
  Capture c = make_capture();
  CHECK(c.bytes.size() == 24);
  Address v6;
  CHECK(parse_address("[2001:db8::7]:4000", v6));
  const Address from[3] = {ipv4(192, 168, 1, 2, 5000), sim_address(70000, 9), v6};
  const Address to[3] = {ipv4(10, 0, 0, 1, 6000), sim_address(0, 1), v6};
  const uint8_t data[5] = {1, 2, 3, 4, 5};
  for (int i = 0; i < 3; i++) capture_datagram(c, 1500000 + i, from[i], to[i], data, 3 + i);
  CHECK(c.packets == 3);
  std::vector<CapturedDatagram> back;
  std::string err;
  CHECK_MSG(read_capture(c.bytes.data(), c.bytes.size(), back, &err), err);
  CHECK(back.size() == 3);
  for (int i = 0; i < 3 && i < static_cast<int>(back.size()); i++) {
    CHECK(back[i].at_us == 1500000 + i);
    CHECK(back[i].from == from[i]);
    CHECK(back[i].to == to[i]);
    CHECK(back[i].data.size() == static_cast<size_t>(3 + i));
    CHECK(back[i].data[0] == 1);
  }
  // The IPv4 header's checksum sums to all ones.
  const uint8_t* ip = c.bytes.data() + 24 + 16;
  uint32_t sum = 0;
  for (int i = 0; i < 20; i += 2) sum += static_cast<uint32_t>(ip[i] << 8 | ip[i + 1]);
  while (sum >> 16) sum = (sum & 0xffff) + (sum >> 16);
  CHECK(sum == 0xffff);
}

TEST(capture_refuses_what_it_cannot_read) {
  std::vector<CapturedDatagram> back;
  std::string err;
  const uint8_t junk[30] = {1, 2, 3};
  CHECK(!read_capture(junk, sizeof junk, back, &err));
  CHECK(err.find("pcap") != std::string::npos);
  Capture c = make_capture();
  capture_datagram(c, 0, sim_address(1, 1), sim_address(2, 2), junk, 10);
  c.bytes.resize(c.bytes.size() - 3);
  CHECK(!read_capture(c.bytes.data(), c.bytes.size(), back, &err));
  CHECK(err.find("record 0") != std::string::npos);
}
