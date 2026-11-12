// The account store (account/store.hpp): creating accounts, the secrets checked under the
// throttle, the codes, and every change logged as a line (account/store_text.hpp).
//
// ge:summary The account store's calls: create, sign in, game login, codes, recovery, the throttle
// ge:keywords account store create sign_in login_game make_code recover throttle hold argon2id
// ge:skill net
// ge:test tests/account_store_test.cpp
#include "account/store.hpp"

#include <cstdio>

#include "account/names.hpp"
#include "account/store_text.hpp"
#include "net/crypto.hpp"

namespace ge::account {

namespace {

constexpr char kAlphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";  // 32, no I L O U

// `chars` characters of the alphabet from the bits of `bytes`, in groups of four with dashes.
std::string make_words(const uint8_t* bytes, int chars) {
  std::string out;
  uint32_t acc = 0;
  int bits = 0, n = 0;
  for (int i = 0; n < chars; i++) {
    acc = (acc << 8) | bytes[i];
    bits += 8;
    while (bits >= 5 && n < chars) {
      bits -= 5;
      if (n > 0 && n % 4 == 0) out.push_back('-');
      out.push_back(kAlphabet[(acc >> bits) & 31]);
      n++;
    }
  }
  return out;
}

// A code as a player typed it, made canonical: dashes and spaces gone, upper case, and the
// letters people mistake for digits read as those digits.
std::string canonical_code(std::string_view typed) {
  std::string out;
  for (char c : typed) {
    if (c == '-' || c == ' ') continue;
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    if (c == 'O') c = '0';
    if (c == 'I' || c == 'L') c = '1';
    out.push_back(c);
  }
  return out;
}

Hash32 code_digest(const char* kind, uint64_t id, std::string_view typed) {
  return net::digest(std::string(kind) + ":" + std::to_string(id) + ":" + canonical_code(typed));
}

bool same(const Hash32& a, const Hash32& b) {
  uint8_t diff = 0;  // every byte read whatever the first difference: no time says where it was
  for (size_t i = 0; i < a.size(); i++) diff |= static_cast<uint8_t>(a[i] ^ b[i]);
  return diff == 0;
}

void hash_secret(Store& s, SecretHash& h, std::string_view secret, const uint8_t* salt) {
  h.set = true;
  h.kib = s.cost.kib;
  h.passes = s.cost.passes;
  for (size_t i = 0; i < h.salt.size(); i++) h.salt[i] = salt[i];
  if (s.work.size() < size_t{h.kib} * 1024) s.work.resize(size_t{h.kib} * 1024);
  net::password_hash(h.hash.data(), secret, h.salt.data(), h.kib, h.passes, s.work.data());
}

bool secret_matches(Store& s, const SecretHash& h, std::string_view secret) {
  if (!h.set) return false;
  if (s.work.size() < size_t{h.kib} * 1024) s.work.resize(size_t{h.kib} * 1024);
  Hash32 got{};
  net::password_hash(got.data(), secret, h.salt.data(), h.kib, h.passes, s.work.data());
  return same(got, h.hash);
}

bool password_ok(std::string_view p) { return p.size() >= 8 && p.size() <= 128; }

bool game_password_ok(std::string_view p) {
  if (p.size() < 6 || p.size() > 64) return false;
  for (const char c : p) {
    if (c == ' ' || c == '\t' || static_cast<uint8_t>(c) < 0x20) return false;
  }
  return true;
}

Account* live(Store& s, uint64_t id) {
  if (id == 0 || id > s.accounts.size()) return nullptr;
  Account& a = s.accounts[id - 1];
  return a.removed_s ? nullptr : &a;
}

Account* live_by_name(Store& s, std::string_view name) {
  const auto it = s.keys.find(name_key(kEveryGame, name));
  return it == s.keys.end() ? nullptr : live(s, it->second);
}

int64_t held_for(const std::unordered_map<std::string, Hold>& m, const std::string& k,
                 int64_t now) {
  const auto it = m.find(k);
  return it != m.end() && it->second.until_us > now ? it->second.until_us - now : 0;
}

int64_t held_for(const std::unordered_map<uint64_t, Hold>& m, uint64_t k, int64_t now) {
  const auto it = m.find(k);
  return it != m.end() && it->second.until_us > now ? it->second.until_us - now : 0;
}

void fail(Hold& h, int64_t now) {
  int64_t hold = 1000000;
  for (uint32_t i = 0; i < h.failures && hold < kHoldMax_us; i++) hold *= 2;
  if (hold > kHoldMax_us) hold = kHoldMax_us;
  h.failures++;
  h.until_us = now + hold;
}

Answer refuse(Refusal r, int64_t wait_us = 0) {
  Answer a;
  a.refusal = r;
  a.wait_us = wait_us;
  a.words = refusal_words(r, wait_us);
  return a;
}

// The throttle round a check: refused unread while held; a failure lengthens both holds.
template <typename Check>
Answer checked(Store& s, std::string_view name, std::string_view from, int64_t now, Check check) {
  const std::string address(from);
  const int64_t by_address = held_for(s.address_holds, address, now);
  if (by_address) return refuse(Refusal::held, by_address);
  Account* a = live_by_name(s, name);
  if (a) {
    const int64_t by_account = held_for(s.account_holds, a->id, now);
    if (by_account) return refuse(Refusal::held, by_account);
    if (a->blocked) return refuse(Refusal::blocked);
  }
  if (!a || !check(*a)) {
    fail(s.address_holds[address], now);
    if (a) fail(s.account_holds[a->id], now);
    return refuse(a ? Refusal::wrong : Refusal::unknown);
  }
  s.account_holds.erase(a->id);
  a->seen_s = now / 1000000;
  s.log += line_number("seen", a->id, a->seen_s);
  Answer ok;
  ok.id = a->id;
  return ok;
}

}  // namespace

void store_init(Store& s, HashCost cost) {
  s = Store{};
  s.cost = cost;
  s.work.resize(size_t{cost.kib < 8 ? 8 : cost.kib} * 1024);
}

Answer create(Store& s, std::string_view name, std::string_view password, int64_t now_us,
              const Fresh& fresh) {
  const std::string why = check_account_name(name);
  if (!why.empty()) {
    Answer a = refuse(Refusal::bad_name);
    a.words = why;
    return a;
  }
  if (!password_ok(password)) return refuse(Refusal::bad_secret);
  const std::string key = name_key(kEveryGame, name);
  const auto it = s.keys.find(key);
  if (it != s.keys.end()) {
    const Account& held = s.accounts[it->second - 1];
    if (!held.removed_s || now_us / 1000000 < held.removed_s + kNameFreeAfter_s) {
      return refuse(Refusal::name_taken);
    }
  }
  Account a;
  a.id = s.accounts.size() + 1;
  a.name = std::string(name);
  a.key = key;
  a.created_s = a.seen_s = now_us / 1000000;
  hash_secret(s, a.password, password, fresh.data());
  Answer out;
  out.code = make_words(fresh.data() + 16, 16);
  a.recovery = code_digest("recovery", a.id, out.code);
  out.id = a.id;
  s.keys[key] = a.id;
  s.log += line_account(a) + line_secret("password", a.id, a.password) +
           line_digest("recovery", a.id, a.recovery);
  s.accounts.push_back(std::move(a));
  return out;
}

Answer sign_in(Store& s, std::string_view name, std::string_view password, std::string_view from,
               int64_t now_us) {
  return checked(s, name, from, now_us,
                 [&](Account& a) { return secret_matches(s, a.password, password); });
}

Answer login_game(Store& s, std::string_view name, std::string_view secret, std::string_view from,
                  int64_t now_us) {
  return checked(s, name, from, now_us, [&](Account& a) {
    const Hash32 d = code_digest("code", a.id, secret);
    for (size_t i = 0; i < a.codes.size(); i++) {
      if (a.codes[i].expires_us > now_us && same(a.codes[i].digest, d)) {
        s.log += line_digest("spent", a.id, d);
        a.codes.erase(a.codes.begin() + static_cast<long>(i));
        return true;
      }
    }
    return secret_matches(s, a.game_password, secret);
  });
}

Answer make_code(Store& s, uint64_t id, int64_t now_us, const Fresh& fresh) {
  Account* a = live(s, id);
  if (!a) return refuse(Refusal::unknown);
  if (a->blocked) return refuse(Refusal::blocked);
  size_t kept = 0;
  for (const LoginCode& c : a->codes) {
    if (c.expires_us > now_us) a->codes[kept++] = c;
  }
  a->codes.resize(kept);
  if (kept >= kCodesLive) return refuse(Refusal::too_many_codes);
  Answer out;
  out.id = id;
  out.code = make_words(fresh.data(), 8);
  LoginCode c;
  c.digest = code_digest("code", id, out.code);
  c.expires_us = now_us + kCodeLife_us;
  a->codes.push_back(c);
  s.log += line_code(id, c);
  return out;
}

Refusal set_password(Store& s, uint64_t id, std::string_view password, const Fresh& fresh) {
  Account* a = live(s, id);
  if (!a) return Refusal::unknown;
  if (!password_ok(password)) return Refusal::bad_secret;
  hash_secret(s, a->password, password, fresh.data());
  s.log += line_secret("password", id, a->password);
  return Refusal::none;
}

Refusal set_game_password(Store& s, uint64_t id, std::string_view secret, const Fresh& fresh) {
  Account* a = live(s, id);
  if (!a) return Refusal::unknown;
  if (secret.empty()) {
    a->game_password = SecretHash{};
  } else {
    if (!game_password_ok(secret)) return Refusal::bad_secret;
    hash_secret(s, a->game_password, secret, fresh.data());
  }
  s.log += line_secret("gamepass", id, a->game_password);
  return Refusal::none;
}

Answer recover(Store& s, std::string_view name, std::string_view recovery_code,
               std::string_view password, std::string_view from, int64_t now_us,
               const Fresh& fresh) {
  if (!password_ok(password)) return refuse(Refusal::bad_secret);
  Answer a = checked(s, name, from, now_us, [&](Account& acc) {
    return same(acc.recovery, code_digest("recovery", acc.id, recovery_code));
  });
  if (a.refusal != Refusal::none) return a;
  Account* acc = live(s, a.id);
  hash_secret(s, acc->password, password, fresh.data());
  a.code = make_words(fresh.data() + 16, 16);
  acc->recovery = code_digest("recovery", acc->id, a.code);
  s.log += line_secret("password", acc->id, acc->password) +
           line_digest("recovery", acc->id, acc->recovery);
  return a;
}

Refusal set_blocked(Store& s, uint64_t id, bool blocked) {
  Account* a = live(s, id);
  if (!a) return Refusal::unknown;
  a->blocked = blocked;
  s.log += line_number("blocked", id, blocked ? 1 : 0);
  return Refusal::none;
}

Refusal remove(Store& s, uint64_t id, int64_t now_us) {
  Account* a = live(s, id);
  if (!a) return Refusal::unknown;
  a->removed_s = now_us / 1000000;
  a->codes.clear();
  a->password = a->game_password = SecretHash{};
  s.log += line_number("removed", id, a->removed_s);
  return Refusal::none;
}

const Account* find_by_name(const Store& s, std::string_view name) {
  return live_by_name(const_cast<Store&>(s), name);
}

const Account* find(const Store& s, uint64_t id) { return live(const_cast<Store&>(s), id); }

std::string refusal_words(Refusal r, int64_t wait_us) {
  switch (r) {
    case Refusal::none:
      return "";
    case Refusal::bad_name:
      return "A name is 2 to 15 letters, digits, dashes, dots or underscores, with no spaces.";
    case Refusal::name_taken:
      return "That name belongs to another player.";
    case Refusal::bad_secret:
      return "A password is 8 to 128 characters; a game password 6 to 64, with no spaces.";
    case Refusal::unknown:
    case Refusal::wrong:
      return "That name and password do not match.";
    case Refusal::held: {
      const int64_t s = (wait_us + 999999) / 1000000;
      char line[96];
      if (s >= 60) {
        std::snprintf(line, sizeof line, "Too many tries. Wait %lld minutes and try again.",
                      static_cast<long long>((s + 59) / 60));
      } else {
        std::snprintf(line, sizeof line, "Too many tries. Wait %lld seconds and try again.",
                      static_cast<long long>(s));
      }
      return line;
    }
    case Refusal::blocked:
      return "This account is blocked.";
    case Refusal::too_many_codes:
      return "You have 4 codes waiting. Use one, or wait for one to run out.";
  }
  return "";
}

}  // namespace ge::account
