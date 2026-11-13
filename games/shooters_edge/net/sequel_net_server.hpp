// THE SEQUEL'S NET SERVER (protocol 34, sequel_net.hpp): the sequel's rules
// (server/sequel_server.hpp, through a RulesRunner) served to clients over engine/net's transports,
// each client's view written by the sequel's wire (server/sequel_wire.hpp), the same bytes a
// recording holds.
//
// OUT OF BAND it answers `getchallenge` (one number per address, kept), `connect` (the protocol,
// the challenge, a free slot, the password; a refusal is a print a player reads), `info 34`,
// `status` and `ping`. A second connect from a slot's address and qport while it is in use is
// ignored.
//
// THE SIGNON follows the client's string commands (`new`, `configstrings`, `baselines`, `begin`,
// each with the level's count) and answers each with up to about 700 bytes and the next prompt;
// a wrong count starts it again. `begin` puts the client in the rules' slot.
//
// FRAMES: every host tick (`sequel_net_frame`, at the rules' step, 13 ms) reads every datagram,
// hands the moves to the rules as packets (their three commands, the rules running those a lost
// packet held), and when the rules have run a 10 Hz frame writes each client its frame: a delta
// from the frame its newest move acknowledged when that is the last one written, else whole. The
// frame's prints, centred prints, stuffed lines and changed configstrings go on the reliable
// channel; the frame and its sounds unreliably.
//
// A LEVEL CHANGE (`sequel_net_level`) keeps every connection: the count goes up, each client is
// sent `changing` and `reconnect` and asks for the signon again with `new`.
//
// A client is dropped on `disconnect`, after `timeout` seconds of silence, on a message it cannot
// read, or on overflowing its reliable channel; it is sent the disconnect message once.
//
// ge:summary The sequel's net server: the rules served to live protocol 34 clients over a transport
// ge:keywords sequel-server protocol-34 live-server getchallenge connect signon configstrings
// ge:keywords baselines begin frame delta ack move checksum status info timeout disconnect
// ge:keywords sequel_net_start sequel_net_frame sequel_net_stop sequel_net_level level-change
// ge:keywords changelevel changing reconnect
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_net_test.cpp
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "net/transport.hpp"
#include "shooters_edge/net/classic_channel.hpp"
#include "shooters_edge/net/sequel_net.hpp"
#include "shooters_edge/server/rules_runner.hpp"
#include "shooters_edge/server/sequel_server.hpp"
#include "shooters_edge/server/sequel_wire.hpp"

namespace shooters_edge::net {

namespace sv = shooters_edge::server;

struct SequelNetConfig {
  std::string hostname = "Shooter's Edge";
  int max_clients = 8;
  std::string password;  // "" or "none": none asked
  double timeout = 125;  // s of silence before a client is dropped
  // Moves whose checksum byte is wrong are ignored (the frame they acknowledge still counts).
  bool verify_checksum = true;
  sv::SequelWireOptions wire;  // the map's checksum, the sky; the count and the loop set here
};

enum class SequelSlot : uint8_t { free, signon, active };

struct SequelNetClient {
  SequelSlot stage = SequelSlot::free;
  gn::Address address;
  uint16_t qport = 0;
  Channel ch;
  sv::SequelWire wire;
  sv::SequelSignon signon;
  std::string userinfo, name;
  bool spectator = false;
  double heard = 0, sent = 0;  // the network's times of the last packet each way, s
  int32_t acked = -1;          // the frame the newest move acknowledged
  int64_t moves = 0, bad_checksums = 0, frames = 0, full_frames = 0;
};

struct SequelNetServer {  // NOLINT(clang-analyzer-optin.performance.Padding): one a server
  SequelNetConfig cfg;
  const sv::RulesRunner* rules = nullptr;
  sv::SequelServer* game = nullptr;
  gn::Transport* transport = nullptr;
  SequelNetClient clients[sh::kMaxClients];
  std::map<uint64_t, int32_t> challenges;  // by the address with its port off
  uint32_t next_challenge = 0;
  int32_t count = 0;  // the level's count, as the server data carries it
  int64_t written_frame = -1;
  double now = 0;
  std::vector<std::string> log;  // what the server would print on its console (the first 512)
  int64_t datagrams_in = 0, bytes_in = 0, bytes_out = 0, refused = 0;
};

// Serves the started sequel runner `rules` (a map loaded) on `t`. False when `rules` is not the
// sequel's.
bool sequel_net_start(SequelNetServer& s, const sv::RulesRunner& rules, gn::Transport& t,
                      const SequelNetConfig& cfg, std::string& error);
// One host tick at the network's time.
void sequel_net_frame(SequelNetServer& s, int64_t now_us);
// Every client told the server is going, and dropped.
void sequel_net_stop(SequelNetServer& s);
int sequel_net_clients(const SequelNetServer& s);
// The rules moved onto another map (their `start` run again) with `wire` the new map's checksum
// and sky: the level's count goes up and every client is told the level changed (`changing`, then
// `reconnect`), and signs on again on the same connection; `begin` seats it on the new map.
void sequel_net_level(SequelNetServer& s, const sv::SequelWireOptions& wire);

}  // namespace shooters_edge::net
