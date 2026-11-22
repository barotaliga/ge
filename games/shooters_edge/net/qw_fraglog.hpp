// THE FRAG LOG on the classic endpoint: every frag the rules report (`logfrag`, one line
// "\killer\victim\" a frag) goes into a buffer of at most 1449 characters, emptied when a line
// would overflow it, and, while the operator's `fraglogfile` has one open, into a file of its own:
// the first unused `frag_<n>.log` (n 0 to 999) in the server's folder, flushed a line at a time.
// The buffer turns over (the console reads "beginning fraglog sequence <n>") when ten minutes have
// passed since the last turnover and it holds a line; the last one filled is what the out-of-band
// `log [<sequence>]` query answers with: "stdlog <sequence>", a newline, its lines and a zero byte,
// or the one byte `m` when the asker has that sequence already or no file is open. Neither answer
// carries the out-of-band marker. Facts: porting/shooters-edge/q1-qw-ledger.md S2 and S168.
//
// ge:summary The classic endpoint's frag log: fraglogfile, the buffer and the `log` query
// ge:keywords fraglogfile frag-log logfrag log-query stdlog frag_log qw_fraglog
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_admin_test.cpp
#pragma once

#include <string>

#include "net/address.hpp"
#include "shooters_edge/net/qw_server.hpp"

namespace shooters_edge::net {

inline constexpr size_t kFragBuffer = 1450;    // a buffer's bytes, its closing zero one of them
inline constexpr double kFragTurnoverS = 600;  // a filled buffer is served after ten minutes
inline constexpr int kFragFiles = 1000;        // frag_0.log .. frag_999.log

// The frame's new frag lines into the buffer and the open file, then the turnover.
void qw_frag_frame(QwServer& s);
// `fraglogfile`: the file opened (the first name not there) or closed; the operator's words.
std::string qw_fraglogfile(QwServer& s);
// The `log [<sequence>]` query's answer to `from`.
void qw_log_query(QwServer& s, const ge::net::Address& from, const std::string& sequence,
                  bool asked);
// The file closed (the server stopping).
void qw_frag_close(QwServer& s);

}  // namespace shooters_edge::net
