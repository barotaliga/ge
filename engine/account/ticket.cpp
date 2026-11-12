// A ticket (account/ticket.hpp): issued, written, read and checked.
//
// ge:summary A ticket's bytes: issued and signed by the master, read and checked by a server
// ge:keywords account ticket sign check expiry bytes
// ge:skill net
// ge:test tests/account_ticket_test.cpp
#include "account/ticket.hpp"

#include <cstring>

#include "net/wire.hpp"

namespace ge::account {

namespace {

constexpr uint8_t kMagic[4] = {'g', 'e', 'T', 'K'};
constexpr uint8_t kVersion = 1;

size_t write_body(const Ticket& t, uint8_t* out, size_t capacity) {
  net::Writer w = net::make_writer(out, capacity);
  net::write_bytes(w, kMagic, 4);
  net::write_u8(w, kVersion);
  net::write_bytes(w, t.master.data(), 32);
  net::write_u64(w, t.account);
  net::write_u64(w, static_cast<uint64_t>(t.issued_s));
  net::write_u64(w, static_cast<uint64_t>(t.expires_s));
  const size_t n = t.name.size() < 255 ? t.name.size() : 255;
  net::write_u8(w, static_cast<uint8_t>(n));
  net::write_bytes(w, t.name.data(), n);
  return w.overflow ? 0 : w.size;
}

}  // namespace

Ticket ticket_issue(const net::SignKeys& keys, uint64_t account, std::string_view name,
                    int64_t now_s, int64_t life_s) {
  Ticket t;
  t.master = keys.pub;
  t.account = account;
  t.issued_s = now_s;
  t.expires_s = now_s + life_s;
  t.name = std::string(name.substr(0, 255));
  uint8_t body[kTicketMax];
  const size_t n = write_body(t, body, sizeof body);
  t.signature = net::sign(keys, body, n);
  return t;
}

size_t ticket_write(const Ticket& t, uint8_t* out, size_t capacity) {
  const size_t n = write_body(t, out, capacity);
  if (n == 0 || n + 64 > capacity) return 0;
  std::memcpy(out + n, t.signature.data(), 64);
  return n + 64;
}

TicketCheck ticket_read(const uint8_t* data, size_t size, const net::Key& master_pub, int64_t now_s,
                        Ticket& out) {
  net::Reader r = net::make_reader(data, size);
  const uint8_t* magic = net::read_view(r, 4);
  if (!magic || std::memcmp(magic, kMagic, 4) != 0 || net::read_u8(r) != kVersion) {
    return TicketCheck::malformed;
  }
  Ticket t;
  const uint8_t* master = net::read_view(r, 32);
  t.account = net::read_u64(r);
  t.issued_s = static_cast<int64_t>(net::read_u64(r));
  t.expires_s = static_cast<int64_t>(net::read_u64(r));
  const uint8_t n = net::read_u8(r);
  const uint8_t* name = net::read_view(r, n);
  const size_t body = r.at;
  const uint8_t* sig = net::read_view(r, 64);
  if (r.bad || !master || !name || !sig || r.at != size) return TicketCheck::malformed;
  std::memcpy(t.master.data(), master, 32);
  t.name.assign(reinterpret_cast<const char*>(name), n);
  std::memcpy(t.signature.data(), sig, 64);
  if (t.master != master_pub) return TicketCheck::other_master;
  if (!net::sign_check(master_pub, t.signature, data, body)) return TicketCheck::bad_signature;
  if (now_s >= t.expires_s) return TicketCheck::expired;
  if (now_s + 300 < t.issued_s) return TicketCheck::not_yet;  // five minutes of clock drift
  out = std::move(t);
  return TicketCheck::ok;
}

const char* ticket_check_name(TicketCheck c) {
  switch (c) {
    case TicketCheck::ok:
      return "ok";
    case TicketCheck::malformed:
      return "malformed";
    case TicketCheck::other_master:
      return "other-master";
    case TicketCheck::bad_signature:
      return "bad-signature";
    case TicketCheck::expired:
      return "expired";
    case TicketCheck::not_yet:
      return "not-yet";
  }
  return "?";
}

}  // namespace ge::account
