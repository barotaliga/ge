// THE CLIENT END OF A JOIN TO A QUAKE SERVER OF PROTOCOL 15 (q1_remote.hpp): its state, kept
// apart so the window's join (qw_remote.hpp's `Remote`) can hold one; the string commands it sends
// and its goodbye.
//
// ge:summary The state of the window's join to a Quake server of protocol 15, its words, goodbye
// ge:keywords netquake protocol-15 nqclientend nq_command nq_stop nq_server_word
// ge:skill net shooter
// ge:test tests/shooters_edge_q1_remote_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "net/transport.hpp"
#include "shooters_edge/net/qw_client.hpp"

namespace shooters_edge::net {

inline constexpr uint16_t kPortNetQuake = 26000;  // where Quake's own servers listen
inline constexpr int kNqTries = 3;                // connect requests before "No Response"
inline constexpr int64_t kNqTryUs = 2500000;      // the wait after each
inline constexpr int64_t kNqResendUs = 1000000;   // a reliable message unanswered, sent again

struct NqClientEnd {
  gn::Transport* t = nullptr;
  gn::Address server;                          // the port the accept named, once accepted
  ClientStage stage = ClientStage::challenge;  // connect, signon, active (playing), dropped
  std::string refused;                         // why it dropped, in the player's words
  std::string name = "player";
  int top = 0, bottom = 0;
  int signon = 0;         // the last signon stage answered (3: begun)
  bool updates = false;   // the first entity update came: moves go out
  int held_back = 0;      // moves kept back since the level began (the first two)
  int tries = 0;          // connect requests sent
  int64_t tried_us = -1;  // when the last one went
  int64_t heard_us = -1;  // when the server's last packet came
  float server_time = 0;  // the last `time` read (each move carries it back)
  int64_t frames = 0;     // unreliable frames read
  int64_t moves = 0;      // moves sent
  int setangles = 0;      // the server's turns read
  // The transport: the sequences, the reliable message being joined, the one in flight.
  uint32_t recv_seq = 0, send_seq = 0, unrel_out = 0;
  std::vector<uint8_t> partial, in_flight;
  bool pending = false;
  int64_t flight_us = 0;
  std::vector<std::string> outbox;  // string commands for the next reliable message
};

// A line for the server, as a string command (the word, a space, the rest; alone: a newline).
void nq_command(NqClientEnd& c, const std::string& line);
// The goodbye: the disconnect byte once, unreliably.
void nq_stop(NqClientEnd& c, int64_t now);
// The words a server of this protocol takes from a player (status, say, tell, kill, ...): the
// window forwards these and keeps every other word it does not know.
bool nq_server_word(const std::string& word);

}  // namespace shooters_edge::net
