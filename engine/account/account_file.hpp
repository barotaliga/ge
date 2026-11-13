// A PLAYER'S ACCOUNT FILE: what a player's own programs keep about their account
// (docs/accounts.md § From our own client): the master's address as the player wrote it, its key
// (learned at the first contact, then checked), and the last ticket with its name. The game CLI's
// `account` group writes it (`<state>/account.txt`, readable by its owner alone); a game's own
// client reads it to log itself in on a server that uses the same master. One `key value` line
// each:
//   master play.example.net:27901
//   key    <64 hex digits>
//   name   Niclas
//   ticket <the ticket's bytes in hex>
//
//   account_file_read(path, out) -> false when there is no file
//   account_file_write(path, file) -> false when it cannot be written
//
// ge:summary A player's account file: the master, its key and the last ticket, read and written
// ge:keywords account-file account-txt ticket master-key login-state
// ge:skill net
// ge:test tests/account_file_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ge::account {

struct AccountFile {
  std::string master;  // as the player wrote it
  std::string key;     // 64 hex digits
  std::string name;    // whose ticket
  std::string ticket;  // the last ticket's bytes in hex
};

bool account_file_read(const std::string& path, AccountFile& out);
// Written whole and made readable by its owner alone; the folder is made first.
bool account_file_write(const std::string& path, const AccountFile& f);

std::string bytes_to_hex(const std::vector<uint8_t>& bytes);
// Empty when `hex` is not an even run of lower-case hex digits.
std::vector<uint8_t> hex_to_bytes(const std::string& hex);

}  // namespace ge::account
