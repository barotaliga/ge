// THE ACCOUNT SERVICE: the store answering over the network (docs/accounts.md § The parts). A
// master's process runs one beside its list, and a game server with no master set runs its own.
// It listens on its own UDP port (kAccountPort when an address names none).
//
// EVERY ASK IS SEALED. The service publishes one key: its Ed25519 public key (`sv_accountkey`),
// which checks its tickets and, converted, is the X25519 key an ask is sealed to. Each ask draws
// a new exchange key pair, seals its body with the key both sides share (net/crypto.hpp
// `box_key`) and is answered under the same key, so only the asker opens the answer. A key used
// for one ask needs no nonce of its own: the ask's and the answer's are fixed and differ.
//
// The datagrams (little endian):
//   key     "geAC" 1, padding to 64                       (unsealed: who are you)
//   pub     "geAC" 2, the service's public key (32)
//   ask     "geAC" 3, the ask's exchange key (32), tag (16), sealed body padded to kAccountAskSize:
//           op u8, then its fields, each a length u8 and its bytes
//   answer  "geAC" 4, the ask's exchange key (32), tag (16), sealed body:
//           refusal u8, wait_us u64, account u64, code (u8 length), words (u8 length),
//           bytes (u16 length): a ticket, or a friends' op's list (friends_service.hpp)
//   report  "geAC" 5, the server's public key (32), its signature (64) over the report, the report
//           (net/report.hpp's bytes)                     (unsealed: a report is no secret)
//   taken   "geAC" 6, the report's id u64, how it was taken u8 (matches.hpp's Taken), the
//           service's signature (64) over "geAC" 6, the server's key, the id and how
//   piece   "geAC" 7, server key (32), report id u64, number u8, count u8, bytes: a `report`'s
//           part after the key, split when too big for one datagram (`Piecing`)
// A report is a game server's finished match (docs/accounts.md § The match report): the service
// keeps it in `matches` by the server's key, adding it to the accounts' totals when the master's
// owner trusts that key. It is answered `taken` (sent again: the same answer), signed, so a server
// stops sending only on the service's own word.
// No answer outgrows its ask, so the service cannot be used to amplify traffic at a third party.
// An ask sent again (lost on the way, or replayed by someone who caught it) gets the answer it
// got the first time, from the recent answers kept by exchange key: an account is never made
// twice, nor a code spent twice, by one ask. A replayed ask's answer opens only for its asker.
//
// The ops:
//   create   name, password                 -> account, the recovery code (`code`)
//   sign_in  name, password                 -> account, a ticket
//   code     name, password                 -> account, a login code for a classic client
//   login    name, code, the player's address (asked by a game server for a classic client)
//                                           -> account, a ticket the server keeps for the seat
//   recover  name, recovery code, password  -> account, a new recovery code
//   ticket_code  a ticket this service signed  -> account, a login code (our own client's: it
//            holds a ticket, never the password, and the classic wire then carries only the code)
//   name     a name's key, folded by the asking game's rules
//                                           -> the account that reserves it (0: none), asked by a
//            game server whose master is elsewhere, to keep a registered name from a guest
//   friend, friends, presence, invite, invites   a ticket first: friends_service.hpp
// The throttle's address is the asker's; for `login` it is the server's and the player's
// together, so one player's wrong codes do not hold the whole server's.
//
//   service_init(svc, config, transport, fresh, ctx)   every frame: service_update(svc, now)
// An asker's half (Ask) on any socket:
//   ask_start(ask, op, service, its key, fields, fresh, now)
//   every frame: ask_update(ask, transport, now); each datagram: ask_read(ask, data, size, from)
//   `done`: `answered` with `reply`, or the service never answered
// Fresh bytes come from the caller (the system's, or a lab's seeded stream): it replays to the bit.
//
// ge:summary The account service over the network: sealed asks, sign-up, sign-in, tickets, codes
// ge:keywords account-service sealed ask x25519 sign-up sign-in ticket login-ask code recover
// ge:keywords amplification replay account-port service_update ask_start ask_read key-fetch pieces
// ge:skill net
// ge:test tests/account_service_test.cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "account/friends.hpp"
#include "account/matches.hpp"
#include "account/store.hpp"
#include "account/ticket.hpp"
#include "net/address.hpp"
#include "net/crypto.hpp"
#include "net/transport.hpp"

namespace ge::account {

constexpr uint16_t kAccountPort = 27901;  // the service's port when an address names none
constexpr size_t kAccountKeyAskSize = 64;
constexpr size_t kAccountAskSize = 512;
constexpr size_t kAccountAskHead = 4 + 1 + 32 + 16;

enum class Op : uint8_t {
  create = 1,
  sign_in = 2,
  code = 3,
  login = 4,
  recover = 5,
  ticket_code = 6,
  name = 7,
  friend_ = 8,  // the friends' ops (friends_service.hpp): each asked with a ticket
  friends = 9,
  presence = 10,
  invite = 11,
  invites = 12
};
constexpr uint8_t kOpLast = 12;
const char* op_name(Op op);

// New random bytes for one call: the operating system's (net::os_random_bytes) or a lab's stream.
using FreshFn = void (*)(void* ctx, uint8_t* out, size_t size);

struct ServiceConfig {
  net::Key
      seed{};  // the service's key pair grows from it; kept beside the store, never in the tree
  int64_t ticket_life_s = kTicketLife_s;
  size_t recent = 1024;  // answers kept for asks sent again
};

struct ServiceStats {
  uint64_t key_asks = 0, asks = 0, answers = 0, resent = 0;
  uint64_t refused = 0;  // short, unknown, not padded, or not sealed to our key
  uint64_t by_op[kOpLast + 1] = {};
  uint64_t tickets = 0;
  uint64_t reports = 0;  // match reports taken (sent again ones too)
};

constexpr size_t kReportPieces = 16, kReportPieceBytes = 1400;
constexpr size_t kPiecingsKept = 32;
constexpr int64_t kPiecingLife_us = 60000000;

// A report arriving in pieces, kept until the last comes (at most kPiecingsKept reports at once,
// the oldest dropped; each for kPiecingLife_us) and then taken whole; the server sends every piece
// again until the answer comes.
struct Piecing {
  net::Key server{};
  uint64_t id = 0;
  int64_t since_us = 0;
  std::vector<std::vector<uint8_t>> pieces;  // by number; empty: not yet come
  size_t have = 0;
};

struct Recent {
  net::Key asker{};
  std::vector<uint8_t> bytes;  // the answer datagram as sent
};

struct Service {
  ServiceConfig config;
  Store store;
  net::SignKeys keys;
  net::Key box_secret{}, box_public{};
  net::Transport* transport = nullptr;
  FreshFn fresh = nullptr;
  void* fresh_ctx = nullptr;
  std::vector<Recent> recent;  // a ring of `config.recent`
  size_t recent_next = 0;
  ServiceStats stats;
  Matches matches;  // the match reports servers sent (matches.hpp)
  Friends friends;  // friends, presence and invites between accounts (friends.hpp)
  std::vector<Piecing> pieces;
  uint8_t buffer[net::kMaxDatagram];
};

// The service on `transport` over `store` (moved in: one read back from its log, or a new one).
void service_init(Service& svc, const ServiceConfig& config, Store&& store,
                  net::Transport& transport, FreshFn fresh, void* fresh_ctx);
// Answer everything waiting on the service's port.
void service_update(Service& svc, int64_t now_us);
// One datagram, for a service sharing a socket: true when it was the service's.
bool service_read(Service& svc, const uint8_t* data, size_t size, const net::Address& from,
                  int64_t now_us);
inline const net::Key& service_key(const Service& svc) { return svc.keys.pub; }
// The `login` op answered IN PROCESS, for a game server that is its own master: a classic
// client's name and one-time code checked against the store under the same throttle (its address
// `self>` and the player's), and on success the seat's ticket signed into `ticket`.
Answer service_login(Service& svc, std::string_view name, std::string_view code,
                     std::string_view player, int64_t now_us, Ticket& ticket);

// A report taken IN PROCESS, from a game server that is its own master (its own key).
Taken service_report(Service& svc, const net::Key& server, const net::MatchReport& r);

// A GAME SERVER'S REPORT sent to a master elsewhere: signed with the server's key, sent again
// every `ask_us` until the master's signed answer comes or `max_tries` pass.
struct ReportAsk {
  net::Address service;
  net::Key service_key{}, server{};
  uint64_t id = 0;
  std::vector<std::vector<uint8_t>> datagrams;  // one `report`, or its pieces
  int64_t next_us = 0, ask_us = 2000000;
  int tries = 0, max_tries = 30;
  bool done = false, answered = false;
  Taken taken = Taken::shown;
};
// False when the report does not fit kReportPieces or is past its limits (nothing is sent).
bool report_ask_start(ReportAsk& a, const net::Address& service, const net::Key& service_key,
                      const net::SignKeys& server, const net::MatchReport& r, int64_t now_us);
void report_ask_update(ReportAsk& a, net::Transport& t, int64_t now_us);
// The service's answer to this report: true when the datagram was it (the caller drops it).
bool report_ask_read(ReportAsk& a, const uint8_t* data, size_t size, const net::Address& from);

// AN ASKER'S HALF.
struct Reply {
  Refusal refusal = Refusal::none;
  int64_t wait_us = 0;
  uint64_t account = 0;
  std::string code, words;
  std::vector<uint8_t> ticket;  // its bytes (ticket_read checks them)
};

struct Ask {
  Op op = Op::create;
  net::Address service;
  net::Key service_key{}, box_secret{}, box_public{}, key{};
  std::vector<uint8_t> datagram;  // the sealed ask, sent again as it was
  int64_t next_us = 0, ask_us = 500000;
  int tries = 0, max_tries = 6;
  bool done = false, answered = false;
  Reply reply;
};

// Seal an ask of `op` with its `fields` (at most 255 bytes each) to the service at `service`
// whose public key is `service_key`; `fresh` is the ask's exchange secret.
void ask_start(Ask& a, Op op, const net::Address& service, const net::Key& service_key,
               const std::vector<std::string>& fields, const net::Key& fresh, int64_t now_us);
void ask_update(Ask& a, net::Transport& t, int64_t now_us);
// The answer to this ask: true when the datagram was it (the caller drops it).
bool ask_read(Ask& a, const uint8_t* data, size_t size, const net::Address& from);

// THE KEY ASK, for a client that does not know the service's key yet. The key it learns this way
// is trusted on first use: a program that has `sv_accountkey` set compares, and refuses another.
struct KeyAsk {
  net::Address service;
  int64_t next_us = 0, ask_us = 500000;
  int tries = 0, max_tries = 6;
  bool done = false, answered = false;
  net::Key key{};
};
void key_ask_start(KeyAsk& k, const net::Address& service, int64_t now_us);
void key_ask_update(KeyAsk& k, net::Transport& t, int64_t now_us);
bool key_ask_read(KeyAsk& k, const uint8_t* data, size_t size, const net::Address& from);

}  // namespace ge::account
