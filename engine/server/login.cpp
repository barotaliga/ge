// Logins on a game server (server/login.hpp).
//
// ge:summary Logins on a game server: the line read, the ask, the answer, the seats' deadlines
// ge:keywords login login-line two-step-login login-ask ticket-check login-deadline
// ge:skill net
// ge:test tests/server_login_test.cpp
#include "server/login.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdio>

#include "account/matchmaker_service.hpp"
#include "account/ticket.hpp"
#include "core/num.hpp"
#include "net/udp_socket.hpp"

namespace ge::server {

namespace {

namespace ga = ge::account;

constexpr const char* kHow = "Log in with: login <name> <code>";

std::string_view trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
  while (!s.empty() &&
         (s.back() == ' ' || s.back() == '\t' || s.back() == '\n' || s.back() == '\r'))
    s.remove_suffix(1);
  return s;
}

// A chat line's text: its quotes taken off, as a client that sends a line it does not know as chat
// wraps it.
std::string_view unquote(std::string_view s) {
  s = trim(s);
  if (s.size() >= 2 && s.front() == '"' && s.back() == '"') s = s.substr(1, s.size() - 2);
  return trim(s);
}

std::vector<std::string_view> words(std::string_view s) {
  std::vector<std::string_view> out;
  size_t at = 0;
  while (at < s.size()) {
    while (at < s.size() && s[at] == ' ') at++;
    size_t end = at;
    while (end < s.size() && s[end] != ' ') end++;
    if (end > at) out.push_back(s.substr(at, end - at));
    at = end;
  }
  return out;
}

bool same_word(std::string_view a, const char* b) {
  size_t i = 0;
  for (; i < a.size() && b[i]; i++) {
    char c = a[i];
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    if (c != b[i]) return false;
  }
  return i == a.size() && b[i] == '\0';
}

// A word that reads as a one-time code: 8 to 16 letters and digits, dashes between.
bool looks_like_code(std::string_view w) {
  int n = 0;
  for (char c : w) {
    if (c == '-') continue;
    const bool alnum = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    if (!alnum) return false;
    n++;
  }
  return n >= 8 && n <= 16;
}

void tell(Logins& l, int slot, std::string text) {
  l.out.push_back({LoginOut::Kind::tell, slot, std::move(text)});
}

bool must_log_in(const LoginSettings& set, bool spectator) {
  return set.login >= 2 || (set.login == 1 && !spectator);
}

std::string seconds_text(double s) {
  char line[32];
  std::snprintf(line, sizeof line, "%d", static_cast<int>(core::num::round(s)));
  return line;
}

// The seat logged in as the account the ticket names (checked already).
void seat_in(Logins& l, int slot, uint64_t account, const std::string& name) {
  LoginSeat& seat = l.seats[slot];
  for (int i = 0; i < kLoginSeats; i++)
    if (i != slot && l.seats[i].state == LoginState::in && l.seats[i].account == account) {
      seat.state = LoginState::guest;
      seat.name.clear();
      l.stats.refused++;
      tell(l, slot, name + " is already logged in on this server.");
      return;
    }
  seat.state = LoginState::in;
  seat.account = account;
  seat.name = name;
  seat.must = false;
  l.stats.in++;
  l.out.push_back({LoginOut::Kind::auth, slot, name});
  if (l.set.name != 0) l.out.push_back({LoginOut::Kind::name, slot, name});
  l.out.push_back({LoginOut::Kind::tell_all, slot, name + " logged in."});
}

void refused(Logins& l, int slot, ga::Refusal r, int64_t wait_us) {
  LoginSeat& seat = l.seats[slot];
  seat.state = LoginState::guest;
  seat.name.clear();
  l.stats.refused++;
  if (r == ga::Refusal::wrong || r == ga::Refusal::unknown || r == ga::Refusal::bad_name)
    tell(l, slot,
         "That name and code don't match. A code works once, within 15 minutes of getting it.");
  else
    tell(l, slot, ga::refusal_words(r, wait_us));
}

// The name and code asked of whoever checks them.
void ask(Logins& l, int slot, std::string_view name, std::string_view code, int64_t unix_us) {
  LoginSeat& seat = l.seats[slot];
  l.stats.asked++;
  if (l.own != nullptr) {
    ga::Ticket t;
    const ga::Answer a = ga::service_login(*l.own, name, code, seat.address, unix_us, t);
    if (a.refusal != ga::Refusal::none) return refused(l, slot, a.refusal, a.wait_us);
    return seat_in(l, slot, t.account, t.name);
  }
  if (!l.key_known) {
    seat.state = LoginState::guest;
    l.stats.unanswered++;
    tell(l, slot, "The login service isn't reachable yet. Try again in a moment.");
    return;
  }
  net::Key fresh{};
  l.fresh(l.fresh_ctx, fresh.data(), fresh.size());
  ga::ask_start(seat.ask, ga::Op::login, l.master, l.master_key,
                {std::string(name), std::string(code), seat.address}, fresh, unix_us);
  net::wipe(fresh.data(), fresh.size());
  seat.state = LoginState::asking;
  seat.name = std::string(name);
}

// The answer to a seat's ask, read: the ticket checked against the master's key.
void answered(Logins& l, int slot, int64_t unix_us) {
  LoginSeat& seat = l.seats[slot];
  const ga::Reply& r = seat.ask.reply;
  if (!seat.ask.answered) {
    seat.state = LoginState::guest;
    l.stats.unanswered++;
    tell(l, slot, "The login service didn't answer. Try again in a moment.");
    return;
  }
  if (r.refusal != ga::Refusal::none) return refused(l, slot, r.refusal, r.wait_us);
  ga::Ticket t;
  const ga::TicketCheck c =
      ga::ticket_read(r.ticket.data(), r.ticket.size(), l.master_key, unix_us / 1000000, t);
  if (c != ga::TicketCheck::ok || r.account != t.account) {
    seat.state = LoginState::guest;
    l.stats.bad_tickets++;
    l.notes.push_back(std::string("the account master's login answer did not check: ") +
                      ga::ticket_check_name(c));
    tell(l, slot, "The login service's answer didn't check out, so you play as a guest.");
    return;
  }
  seat_in(l, slot, t.account, t.name);
}

bool reserved_here(const Logins& l, int slot, const std::string& key) {
  for (int i = 0; i < kLoginSeats; i++)
    if (i != slot && l.seats[i].state == LoginState::in &&
        ga::name_key(l.rules, l.seats[i].name) == key)
      return true;
  if (l.own == nullptr) return false;
  const auto it = l.own->store.keys.find(key);
  return it != l.own->store.keys.end() && ga::find(l.own->store, it->second) != nullptr;
}

// A master elsewhere's answer about `key`, kept; the master asked when it never was. False (let
// the name go) until it answers that an account holds it.
bool reserved_elsewhere(Logins& l, const std::string& key) {
  if (l.own != nullptr || l.link == nullptr || !l.key_known) return false;
  for (const NameCheck& c : l.names)
    if (c.key == key) return c.answered && c.reserved;
  if (l.names.size() >= kNameChecks) {  // the oldest answered one goes
    for (auto it = l.names.begin(); it != l.names.end(); ++it)
      if (it->answered) {
        l.names.erase(it);
        break;
      }
    if (l.names.size() >= kNameChecks) return false;  // all still asking: ask later
  }
  NameCheck& c = l.names.emplace_back();
  c.key = key;
  net::Key fresh{};
  l.fresh(l.fresh_ctx, fresh.data(), fresh.size());
  ga::ask_start(c.ask, ga::Op::name, l.master, l.master_key, {key}, fresh, l.now_us);
  l.stats.names_asked++;
  return false;
}

// A name the master says an account holds: every guest going by it is renamed (told now, or on
// joining when still connecting).
void name_answered(Logins& l, const NameCheck& c) {
  for (int i = 0; i < kLoginSeats; i++) {
    LoginSeat& seat = l.seats[i];
    if (seat.state == LoginState::in || seat.shown.empty() ||
        ga::name_key(l.rules, seat.shown) != c.key)
      continue;
    const std::string name = std::move(seat.shown);
    seat.shown.clear();
    l.out.push_back({LoginOut::Kind::unname, i, name});
    if (seat.state == LoginState::empty) {
      seat.renamed = name;
    } else {
      l.stats.renamed++;
      tell(
          l, i,
          "The name " + name + " belongs to a registered player. If it's yours, log in to use it.");
    }
  }
}

}  // namespace

bool logins_open(Logins& l, const LoginSettings& set, const account::NameRules& rules,
                 account::Service* own, net::Transport* link, account::FreshFn fresh,
                 void* fresh_ctx, int64_t unix_us, std::string& error) {
  l.set = set;
  l.rules = rules;
  l.own = nullptr;
  l.link = nullptr;
  l.key_known = false;
  l.fresh = fresh;
  l.fresh_ctx = fresh_ctx;
  for (LoginSeat& s : l.seats) s = LoginSeat{};
  l.names.clear();
  l.now_us = unix_us;
  l.out.clear();
  if (set.master.empty()) {
    l.own = own;
    if (own == nullptr && set.login > 0)
      error =
          "sv_login is on, but this server has no account service of its own and no "
          "sv_accountmaster; set sv_accountmaster to the master's address, or run the server "
          "as a dedicated process, which hosts its own";
    return error.empty();
  }
  if (!net::parse_address(set.master, l.master) &&
      !net::resolve_ipv4(set.master, account::kAccountPort, l.master, &error)) {
    error = "sv_accountmaster " + set.master + " is not an address (" + error +
            "); set it to the master's host[:port]";
    return false;
  }
  if (l.master.port == 0) l.master.port = account::kAccountPort;
  if (!set.key.empty()) {
    if (!net::key_from_hex(set.key, l.master_key, error)) {
      error = "sv_accountkey: " + error + "; set it to the 64 hex digits the master prints";
      return false;
    }
    l.key_known = true;
  }
  if (link == nullptr) {
    error = "sv_accountmaster is set, but the server has no socket to ask it on";
    return false;
  }
  l.link = link;
  if (!l.key_known) account::key_ask_start(l.key_ask, l.master, unix_us);
  return true;
}

bool logins_active(const Logins& l) { return l.own != nullptr || l.link != nullptr; }

void login_seat(Logins& l, int slot, int userid, bool spectator, std::string_view address,
                double now_s) {
  if (slot < 0 || slot >= kLoginSeats) return;
  LoginSeat& seat = l.seats[slot];
  if (seat.state != LoginState::empty && seat.userid == userid) return;
  std::string renamed = std::move(seat.renamed);
  std::string shown = std::move(seat.shown);
  seat = LoginSeat{};
  seat.shown = std::move(shown);
  seat.state = LoginState::guest;
  seat.userid = userid;
  seat.spectator = spectator;
  seat.address = std::string(address);
  if (!renamed.empty()) {
    l.stats.renamed++;
    tell(l, slot,
         "The name " + renamed +
             " belongs to a registered player. If it's yours, log in to use it.");
  }
  if (!logins_active(l)) return;
  seat.must = must_log_in(l.set, spectator);
  if (l.set.login == 0) return;
  if (!l.set.welcome.empty()) tell(l, slot, l.set.welcome);
  tell(l, slot, std::string("This server keeps stats for registered players. ") + kHow);
  if (seat.must) {
    seat.deadline_s = now_s + l.set.time_s;
    tell(l, slot,
         "Log in within " + seconds_text(l.set.time_s) + " seconds to stay on the server.");
  }
}

void login_leave(Logins& l, int slot) {
  if (slot < 0 || slot >= kLoginSeats) return;
  l.seats[slot] = LoginSeat{};
}

bool login_line(Logins& l, int slot, std::string_view line, double now_s, int64_t unix_us) {
  (void)now_s;
  l.now_us = unix_us;
  if (slot < 0 || slot >= kLoginSeats) return false;
  LoginSeat& seat = l.seats[slot];
  std::string_view text = trim(line);
  bool chat = false;
  {
    const std::vector<std::string_view> head = words(text);
    if (!head.empty() && (same_word(head[0], "say") || same_word(head[0], "say_team"))) {
      chat = true;
      text = unquote(text.substr(head[0].size()));
    }
  }
  const std::vector<std::string_view> w = words(text);
  const bool is_login = !w.empty() && same_word(w[0], "login");
  if (is_login) {
    l.stats.lines++;
    if (!logins_active(l)) {
      tell(l, slot, "This server doesn't keep logins.");
      return true;
    }
    if (seat.state == LoginState::in) {
      tell(l, slot, "You're already logged in as " + seat.name + ".");
      return true;
    }
    if (seat.state == LoginState::asking) {
      tell(l, slot, "Your login is being checked.");
      return true;
    }
    if (w.size() == 1 || w.size() > 3) {
      tell(l, slot, kHow);
      return true;
    }
    if (w.size() == 2) {
      seat.state = LoginState::code_next;
      seat.name = std::string(w[1]);
      tell(l, slot, "Enter your login code for " + seat.name + ":");
      return true;
    }
    ask(l, slot, w[1], w[2], unix_us);
    return true;
  }
  if (seat.state == LoginState::code_next) {
    if (w.size() == 1 && looks_like_code(w[0])) {
      l.stats.lines++;
      const std::string name = seat.name;
      ask(l, slot, name, w[0], unix_us);
      return true;
    }
    seat.state = LoginState::guest;  // something else said: the two-step login is forgotten
    seat.name.clear();
  }
  if (chat && seat.must && seat.state != LoginState::in) {
    l.stats.chat_held++;
    tell(l, slot, std::string("Log in before you chat. ") + kHow);
    return true;
  }
  return false;
}

void logins_frame(Logins& l, double now_s, int64_t unix_us) {
  l.now_us = unix_us;
  if (l.link != nullptr) {
    if (!l.key_known && !l.key_ask.done) account::key_ask_update(l.key_ask, *l.link, unix_us);
    for (LoginSeat& s : l.seats)
      if (s.state == LoginState::asking) account::ask_update(s.ask, *l.link, unix_us);
    for (NameCheck& c : l.names)
      if (!c.answered) account::ask_update(c.ask, *l.link, unix_us);
    if (l.offering) account::ask_update(l.offer_ask, *l.link, unix_us);
    net::Address from;
    for (;;) {
      const int n = net::transport_receive(*l.link, l.buffer, sizeof l.buffer, from, unix_us);
      if (n < 0) break;
      if (n == 0) continue;
      const auto size = static_cast<size_t>(n);
      if (!l.key_known && account::key_ask_read(l.key_ask, l.buffer, size, from)) {
        if (l.key_ask.answered) {
          l.master_key = l.key_ask.key;
          l.key_known = true;
          l.notes.push_back("the account master " + net::to_string(l.master) +
                            " answered with key " + net::key_to_hex(l.master_key) +
                            "; set sv_accountkey to it so no other key is believed");
        }
        continue;
      }
      bool read = false;
      for (LoginSeat& s : l.seats)
        if (s.state == LoginState::asking && account::ask_read(s.ask, l.buffer, size, from)) {
          read = true;
          break;
        }
      for (size_t k = 0; !read && k < l.names.size(); k++)
        read = !l.names[k].answered && account::ask_read(l.names[k].ask, l.buffer, size, from);
      if (!read && l.offering) account::ask_read(l.offer_ask, l.buffer, size, from);
    }
    if (l.offering && l.offer_ask.done) {
      l.offering = false;
      if (l.offer_ask.answered && l.offer_ask.reply.refusal == ga::Refusal::none) {
        l.stats.offers_taken++;
      } else if (l.offer_ask.answered) {
        l.notes.push_back("the account master did not take this server's ranked seats: " +
                          l.offer_ask.reply.words +
                          " (its owner trusts a server by the key printed at its start)");
      }
    }
    for (size_t k = 0; k < l.names.size();) {
      NameCheck& c = l.names[k];
      if (c.answered || !c.ask.done) {
        k++;
      } else if (!c.ask.answered) {  // no answer: asked again the next time the name comes
        l.names.erase(l.names.begin() + static_cast<std::ptrdiff_t>(k));
      } else {
        c.answered = true;
        c.reserved = c.ask.reply.refusal == ga::Refusal::none && c.ask.reply.account != 0;
        if (c.reserved) name_answered(l, c);
        k++;
      }
    }
    if (!l.key_known && l.key_ask.done && !l.key_ask.answered) {
      l.notes.push_back("the account master " + net::to_string(l.master) +
                        " did not answer for its key; asking again");
      account::key_ask_start(l.key_ask, l.master, unix_us);
    }
  }
  for (int i = 0; i < kLoginSeats; i++) {
    LoginSeat& s = l.seats[i];
    if (s.state == LoginState::asking && s.ask.done) answered(l, i, unix_us);
    const bool waiting =
        s.must && (s.state == LoginState::guest || s.state == LoginState::code_next);
    if (!waiting) continue;
    if (!s.warned && l.set.time_s > 20 && now_s >= s.deadline_s - 10) {
      s.warned = true;
      tell(l, i, std::string("10 seconds left to log in. ") + kHow);
    }
    if (now_s >= s.deadline_s) {
      l.stats.timed_out++;
      l.out.push_back({LoginOut::Kind::drop, i, "didn't log in in time"});
      s = LoginSeat{};
    }
  }
}

NameVerdict login_name(Logins& l, int slot, std::string& name, bool quiet) {
  if (slot < 0 || slot >= kLoginSeats) return NameVerdict::free;
  LoginSeat& seat = l.seats[slot];
  if (seat.state == LoginState::in) {
    if (l.set.name == 0 || name == seat.name) return NameVerdict::free;
    name = seat.name;
    if (!quiet) tell(l, slot, "You play as " + seat.name + " while logged in.");
    return NameVerdict::account;
  }
  if (l.set.reserved == 0 || !logins_active(l)) return NameVerdict::free;
  const std::string key = account::name_key(l.rules, name);
  if (key.empty()) return NameVerdict::free;
  if (!reserved_here(l, slot, key) && !reserved_elsewhere(l, key)) {
    seat.shown = name;
    return NameVerdict::free;
  }
  seat.shown.clear();
  l.stats.renamed += quiet ? 0 : 1;
  if (quiet)
    seat.renamed = name;
  else
    tell(l, slot, "The name " + name + " belongs to a registered player. Log in to use it.");
  return NameVerdict::reserved;
}

uint64_t login_account(const Logins& l, int slot) {
  if (slot < 0 || slot >= kLoginSeats) return 0;
  return l.seats[slot].state == LoginState::in ? l.seats[slot].account : 0;
}

bool login_key(const Logins& l, net::Key& out) {
  if (l.own != nullptr) {
    out = account::service_key(*l.own);
    return true;
  }
  if (!l.key_known) return false;
  out = l.master_key;
  return true;
}

std::string login_key_tag(const net::Key& key) { return net::key_to_hex(key).substr(0, 16); }

void logins_offer(Logins& l, std::string_view game, std::string_view address, uint16_t port,
                  int free_seats, int64_t unix_us) {
  if (l.set.ranked.empty() || unix_us < l.offer_due_us || l.offering) return;
  l.offer_due_us = unix_us + kOfferEvery_us;
  const int seats = std::max(0, std::min(free_seats, 64));
  const int64_t now_s = unix_us / 1000000;
  if (l.own != nullptr) {
    account::service_offer(*l.own, l.own->keys.pub, game, l.set.ranked, address, seats, now_s);
    l.stats.offers++;
    l.stats.offers_taken++;
    return;
  }
  if (l.link == nullptr || !l.key_known || l.signer == nullptr) return;
  const std::string text = account::offer_text(game, l.set.ranked, port, seats, now_s);
  const net::Signature sig =
      net::sign(*l.signer, reinterpret_cast<const uint8_t*>(text.data()), text.size());
  net::Key fresh{};
  l.fresh(l.fresh_ctx, fresh.data(), fresh.size());
  account::ask_start(l.offer_ask, ga::Op::offer, l.master, l.master_key,
                     {std::string(l.signer->pub.begin(), l.signer->pub.end()),
                      std::string(sig.begin(), sig.end()), std::string(game), l.set.ranked,
                      std::to_string(port), std::to_string(seats), std::to_string(now_s)},
                     fresh, unix_us);
  net::wipe(fresh.data(), fresh.size());
  account::ask_update(l.offer_ask, *l.link, unix_us);
  l.offering = true;
  l.stats.offers++;
}

}  // namespace ge::server
