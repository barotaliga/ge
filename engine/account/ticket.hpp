// A TICKET: what a player signed in from our own client carries to every server it joins
// (docs/accounts.md § Logging in). The master signs it with its Ed25519 key; a server checks it
// with the master's public key (`sv_accountkey`) alone, so a player logs in while the master is
// down. A ticket is good for 24 hours and for every game.
//
// The bytes (little endian):
//   "geTK" version u8 (1), master's public key (32), account u64, issued_s i64, expires_s i64,
//   name length u8 and the name, then the signature (64) over everything before it
//
// A ticket names the master that signed it, so a server told another master's key says which.
// It is a bearer token: it travels only inside a sealed channel (the account service's answer,
// the game's sealed connect), never in the clear.
//
//   ticket_issue(keys, id, name, now_s, life_s)     -> the ticket, signed
//   ticket_write(ticket, out, capacity)            -> its bytes
//   ticket_read(data, size, master_pub, now_s, out) -> ok, or why not
//
// ge:summary A ticket: an account's name and id signed by the master, checked offline by a server
// ge:keywords account ticket signed-ticket ed25519 signature bearer expiry offline-check
// ge:keywords ticket_issue ticket_write ticket_read ticket-check master-key sv_accountkey
// ge:skill net
// ge:test tests/account_ticket_test.cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

#include "net/crypto.hpp"

namespace ge::account {

constexpr int64_t kTicketLife_s = 24ll * 3600;
constexpr size_t kTicketHead = 4 + 1 + 32 + 8 + 8 + 8 + 1;
constexpr size_t kTicketMax = kTicketHead + 255 + 64;

struct Ticket {
  net::Key master{};  // the public key it was signed with
  uint64_t account = 0;
  int64_t issued_s = 0, expires_s = 0;
  std::string name;
  net::Signature signature{};
};

enum class TicketCheck : uint8_t {
  ok,
  malformed,      // not a ticket's bytes
  other_master,   // signed by another master than the one asked about
  bad_signature,  // changed after signing, or forged
  expired,
  not_yet  // issued in the future (a master's clock far ahead)
};

Ticket ticket_issue(const net::SignKeys& keys, uint64_t account, std::string_view name,
                    int64_t now_s, int64_t life_s = kTicketLife_s);
// The ticket's bytes into `out`; 0 when `capacity` is short (kTicketMax always fits).
size_t ticket_write(const Ticket& t, uint8_t* out, size_t capacity);
TicketCheck ticket_read(const uint8_t* data, size_t size, const net::Key& master_pub, int64_t now_s,
                        Ticket& out);
const char* ticket_check_name(TicketCheck c);

}  // namespace ge::account
