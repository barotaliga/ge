// THE ACCOUNT STORE AS TEXT: the log a master appends to and the snapshot it compacts it into
// (account/store.hpp). One change a line, words split by single spaces, hashes and salts in lower
// case hex; the snapshot is the same lines, one set per account, so one reader reads both:
//
//   account <id> <name> <created_s>
//   password <id> <kib> <passes> <salt hex> <hash hex>
//   recovery <id> <digest hex>
//   code <id> <expires_us> <digest hex>                      spent <id> <digest hex>
//   blocked <id> 0|1        removed <id> <s>        seen <id> <s>
//
// Lines are applied in order; a line naming an account not yet created, or one that does not
// parse, is refused with its number. Codes past their time are left out of a snapshot. The
// throttle is not kept: a master that restarts starts every hold again at none.
//
//   store_text(store)                 -> the snapshot
//   store_read(store, text, error)    -> false, `error` naming the line, when a line is refused
//
// ge:summary The account store as text: the log's lines and the snapshot, written and read back
// ge:keywords account store log snapshot compact text lines persistence store_text store_read
// ge:keywords line_account line_secret
// ge:skill net
// ge:test tests/account_store_test.cpp
#pragma once
#include <string>
#include <string_view>

#include "account/store.hpp"

namespace ge::account {

std::string store_text(const Store& s, int64_t now_us);
bool store_read(Store& s, std::string_view text, std::string& error);

// One line each, with its newline: what the store appends to its log as it changes.
std::string line_account(const Account& a);
std::string line_secret(const char* kind, uint64_t id, const SecretHash& h);
std::string line_digest(const char* kind, uint64_t id, const Hash32& d);
std::string line_code(uint64_t id, const LoginCode& c);
std::string line_number(const char* kind, uint64_t id, int64_t value);

}  // namespace ge::account
