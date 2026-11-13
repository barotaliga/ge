// Our own client logging itself in (account/client_login.hpp).
//
// ge:summary Our own client logging itself in: the saved ticket traded for a one-time code
// ge:keywords client-login autologin ticket-code
// ge:skill net
// ge:test tests/account_client_login_test.cpp
#include "account/client_login.hpp"

#include "account/ticket.hpp"
#include "net/crypto.hpp"
#include "net/udp_socket.hpp"

namespace ge::account {

bool client_login_load(ClientLogin& c, const std::string& path, int64_t unix_s, std::string& why) {
  c = ClientLogin{};
  if (!account_file_read(path, c.file)) {
    why = "You haven't signed in to an account on this computer.";
    return false;
  }
  if (c.file.ticket.empty() || c.file.master.empty()) {
    why = "You haven't signed in to an account on this computer.";
    return false;
  }
  std::string error;
  if (!net::key_from_hex(c.file.key, c.key, error)) {
    why = "Your account file is damaged. Sign in again.";
    return false;
  }
  if (!net::parse_address(c.file.master, c.master) &&
      !net::resolve_ipv4(c.file.master, kAccountPort, c.master, &error)) {
    why = "Couldn't find the account server " + c.file.master + ".";
    return false;
  }
  if (c.master.port == 0) c.master.port = kAccountPort;
  c.ticket = hex_to_bytes(c.file.ticket);
  Ticket t;
  const TicketCheck check = ticket_read(c.ticket.data(), c.ticket.size(), c.key, unix_s, t);
  if (check == TicketCheck::expired) {
    why = "Your sign-in has run out. Sign in again.";
    return false;
  }
  if (check != TicketCheck::ok) {
    why = "Your account file is damaged. Sign in again.";
    return false;
  }
  if (c.file.name.empty()) c.file.name = t.name;
  c.tag = net::key_to_hex(c.key).substr(0, 16);
  return true;
}

bool client_login_fits(const ClientLogin& c, std::string_view tag) {
  return !c.tag.empty() && tag == c.tag;
}

void client_login_ask(ClientLogin& c, const net::Key& fresh, int64_t now_us) {
  ask_start(c.ask, Op::ticket_code, c.master, c.key,
            {std::string(c.ticket.begin(), c.ticket.end())}, fresh, now_us);
  c.step = ClientLoginStep::asking;
}

bool client_login_update(ClientLogin& c, net::Transport& t, int64_t now_us) {
  if (c.step != ClientLoginStep::asking) return c.step != ClientLoginStep::idle;
  ask_update(c.ask, t, now_us);
  uint8_t buffer[net::kMaxDatagram];
  net::Address from;
  for (;;) {
    const int n = net::transport_receive(t, buffer, sizeof buffer, from, now_us);
    if (n < 0) break;
    if (n > 0) ask_read(c.ask, buffer, static_cast<size_t>(n), from);
  }
  if (!c.ask.done) return false;
  if (!c.ask.answered) {
    c.step = ClientLoginStep::silent;
  } else if (c.ask.reply.refusal != Refusal::none || c.ask.reply.code.empty()) {
    c.step = ClientLoginStep::refused;
    c.words = c.ask.reply.words;
  } else {
    c.step = ClientLoginStep::code;
    c.code = c.ask.reply.code;
  }
  return true;
}

}  // namespace ge::account
