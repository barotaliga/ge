// The game CLI's `account` group (docs/accounts.md § The service on the wire): a player's account
// at a master's account service, from the command line. Sign up, sign in for a ticket, a one-time
// code for a classic client's console login, recovery. The service's address and key and the last
// ticket are kept in `<state>/account.txt` (owner-only): the key is learned at the first contact
// and checked at every later one (`--key=<hex>` pins it instead).
//
// ge:summary The game CLI's account group: sign up, sign in, a login code, recover, the saved key
// ge:keywords gamecli account signup sign-in login-code recover ticket account-file master-key
// ge:keywords trust-on-first-use cmd_account
// ge:skill agent-interface
// ge:test tests/gamecli_account_test.cpp
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <thread>

#include "account/account_file.hpp"
#include "account/friends_service.hpp"
#include "account/names.hpp"
#include "account/service.hpp"
#include "account/ticket.hpp"
#include "core/json_write.hpp"
#include "gamecli/commands.hpp"
#include "net/token.hpp"
#include "net/transport.hpp"
#include "net/udp_socket.hpp"

namespace ge::gamecli {

namespace fs = std::filesystem;
namespace gn = ge::net;
namespace ga = ge::account;
using core::json_arr;
using core::json_num;
using core::json_obj;
using core::json_str;

namespace {

using Clock = std::chrono::steady_clock;

int64_t since_us(Clock::time_point t0) {
  return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count();
}

int64_t unix_s() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

// What `<state>/account.txt` holds (account/account_file.hpp: the game's own client reads it too).
using Saved = ga::AccountFile;

std::string file_of(const Ctx& ctx) {
  return (fs::path(ctx.places.state) / "account.txt").string();
}

Saved load(const Ctx& ctx) {
  Saved s;
  ga::account_file_read(file_of(ctx), s);
  return s;
}

bool save(const Ctx& ctx, const Saved& s) { return ga::account_file_write(file_of(ctx), s); }

std::string hex(const std::vector<uint8_t>& bytes) { return ga::bytes_to_hex(bytes); }
std::vector<uint8_t> unhex(const std::string& s) { return ga::hex_to_bytes(s); }

double wait_of(const Ctx& ctx) {
  const std::string w = ctx.flag("wait");
  const double v = w.empty() ? 0 : std::strtod(w.c_str(), nullptr);
  return v > 0 ? v : 4;
}

// The password from --password, else one line of standard input.
std::string password_of(Ctx& ctx, const char* flag) {
  if (ctx.has(flag)) return ctx.flag(flag);
  std::string line;
  std::getline(std::cin, line);
  if (!line.empty() && line.back() == '\r') line.pop_back();
  return line;
}

// The service reached and asked: true with `reply`, or false with the answer refused.
bool ask(Ctx& ctx, ga::Op op, const std::vector<std::string>& fields, Saved& saved,
         ga::Reply& reply) {
  if (ctx.has("master") && ctx.flag("master") != saved.master) {
    saved = Saved{};  // another service: its key is learned afresh, the old ticket is not its
    saved.master = ctx.flag("master");
  }
  if (saved.master.empty()) {
    refuse(ctx, 2, "no account service named",
           "pass --master=<host[:port]> (a hub run with --accounts, or a server that is its own "
           "master); it is kept for the next time");
    return false;
  }
  gn::Address at;
  std::string error;
  if (!gn::parse_address(saved.master, at) &&
      !gn::resolve_ipv4(saved.master, ga::kAccountPort, at, &error)) {
    refuse(ctx, 1, saved.master + " is not an address: " + error, "check --master");
    return false;
  }
  if (at.port == 0) at.port = ga::kAccountPort;
  gn::Transport t;
  if (!gn::open_udp_transport(t, gn::ipv4(0, 0, 0, 0, 0), &error)) {
    refuse(ctx, 1, "the socket did not open: " + error, "check that this machine has a network");
    return false;
  }
  const int64_t until = static_cast<int64_t>(wait_of(ctx) * 1e6);
  const Clock::time_point t0 = Clock::now();
  uint8_t buf[gn::kMaxDatagram];
  gn::Key key{};
  const std::string pinned = ctx.has("key") ? ctx.flag("key") : saved.key;
  if (!pinned.empty() && !gn::key_from_hex(pinned, key, error)) {
    gn::close_transport(t);
    refuse(ctx, 2, "the service's key: " + error, "pass --key=<64 hex digits> or leave it out");
    return false;
  }
  ga::KeyAsk k;
  ga::key_ask_start(k, at, 0);
  while (!k.done && since_us(t0) < until) {
    const int64_t now = since_us(t0);
    ga::key_ask_update(k, t, now);
    gn::Address from;
    const int n = gn::transport_receive(t, buf, sizeof buf, from, now);
    if (n > 0)
      ga::key_ask_read(k, buf, static_cast<size_t>(n), from);
    else
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  if (!k.answered) {
    gn::close_transport(t);
    refuse(ctx, 1, gn::to_string(at) + " did not answer as an account service",
           "check the address and port (the service's default is " +
               std::to_string(ga::kAccountPort) + ")");
    return false;
  }
  if (!pinned.empty() && k.key != key) {
    gn::close_transport(t);
    refuse(ctx, 1,
           gn::to_string(at) + " answered with another key than the one kept for it (" +
               gn::key_to_hex(k.key) + ")",
           "if its owner made a new key, pass --key=<the new one>; otherwise do not sign in "
           "there");
    return false;
  }
  key = k.key;
  saved.key = gn::key_to_hex(key);
  gn::Key fresh{};
  gn::os_random_bytes(fresh.data(), fresh.size());
  ga::Ask a;
  ga::ask_start(a, op, at, key, fields, fresh, since_us(t0));
  while (!a.done && since_us(t0) < until) {
    const int64_t now = since_us(t0);
    ga::ask_update(a, t, now);
    gn::Address from;
    const int n = gn::transport_receive(t, buf, sizeof buf, from, now);
    if (n > 0)
      ga::ask_read(a, buf, static_cast<size_t>(n), from);
    else
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  gn::close_transport(t);
  if (!a.answered) {
    refuse(ctx, 1, gn::to_string(at) + " did not answer the " + ga::op_name(op),
           "try again; --wait=<s> waits longer");
    return false;
  }
  reply = std::move(a.reply);
  if (!save(ctx, saved)) {
    refuse(ctx, 1, "cannot write " + file_of(ctx), "check --state");
    return false;
  }
  if (reply.refusal != ga::Refusal::none) {
    refuse(
        ctx, 1, reply.words.empty() ? "refused" : reply.words,
        reply.refusal == ga::Refusal::held ? "wait, then try again" : "check the name and secret");
    return false;
  }
  return true;
}

std::string ticket_json(const ga::Ticket& t) {
  return json_obj({{"account", json_num(static_cast<long long>(t.account))},
                   {"name", json_str(t.name)},
                   {"expires_s", json_num(static_cast<long long>(t.expires_s))}});
}

}  // namespace

int cmd_account_signup(Ctx& ctx) {
  if (!need_places(ctx)) return 1;
  if (ctx.words.size() != 1)
    return refuse(ctx, 2, "account signup takes the name",
                  std::string(ctx.spec.cli) + " account signup <name> --master=<host>");
  Saved saved = load(ctx);
  ga::Reply r;
  if (!ask(ctx, ga::Op::create, {ctx.words[0], password_of(ctx, "password")}, saved, r)) return 1;
  return answer(ctx,
                json_obj({{"account", json_num(static_cast<long long>(r.account))},
                          {"name", json_str(ctx.words[0])},
                          {"recovery_code", json_str(r.code)},
                          {"master", json_str(saved.master)}}),
                "Account " + ctx.words[0] + " made. Your recovery code, shown once: " + r.code +
                    "\nKeep it: it sets a new password if you forget yours.",
                "signed up", std::string(ctx.spec.cli) + " account code " + ctx.words[0]);
}

int cmd_account_signin(Ctx& ctx) {
  if (!need_places(ctx)) return 1;
  if (ctx.words.size() != 1)
    return refuse(ctx, 2, "account signin takes the name",
                  std::string(ctx.spec.cli) + " account signin <name>");
  Saved saved = load(ctx);
  ga::Reply r;
  if (!ask(ctx, ga::Op::sign_in, {ctx.words[0], password_of(ctx, "password")}, saved, r)) return 1;
  gn::Key key{};
  std::string error;
  gn::key_from_hex(saved.key, key, error);
  ga::Ticket t;
  if (ga::ticket_read(r.ticket.data(), r.ticket.size(), key, unix_s(), t) != ga::TicketCheck::ok)
    return refuse(ctx, 1, "the service's ticket did not check against its key",
                  "the service's clock may be far off; tell its owner");
  saved.ticket = hex(r.ticket);
  saved.name = t.name;
  if (!save(ctx, saved)) return refuse(ctx, 1, "cannot write " + file_of(ctx), "check --state");
  return answer(ctx, ticket_json(t),
                "Signed in as " + t.name + "; the ticket is kept for 24 hours.", "signed in");
}

int cmd_account_code(Ctx& ctx) {
  if (!need_places(ctx)) return 1;
  Saved saved = load(ctx);
  // Signed in already (a ticket kept, its name the one asked for or none named): the ticket buys
  // the code and no password is asked.
  const bool by_ticket = !ctx.has("password") && !saved.ticket.empty() &&
                         (ctx.words.empty() || (ctx.words.size() == 1 &&
                                                ga::name_key(ga::kEveryGame, ctx.words[0]) ==
                                                    ga::name_key(ga::kEveryGame, saved.name))) &&
                         (!ctx.has("master") || ctx.flag("master") == saved.master);
  if (!by_ticket && ctx.words.size() != 1)
    return refuse(ctx, 2, "account code takes the name, unless you signed in already",
                  std::string(ctx.spec.cli) + " account code <name>");
  const std::string name = by_ticket ? saved.name : ctx.words[0];
  ga::Reply r;
  if (by_ticket) {
    const std::vector<uint8_t> bytes = unhex(saved.ticket);
    if (!ask(ctx, ga::Op::ticket_code, {std::string(bytes.begin(), bytes.end())}, saved, r))
      return 1;
  } else if (!ask(ctx, ga::Op::code, {name, password_of(ctx, "password")}, saved, r)) {
    return 1;
  }
  return answer(ctx,
                json_obj({{"code", json_str(r.code)},
                          {"minutes", json_num(15)},
                          {"by", json_str(by_ticket ? "ticket" : "password")}}),
                "Your login code: " + r.code + "\nIn a classic client's console type: login " +
                    name + " " + r.code + "\nIt works once, within 15 minutes.",
                "a login code");
}

int cmd_account_recover(Ctx& ctx) {
  if (!need_places(ctx)) return 1;
  if (ctx.words.size() != 2)
    return refuse(ctx, 2, "account recover takes the name and the recovery code",
                  std::string(ctx.spec.cli) +
                      " account recover <name> <recovery code> --password=<the new one>");
  Saved saved = load(ctx);
  ga::Reply r;
  if (!ask(ctx, ga::Op::recover, {ctx.words[0], ctx.words[1], password_of(ctx, "password")}, saved,
           r))
    return 1;
  return answer(ctx, json_obj({{"recovery_code", json_str(r.code)}}),
                "Password set. Your new recovery code, shown once: " + r.code, "recovered");
}

int cmd_account_show(Ctx& ctx) {
  if (!need_places(ctx)) return 1;
  const Saved s = load(ctx);
  std::string text = s.master.empty() ? "No account service used yet.\n"
                                      : "Service " + s.master + ", key " + s.key + "\n";
  std::string ticket = "null";
  if (!s.ticket.empty()) {
    gn::Key key{};
    std::string error;
    gn::key_from_hex(s.key, key, error);
    const std::vector<uint8_t> bytes = unhex(s.ticket);
    ga::Ticket t;
    const ga::TicketCheck c = ga::ticket_read(bytes.data(), bytes.size(), key, unix_s(), t);
    ticket = c == ga::TicketCheck::ok ? ticket_json(t) : json_str(ga::ticket_check_name(c));
    text += c == ga::TicketCheck::ok
                ? "Signed in as " + t.name + "\n"
                : "The kept ticket is " + std::string(ga::ticket_check_name(c)) + "\n";
  }
  return answer(
      ctx, json_obj({{"master", json_str(s.master)}, {"key", json_str(s.key)}, {"ticket", ticket}}),
      text, "the account file");
}

// THE FRIENDS' COMMANDS: each asked with the ticket `account signin` kept, never a password.
namespace {

// The kept ticket's bytes, or a refusal that says to sign in.
bool ticket_field(Ctx& ctx, const Saved& saved, std::string& out) {
  if (saved.ticket.empty()) {
    refuse(ctx, 2, "not signed in", std::string(ctx.spec.cli) + " account signin <name>");
    return false;
  }
  const std::vector<uint8_t> bytes = unhex(saved.ticket);
  out.assign(bytes.begin(), bytes.end());
  return true;
}

const char* row_kind_name(ga::FriendRowKind k) {
  return k == ga::FriendRowKind::friend_    ? "friend"
         : k == ga::FriendRowKind::asks_you ? "asks_you"
                                            : "you_asked";
}

}  // namespace

int cmd_friends_list(Ctx& ctx) {
  if (!need_places(ctx)) return 1;
  Saved saved = load(ctx);
  std::string t;
  if (!ticket_field(ctx, saved, t)) return 2;
  std::vector<ga::FriendRow> all;
  bool more = true;
  for (int page = 0; more && page < 64; page++) {
    ga::Reply r;
    if (!ask(ctx, ga::Op::friends, {t, std::to_string(page)}, saved, r)) return 1;
    std::vector<ga::FriendRow> rows;
    if (!ga::friends_page_read(r.ticket, rows, more))
      return refuse(ctx, 1, "the service's friends list did not read", "try again");
    all.insert(all.end(), rows.begin(), rows.end());
  }
  std::vector<std::string> v;
  v.reserve(all.size());
  std::string text;
  for (const ga::FriendRow& f : all) {
    v.push_back(json_obj({{"name", json_str(f.name)},
                          {"kind", json_str(row_kind_name(f.kind))},
                          {"state", json_str(ga::presence_state_name(f.presence.state))},
                          {"game", json_str(f.presence.game)},
                          {"where", json_str(f.presence.where)},
                          {"words", json_str(f.presence.words)}}));
    if (f.kind == ga::FriendRowKind::asks_you) {
      text +=
          f.name + " wants to be your friend (" + ctx.spec.cli + " friends add " + f.name + ")\n";
    } else if (f.kind == ga::FriendRowKind::you_asked) {
      text += f.name + " has not answered your request yet\n";
    } else {
      text += f.name + ": " + ga::presence_state_name(f.presence.state);
      if (!f.presence.words.empty()) text += ", " + f.presence.words;
      if (!f.presence.where.empty()) text += " on " + f.presence.where;
      text += "\n";
    }
  }
  if (all.empty())
    text = "No friends yet. Add one: " + std::string(ctx.spec.cli) + " friends add <name>\n";
  return answer(ctx, json_obj({{"friends", json_arr(v)}}), text, "the friends list");
}

int cmd_friends_change(Ctx& ctx, const char* verb) {
  if (!need_places(ctx)) return 1;
  if (ctx.words.size() != 1)
    return refuse(ctx, 2, std::string("friends ") + verb + " takes a player's name",
                  std::string(ctx.spec.cli) + " friends " + verb + " <name>");
  Saved saved = load(ctx);
  std::string t;
  if (!ticket_field(ctx, saved, t)) return 2;
  static const std::map<std::string, std::string> kVerb = {
      {"add", "ask"}, {"remove", "drop"}, {"block", "block"}, {"unblock", "unblock"}};
  ga::Reply r;
  if (!ask(ctx, ga::Op::friend_, {t, kVerb.at(verb), ctx.words[0]}, saved, r)) return 1;
  return answer(ctx, json_obj({{"answer", json_str(r.code)}, {"words", json_str(r.words)}}),
                r.words, r.code.c_str());
}

int cmd_friends_add(Ctx& ctx) { return cmd_friends_change(ctx, "add"); }
int cmd_friends_remove(Ctx& ctx) { return cmd_friends_change(ctx, "remove"); }
int cmd_friends_block(Ctx& ctx) { return cmd_friends_change(ctx, "block"); }
int cmd_friends_unblock(Ctx& ctx) { return cmd_friends_change(ctx, "unblock"); }

int cmd_friends_invite(Ctx& ctx) {
  if (!need_places(ctx)) return 1;
  if (ctx.words.size() != 1 || !ctx.has("where"))
    return refuse(ctx, 2, "friends invite takes a friend's name and --where=<server address>",
                  std::string(ctx.spec.cli) + " friends invite <name> --where=<host:port>");
  Saved saved = load(ctx);
  std::string t;
  if (!ticket_field(ctx, saved, t)) return 2;
  ga::Reply r;
  if (!ask(ctx, ga::Op::invite,
           {t, ctx.words[0], ctx.spec.game, ctx.flag("where"), ctx.flag("note")}, saved, r))
    return 1;
  return answer(ctx, json_obj({{"words", json_str(r.words)}}), r.words, "invited");
}

int cmd_friends_invites(Ctx& ctx) {
  if (!need_places(ctx)) return 1;
  Saved saved = load(ctx);
  std::string t;
  if (!ticket_field(ctx, saved, t)) return 2;
  ga::Reply r;
  if (!ask(ctx, ga::Op::invites, {t}, saved, r)) return 1;
  std::vector<ga::InviteRow> inv;
  if (!ga::invites_read(r.ticket, inv))
    return refuse(ctx, 1, "the service's invites did not read", "try again");
  std::vector<std::string> v;
  v.reserve(inv.size());
  std::string text;
  for (const ga::InviteRow& i : inv) {
    v.push_back(json_obj({{"from", json_str(i.from)},
                          {"game", json_str(i.game)},
                          {"where", json_str(i.where)},
                          {"note", json_str(i.note)},
                          {"age_s", json_num(i.age_s)}}));
    text += i.from + " invites you to " + i.where + (i.note.empty() ? "" : ": " + i.note) +
            " (join: " + ctx.spec.cli + " join " + i.where + ")\n";
  }
  if (inv.empty()) text = "No invites waiting.\n";
  return answer(ctx, json_obj({{"invites", json_arr(v)}}), text, "the invites");
}

int cmd_friends_status(Ctx& ctx) {
  if (!need_places(ctx)) return 1;
  ga::PresenceState st;
  if (ctx.words.size() != 1 || !ga::presence_state_from(ctx.words[0], st))
    return refuse(ctx, 2, "friends status takes offline, online, lobby or playing",
                  std::string(ctx.spec.cli) + " friends status playing --where=<host:port>");
  Saved saved = load(ctx);
  std::string t;
  if (!ticket_field(ctx, saved, t)) return 2;
  ga::Reply r;
  if (!ask(ctx, ga::Op::presence,
           {t, ctx.words[0], ctx.spec.game, ctx.flag("where"), ctx.flag("words")}, saved, r))
    return 1;
  return answer(ctx, json_obj({{"state", json_str(ctx.words[0])}}),
                std::string("Your friends now see you as ") + ctx.words[0] + ".", "status set");
}

}  // namespace ge::gamecli
