// THE ACCOUNT STORE a master keeps (docs/accounts.md § The account): every account, the names
// they reserve, the hashes of their secrets, the one-time codes they made, and the throttle on
// wrong secrets. A store is plain data and free functions; the caller passes the time
// (microseconds since the Unix epoch) and FRESH bytes (32, from the operating system in a real
// master, a seeded stream in a lab), so the same calls give the same store everywhere.
//
// THE SECRETS. Nothing is kept in the clear:
//   - the PASSWORD (8 to 128 bytes) signs in from our own clients and the hub's Account page, over
//     a sealed channel; Argon2id, at the store's cost (`HashCost`) when it was set, kept with it;
//   - the GAME PASSWORD (6 to 64 bytes, no spaces), optional, logs in to game servers only, from a
//     classic client's console line in plain text; Argon2id too;
//   - a LOGIN CODE (`K7QF-29MD`: 8 characters of 32, 40 bits) is one login within 15 minutes; at
//     most 4 live a time; kept as a BLAKE2b digest;
//   - the RECOVERY CODE (16 characters, 80 bits) is shown once at sign-up and when used, and sets a
//     new password; a digest.
// A login code and the game password are taken by `login_game`, never the password; the password
// only by `sign_in`. So a line read on a classic link never opens the account.
//
// THE THROTTLE. A wrong secret holds the account, and the address it came from, for 1 s, then
// twice as long at each further failure up to 15 minutes. While held, a try is refused unread
// (no hash is spent on it) and says how long to wait. A right secret clears the account's hold.
//
// THE NAMES. An account holds its name's key (account/names.hpp, every game's folding); a removed
// account frees it 30 days after. `find_by_name` takes any spelling with the same key.
//
// THE LOG. Every change is also a line appended to `log` (store_text.hpp), which the master writes
// to its file and clears; reading the file back gives the same store.
//
//   store_init(store, cost)
//   create(store, name, password, now, fresh)          -> the account and its recovery code
//   sign_in(store, name, password, from, now)          -> the account, or why not
//   login_game(store, name, secret, from, now)         -> the same, for a code or game password
//   make_code(store, id, now, fresh)                   -> a login code
//   set_password, set_game_password, recover, set_blocked, remove
//
// ge:summary The account store: accounts, reserved names, Argon2id secrets, codes, the throttle
// ge:keywords account accounts store sign-up signup sign-in login password game-password
// ge:keywords login-code one-time-code recovery-code argon2id throttle hold brute-force block
// ge:keywords remove reserved-name create sign_in login_game make_code set_password
// ge:keywords set_game_password recover set_blocked find_by_name hash-cost refusal
// ge:skill net
// ge:test tests/account_store_test.cpp
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ge::account {

using Hash32 = std::array<uint8_t, 32>;
using Fresh = std::array<uint8_t, 32>;  // new random bytes for one call

// What a slow hash costs: KiB of memory and passes over it. A master's default is 64 MiB and 3
// passes; a test or a lab passes a small one.
struct HashCost {
  uint32_t kib = 65536;
  uint32_t passes = 3;
};

struct SecretHash {
  bool set = false;
  uint32_t kib = 0, passes = 0;  // the cost it was made at, so a store's new cost reads it still
  std::array<uint8_t, 16> salt{};
  Hash32 hash{};
};

struct LoginCode {
  Hash32 digest{};
  int64_t expires_us = 0;
};

struct Account {
  uint64_t id = 0;
  std::string name, key;  // as written; the reservation's key (name_key with kEveryGame)
  SecretHash password, game_password;
  Hash32 recovery{};
  std::vector<LoginCode> codes;
  int64_t created_s = 0, seen_s = 0;
  int64_t removed_s = 0;  // 0: alive; else when, and the name is free 30 days after
  bool blocked = false;
};

struct Hold {
  uint32_t failures = 0;
  int64_t until_us = 0;
};

struct Store {
  HashCost cost;
  std::vector<Account> accounts;                   // by id - 1
  std::unordered_map<std::string, uint64_t> keys;  // a reserved key to its account's id
  std::unordered_map<uint64_t, Hold> account_holds;
  std::unordered_map<std::string, Hold> address_holds;
  std::vector<uint8_t> work;  // the slow hash's memory, kept between calls
  std::string log;            // the lines since the master last wrote them (store_text.hpp)
};

enum class Refusal : uint8_t {
  none,
  bad_name,       // not a name an account may have
  name_taken,     // an account holds its key
  bad_secret,     // a password or game password of the wrong length or with a space
  unknown,        // no such account (said to a player as `wrong`: a name is not confirmed)
  wrong,          // the secret does not match
  held,           // too many wrong secrets: wait
  blocked,        // the master's owner blocked it
  too_many_codes  // 4 codes live already
};

struct Answer {
  Refusal refusal = Refusal::none;
  uint64_t id = 0;
  int64_t wait_us = 0;  // with `held`
  std::string code;     // a new login or recovery code, when the call makes one
  std::string words;    // what to tell the player: "" when it went through
};

constexpr int64_t kCodeLife_us = 15ll * 60 * 1000000;
constexpr int64_t kHoldMax_us = 15ll * 60 * 1000000;
constexpr int64_t kNameFreeAfter_s = 30ll * 24 * 3600;
constexpr size_t kCodesLive = 4;

void store_init(Store& s, HashCost cost);

Answer create(Store& s, std::string_view name, std::string_view password, int64_t now_us,
              const Fresh& fresh);
Answer sign_in(Store& s, std::string_view name, std::string_view password, std::string_view from,
               int64_t now_us);
Answer login_game(Store& s, std::string_view name, std::string_view secret, std::string_view from,
                  int64_t now_us);
Answer make_code(Store& s, uint64_t id, int64_t now_us, const Fresh& fresh);
Refusal set_password(Store& s, uint64_t id, std::string_view password, const Fresh& fresh);
// An empty `secret` clears the game password.
Refusal set_game_password(Store& s, uint64_t id, std::string_view secret, const Fresh& fresh);
// The recovery code sets a new password and gives a new recovery code (in `code`).
Answer recover(Store& s, std::string_view name, std::string_view recovery_code,
               std::string_view password, std::string_view from, int64_t now_us,
               const Fresh& fresh);
Refusal set_blocked(Store& s, uint64_t id, bool blocked);
Refusal remove(Store& s, uint64_t id, int64_t now_us);

// The live account whose name has `name`'s key, or nullptr.
const Account* find_by_name(const Store& s, std::string_view name);
const Account* find(const Store& s, uint64_t id);

// What a refusal says to a player, in a game's words.
std::string refusal_words(Refusal r, int64_t wait_us);

}  // namespace ge::account
