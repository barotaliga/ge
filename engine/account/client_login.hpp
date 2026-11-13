// A PLAYER'S OWN CLIENT LOGGING ITSELF IN (docs/accounts.md § From our own client): the account
// file the game CLI keeps (account_file.hpp) holds the master's address, its key and the last
// ticket. A server that uses the same master says so (its tag: the first 16 hex digits of the key
// its tickets are signed with; a classic server puts it in its server info as `*accountkey`).
// The client then trades its ticket with the master for a one-time login code (`Op::ticket_code`,
// sealed to the key it holds, over a socket of its own) and sends the server the login line a
// player would type. The ticket never goes to the game server, and the code works once.
//
// Nothing here blocks or reads a clock: the caller passes the time, the fresh key and the socket.
//
//   client_login_load(c, path, unix_s, why) -> false when there is nothing to log in with
//   client_login_fits(c, tag)               -> the server's master is the file's
//   client_login_ask(c, fresh, now_us)      then client_login_update(c, t, now_us) each frame
//
// ge:summary Our own client logging itself in: the saved ticket traded for a one-time code
// ge:keywords client-login autologin accountkey ticket-code login-code account-file
// ge:skill net
// ge:test tests/account_client_login_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "account/account_file.hpp"
#include "account/service.hpp"
#include "net/address.hpp"
#include "net/transport.hpp"

namespace ge::account {

enum class ClientLoginStep : uint8_t {
  idle,     // loaded, nothing asked
  asking,   // the ticket sent to the master for a code
  code,     // `code` holds the login code: send `login <name> <code>`
  refused,  // the master said no (`words` says why, in a player's words)
  silent    // the master did not answer
};

struct ClientLogin {
  AccountFile file;
  net::Address master;
  net::Key key{};
  std::string tag;  // the key's first 16 hex digits, as a server advertises it
  std::vector<uint8_t> ticket;
  Ask ask;
  std::string code, words;
  ClientLoginStep step = ClientLoginStep::idle;
};

// The file at `path` read: false with `why` (a line for the player's console) when there is no
// file, no ticket in it, or the ticket has run out or does not read against the file's key.
bool client_login_load(ClientLogin& c, const std::string& path, int64_t unix_s, std::string& why);
// True when `tag` (a server's advertised key) is the file's master's.
bool client_login_fits(const ClientLogin& c, std::string_view tag);
// Asks the master for a login code with the ticket.
void client_login_ask(ClientLogin& c, const net::Key& fresh, int64_t now_us);
// Sends the ask again when due and reads what came on `t` (the client's own socket for it).
// True once the ask has ended (`step` is code, refused or silent).
bool client_login_update(ClientLogin& c, net::Transport& t, int64_t now_us);

}  // namespace ge::account
