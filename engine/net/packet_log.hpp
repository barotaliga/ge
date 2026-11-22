// THE PACKET LOG: every datagram a transport sends or takes in, written as text to a file while
// an operator watches a live server's traffic (Quake II's `net_log_enable`, `net_log_name` and
// `net_log_flush` are its settings' spellings). Each entry is a header line, the time in
// milliseconds since the process started, the kind (`UDP` or `TCP` and `recv` or `send`), the
// peer's address and the length ("123 : UDP recv : 1.2.3.4:27910 : 42 bytes"), then the bytes 16
// to a row: a 4-digit offset, the bytes in hex, and the printable ones as letters (the rest as
// `.`), then an empty line. A send is logged once it went; a failed send is not.
//
// The file is `logs/<name>.log` (`.log` added unless it ends so) under a folder the caller gives:
// mode 1 writes it new, 2 or more adds to its end; flush 0 buffers, 1 flushes each entry, 2 or
// more writes unbuffered. A change of any of them closes and opens it again ("Closing network
// log.", "Logging network packets to logs/<name>.log"); one that cannot be opened says so
// ("Couldn't open logs/<name>.log: <reason>") and the caller sets the mode back to 0.
// Point a transport's `log` at one (transport.hpp), as with a capture.
//
// ge:summary The packet log: every datagram in and out as a text hex dump in logs/<name>.log
// ge:keywords net packet-log net_log net_log_enable net_log_name net_log_flush hex-dump traffic
// ge:skill net
// ge:test tests/net_packet_log_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

#include "net/address.hpp"

namespace ge::net {

struct PacketLog {
  std::FILE* file = nullptr;
  std::string shown;  // the path as printed: logs/<name>.log
  // The settings the file was opened with (the next check compares).
  int mode = 0;
  int flush = 0;
  std::string name;
  std::string folder;
  int64_t start_us = 0;  // the clock's reading the times count from

  PacketLog() = default;
  PacketLog(const PacketLog&) = delete;
  PacketLog& operator=(const PacketLog&) = delete;
  ~PacketLog();
};

// The log brought to the settings (above). Lines to print are added to `said`, each ending in a
// newline. False when the file could not be opened: the caller sets the mode back to 0.
bool packet_log_update(PacketLog& log, int mode, std::string_view folder, std::string_view name,
                       int flush, std::string& said);
void packet_log_close(PacketLog& log);
// One entry's text (above): `ms` the time, `tcp` the kind, `sent` the direction.
std::string packet_log_text(int64_t ms, bool tcp, bool sent, const Address& peer,
                            const uint8_t* data, size_t size);
// One datagram written, when the log is open (`now_us` on the transport's clock).
void packet_log_datagram(PacketLog& log, int64_t now_us, bool tcp, bool sent, const Address& peer,
                         const uint8_t* data, size_t size);

}  // namespace ge::net
