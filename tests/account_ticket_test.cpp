// Tests: tickets (engine/account/ticket.hpp): issued and read back, refused when another master
// signed it, when a byte changed, when it expired, and when it is cut short.
//
// ge:summary Tests: a ticket signed, written, read and checked; every refusal
// ge:keywords test account ticket sign check expired other-master bad-signature malformed
// ge:skill net testing
#include "account/ticket.hpp"

#include "check.hpp"

namespace ga = ge::account;
namespace gn = ge::net;

namespace {

gn::Key seed_of(uint8_t b) {
  gn::Key k{};
  for (size_t i = 0; i < k.size(); i++) k[i] = static_cast<uint8_t>(b + i);
  return k;
}

constexpr int64_t kNow = 1794500000;

}  // namespace

TEST(ticket_round_trips_and_checks) {
  const gn::SignKeys keys = gn::sign_keys(seed_of(1));
  const ga::Ticket t = ga::ticket_issue(keys, 42, "Niclas", kNow);
  uint8_t bytes[ga::kTicketMax];
  const size_t n = ga::ticket_write(t, bytes, sizeof bytes);
  CHECK(n == ga::kTicketHead + 6 + 64);
  ga::Ticket back;
  CHECK(ga::ticket_read(bytes, n, keys.pub, kNow + 60, back) == ga::TicketCheck::ok);
  CHECK(back.account == 42 && back.name == "Niclas" && back.expires_s == kNow + ga::kTicketLife_s);
}

TEST(ticket_refusals) {
  const gn::SignKeys keys = gn::sign_keys(seed_of(1));
  const gn::SignKeys other = gn::sign_keys(seed_of(2));
  const ga::Ticket t = ga::ticket_issue(keys, 42, "Niclas", kNow);
  uint8_t bytes[ga::kTicketMax];
  const size_t n = ga::ticket_write(t, bytes, sizeof bytes);
  ga::Ticket back;
  CHECK(ga::ticket_read(bytes, n, other.pub, kNow, back) == ga::TicketCheck::other_master);
  CHECK(ga::ticket_read(bytes, n, keys.pub, kNow + ga::kTicketLife_s, back) ==
        ga::TicketCheck::expired);
  CHECK(ga::ticket_read(bytes, n, keys.pub, kNow - 3600, back) == ga::TicketCheck::not_yet);
  CHECK(ga::ticket_read(bytes, n - 1, keys.pub, kNow, back) == ga::TicketCheck::malformed);
  bytes[n - 70] ^= 1;  // a byte of the name: "Niclas" made another name
  CHECK(ga::ticket_read(bytes, n, keys.pub, kNow, back) == ga::TicketCheck::bad_signature);
  // A ticket signed by another key but naming this master's: the signature fails.
  ga::Ticket forged = ga::ticket_issue(other, 1, "admin", kNow);
  forged.master = keys.pub;
  const size_t m = ga::ticket_write(forged, bytes, sizeof bytes);
  CHECK(ga::ticket_read(bytes, m, keys.pub, kNow, back) == ga::TicketCheck::bad_signature);
}
