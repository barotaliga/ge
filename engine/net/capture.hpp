// A PACKET CAPTURE: every datagram a node sent and received, with its time, as a pcap file
// (libpcap's classic format, microseconds, link type RAW), so any packet tool opens it and
// `ge net` (M17b) decodes the engine's protocol inside it. Each datagram is wrapped in a made-up
// IPv4 or IPv6 and UDP header carrying its two addresses; a simulated node `sim:<n>:<port>` is
// written as 198.18.0.0/15 (the range set aside for benchmarking networks) plus n, and read back
// as the simulated address. The capture is built in memory: a lab attaches its bytes, a tool
// writes them to a file.
//
// ge:summary A packet capture as a pcap file: datagrams with times and addresses, read back
// ge:keywords net capture pcap libpcap packet-capture sniff trace wire dump record tcpdump
// ge:keywords inspect traffic
// ge:skill net
// ge:test tests/net_capture_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "net/address.hpp"

namespace ge::net {

struct Capture {
  std::vector<uint8_t> bytes;  // the file: the global header, then one record a datagram
  size_t packets = 0;
};

// An empty capture: the pcap global header alone.
Capture make_capture();
void capture_datagram(Capture& c, int64_t at_us, const Address& from, const Address& to,
                      const uint8_t* data, size_t size);

struct CapturedDatagram {
  int64_t at_us = 0;
  Address from, to;
  std::vector<uint8_t> data;
};

// A pcap file's datagrams (UDP over IPv4 or IPv6, link type RAW); false and `error` on anything
// else, so a capture from another tool says which of its records the reader cannot take.
bool read_capture(const uint8_t* bytes, size_t size, std::vector<CapturedDatagram>& out,
                  std::string* error);

}  // namespace ge::net
