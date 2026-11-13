// LOGINS ON A GAME SERVER (docs/accounts.md § Logging in): the seam every game's server logs its
// players in through, whatever its protocol. The game tells it who sits where and hands it the
// lines its players type; it answers with what the game must do (`LoginOut`: tell a player a
// line, tell everyone, set a seat's name, set its login key, drop it), so the game keeps its own
// wire and words for the rest.
//
// WHO CHECKS THE CODE. A server whose `sv_accountmaster` is empty is its OWN master: its service
// (account/service.hpp, hosted by the dedicated process over `accounts.log` in its game folder)
// answers the login in process (`own`). Otherwise the server asks the master at `sv_accountmaster`
// over its own socket (`link`), sealed to `sv_accountkey` (learned at the first answer when unset,
// and printed for the owner to set), and checks the ticket the master signs before it believes it.
//
// A CLASSIC CLIENT logs in from the console: `login <name> <code>` (sent as a command, or as chat
// by a client that sends a line it does not know as chat: `say login ...`, quoted or not), or the
// two-step form: `login <name>`, then the code alone on the next line. The code is a ONE-TIME
// code from the hub's Account page or the game CLI (`account code`): the classic protocols carry
// the line in plain text, so nothing the line holds is any use twice. A login line is never shown
// as chat, and the game takes it before its rules or any recording sees it.
//
// `sv_login` 0 asks nobody (a login still counts); 1 asks every player, a spectator may watch as a
// guest; 2 asks everyone. A seat that must log in is told how on joining (after
// `sv_loginwelcome`), sends no chat until it has, and is dropped after `sv_logintime` seconds.
// Logged in, the seat's login key is the account's name and, with `sv_loginname 1`, so is its
// name, which it may not change. `sv_reservednames 1`: a guest whose name folds (account/names.hpp,
// the game's `NameRules`) to an account's on the server's own store, or to a seat's logged in
// here, plays under the game's default name; a name logged in on another seat is refused.
//
// Every line a player reads is a game's words (docs/architecture.md § Player text).
//
// ge:summary Logins on a game server: settings, the console login line, own or remote master
// ge:keywords login logins sv_login sv_logintime sv_loginname sv_loginwelcome sv_accountmaster
// ge:keywords sv_accountkey sv_reservednames sv_accountport login-code one-time-code
// ge:keywords classic-login reserved-names auth-key login-prompt own-master account-master
// ge:skill net
// ge:test tests/server_login_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "account/names.hpp"
#include "account/service.hpp"
#include "net/address.hpp"
#include "net/transport.hpp"

namespace ge::server {

constexpr int kLoginSeats = 32;

// The server's login settings (ServerConfig's sv_login... fields, copied).
struct LoginSettings {
  std::string welcome;   // sv_loginwelcome: the line before the prompt
  std::string master;    // sv_accountmaster: "" the server is its own master
  std::string key;       // sv_accountkey: the master's public key, 64 hex digits
  double time_s = 60;    // sv_logintime: a seat that must log in has this long
  int32_t login = 0;     // sv_login: 0 none asked, 1 players, 2 everyone
  int32_t name = 1;      // sv_loginname: 1 a logged-in seat is shown by its account's name
  int32_t reserved = 1;  // sv_reservednames: 1 guests may not use an account's name
  int32_t port = 0;      // sv_accountport: the own master's service port (0: kAccountPort)
};

enum class LoginState : uint8_t { empty, guest, code_next, asking, in };

struct LoginSeat {
  LoginState state = LoginState::empty;
  int userid = 0;  // the game's id of whoever sits here (another id: another player)
  bool spectator = false;
  bool must = false;      // has to log in, by sv_login
  bool warned = false;    // told its time is nearly up
  double deadline_s = 0;  // when a seat that must log in is dropped (the game's clock)
  std::string name;       // the name typed (code_next, asking) or the account's (in)
  std::string address;    // the player's address, for the throttle
  uint64_t account = 0;
  std::string renamed;  // a reserved name the seat asked for while connecting, told on joining
  std::string shown;    // the name the seat was last let go by (a master elsewhere checks it)
  account::Ask ask;     // to a master elsewhere
};

// A name asked of a master elsewhere (`Op::name`): whether an account reserves it. Kept while the
// server runs (at most kNameChecks, the oldest answered dropped), so a name is asked once.
struct NameCheck {
  std::string key;  // the name folded by the game's rules
  account::Ask ask;
  bool answered = false, reserved = false;
};
inline constexpr size_t kNameChecks = 256;

struct LoginOut {
  // unname: a guest's name turned out registered; the game names the seat its default name
  enum class Kind : uint8_t { tell, tell_all, name, auth, drop, unname } kind = Kind::tell;
  int slot = -1;
  std::string text;  // the line (no newline), the name or login, or the drop's reason
};

struct LoginStats {
  uint64_t lines = 0, asked = 0, in = 0, refused = 0, unanswered = 0, bad_tickets = 0;
  uint64_t timed_out = 0, chat_held = 0, renamed = 0, names_asked = 0;
};

struct Logins {
  LoginSettings set;
  account::NameRules rules;         // how the game's names fold
  account::Service* own = nullptr;  // the server is its own master
  net::Transport* link = nullptr;   // the socket asks to a master elsewhere go on
  net::Address master;              // that master
  net::Key master_key{};            // its key (`key_known` once set or learned)
  bool key_known = false;
  account::KeyAsk key_ask;           // learning it, when sv_accountkey is unset
  account::FreshFn fresh = nullptr;  // an ask's exchange secret
  void* fresh_ctx = nullptr;
  int64_t now_us = 0;  // the Unix clock at the last frame or line (a name's ask starts on it)
  LoginSeat seats[kLoginSeats];
  std::vector<NameCheck> names;    // names asked of a master elsewhere
  std::vector<LoginOut> out;       // what the game must do, in order; it empties it
  std::vector<std::string> notes;  // lines for the server's own log (a key learned)
  LoginStats stats;
  uint8_t buffer[net::kMaxDatagram];
};

// The logins over `own` (the server's own service) or, when `set.master` names one, the master
// there asked over `link`. False with `error` (what and what to set) when the master's address or
// key does not read, or neither is given while logins are asked.
bool logins_open(Logins& l, const LoginSettings& set, const account::NameRules& rules,
                 account::Service* own, net::Transport* link, account::FreshFn fresh,
                 void* fresh_ctx, int64_t unix_us, std::string& error);
bool logins_active(const Logins& l);  // a service to ask: own or a master

// A seat taken by `userid` (a new player, or the same one again: nothing). Tells it how to log in
// when it must, or when logins are asked of players and it may.
void login_seat(Logins& l, int slot, int userid, bool spectator, std::string_view address,
                double now_s);
// The seat let go (the player left).
void login_leave(Logins& l, int slot);

// A line the player at `slot` sent (a command, or `say ...`). True when it was the login's: a
// login, the code after a two-step login, or chat from a seat that must log in first; the game
// drops it then (nobody sees it, nothing records it).
bool login_line(Logins& l, int slot, std::string_view line, double now_s, int64_t unix_us);

// Every frame: the masters' answers read, the asks sent again, the seats out of time dropped.
void logins_frame(Logins& l, double now_s, int64_t unix_us);

// What a seat may be called.
enum class NameVerdict : uint8_t {
  free,      // the name as wanted
  reserved,  // an account's (own store) or another seat's login: the game's default name instead
  account    // the seat is logged in with sv_loginname 1: `name` is set to its account's
};
// The name the player at `slot` wants (on joining or renaming). A refusal tells the player why,
// unless `quiet` (a seat still connecting, which reads nothing yet). With a master elsewhere a name
// not asked before is let go while the master is asked; if it answers that an account holds it,
// the seat is told and an `unname` order renames it.
NameVerdict login_name(Logins& l, int slot, std::string& name, bool quiet = false);

// The account the seat is logged in to (0: a guest).
uint64_t login_account(const Logins& l, int slot);

// The key the server's tickets are signed with: its own service's, or its master's once known.
// A game advertises its tag (the first 16 hex digits) so a player's own client, holding a ticket
// signed by the same key, knows it can log itself in (docs/accounts.md § From our own client).
bool login_key(const Logins& l, net::Key& out);
std::string login_key_tag(const net::Key& key);

}  // namespace ge::server
