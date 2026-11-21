// THE CLASSIC MASTERS: the list a classic server reports itself to (`setmaster`) and the
// heartbeats it sends them, for Quake's and Quake II's servers alike, each in its own form. A
// pure model: the server's frame asks it what to send and to whom, and sends it on its own port;
// names are resolved by the caller's resolver, so a test runs it on the simulated network.
//
// QUAKE (MasterFamily::qw): at most 8 masters, port 27000 when none is given. `setmaster` clears
// the list, then takes each address in turn, telling "Master server at A:P" and "Sending a
// ping." and pinging it (`k`, NUL; no answer awaited); `none` or a name that does not resolve
// says "Setting nomaster mode." and stops there (what came before it stays). A heartbeat goes on
// the first frame and every 300 s after, and at the next frame after a `setmaster` or
// `heartbeat`: `a`, a newline, the heartbeat's number (counted from 1, counted even with no
// master), a newline, the clients connected or playing, a newline; no connectionless prefix.
// Quitting sends `C` and a newline. "Sending heartbeat to A:P" is told for each send.
//
// QUAKE II (MasterFamily::q2): at most 8 masters, port 27900 when none is given; `setmaster`
// makes the server public and asks for a round, with no ping: "Master server at A:P." for each,
// "Couldn't resolve master: X", "Too many masters.". Only a public dedicated server beats: the
// first round 5 s after the start, then every 300 s, one master a frame in turn; each beat is the
// connectionless `heartbeat`, a newline and the status text. Quitting sends `shutdown` to each.
//
// ge:summary Classic master lists and heartbeats: setmaster, heartbeat, shutdown, for qw and q2
// ge:keywords setmaster heartbeat master-server masters shutdown public classic-master
// ge:keywords qw-heartbeat q2-heartbeat nomaster listmasters masters_list_text
// ge:skill classic net
// ge:test tests/classic_masters_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "net/address.hpp"

namespace ge::classic::protocol {

enum class MasterFamily : uint8_t { qw, q2 };

inline constexpr int kMostMasters = 8;
inline constexpr double kBeatEvery = 300;   // s between heartbeats (rounds)
inline constexpr double kQ2FirstRound = 5;  // s after the start, Quake II's first round
inline constexpr uint16_t kQwMasterPort = 27000, kQ2MasterPort = 27900;

// A datagram to send: where, and its bytes.
struct MasterSend {
  net::Address to;
  std::vector<uint8_t> bytes;
};

struct Masters {
  MasterFamily family = MasterFamily::qw;
  std::vector<net::Address> list;  // at most kMostMasters
  std::vector<std::string> names;  // each as `setmaster` was given it (Quake II's)
  std::vector<double> heard_s;     // when each last acknowledged a heartbeat (-1: never)
  int64_t sequence = 0;            // Quake's heartbeats so far (its number in the next is + 1)
  double last_s = -1e9;            // when the last heartbeat (round) started
  bool now = false;                // a heartbeat (round) asked for at the next frame
  bool is_public = false;          // Quake II: `public` (setmaster turns it on)
  size_t round_next = 0;           // Quake II: the next master of the round under way
  bool in_round = false;
};

// A master's acknowledgement (`ack`) from `from`: its time kept for `listmasters`.
void masters_heard(Masters& m, const net::Address& from, double now_s);
// `listmasters` (Quake II): `There are no masters.`, or a row a master: its number from 1, the
// name as given (21 columns), the milliseconds since its last acknowledgement (`never`, or `down`
// when the server is not running) and its address.
std::string masters_list_text(const Masters& m, double now_s, bool running);

// "host[:port]" resolved, the port `port` when it names none; false when it does not resolve.
using MasterResolve = std::function<bool(std::string_view text, uint16_t port, net::Address& out)>;

// The resolver a server uses: "1.2.3.4[:port]", "sim:<node>[:<port>]" or a host name (the OS's
// resolver, which blocks while it asks: a console command's, never a frame's).
bool resolve_master(std::string_view text, uint16_t port, net::Address& out);

// The list kept by `family`, its clock started at `now_s` (Quake beats on the first frame,
// Quake II 5 s on); a list already set stays.
void masters_start(Masters& m, MasterFamily family, double now_s);
// Whether masters_start has run.
bool masters_started(const Masters& m);
// `setmaster <args>`: the list as the family takes it, its lines into `said` and Quake's pings
// into `out`.
void set_masters(Masters& m, const std::vector<std::string>& args, const MasterResolve& resolve,
                 std::vector<std::string>& said, std::vector<MasterSend>& out);
// `heartbeat`: one at the next frame.
void force_heartbeat(Masters& m);
// A server frame at `now_s`: what goes to the masters now. `clients` is Quake's count (connected
// or playing), `status` Quake II's status text (its server info line and a line a player), and
// `dedicated` whether Quake II's server may beat at all.
void masters_frame(Masters& m, double now_s, int clients, const std::string& status, bool dedicated,
                   std::vector<MasterSend>& out, std::vector<std::string>& said);
// The server quitting: Quake's `C`, Quake II's `shutdown` (a public one's), to every master.
void masters_shutdown(const Masters& m, std::vector<MasterSend>& out,
                      std::vector<std::string>& said);

// The datagrams themselves.
std::vector<uint8_t> qw_heartbeat(int64_t sequence, int clients);
std::vector<uint8_t> qw_master_shutdown();
std::vector<uint8_t> qw_master_ping();
std::vector<uint8_t> q2_heartbeat(const std::string& status);
std::vector<uint8_t> q2_master_shutdown();

}  // namespace ge::classic::protocol
