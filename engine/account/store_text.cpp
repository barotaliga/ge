// The account store as text (account/store_text.hpp): the lines written and read back.
//
// ge:summary The account store's lines: written one per change, read back into a store
// ge:keywords account store log snapshot text parse hex
// ge:skill net
// ge:test tests/account_store_test.cpp
#include "account/store_text.hpp"

#include <cstdlib>
#include <vector>

#include "account/names.hpp"

namespace ge::account {

namespace {

std::string hex(const uint8_t* bytes, size_t n) {
  static const char kDigits[] = "0123456789abcdef";
  std::string out;
  out.reserve(n * 2);
  for (size_t i = 0; i < n; i++) {
    out.push_back(kDigits[bytes[i] >> 4]);
    out.push_back(kDigits[bytes[i] & 15]);
  }
  return out;
}

bool unhex(std::string_view text, uint8_t* out, size_t n) {
  if (text.size() != n * 2) return false;
  for (size_t i = 0; i < n * 2; i++) {
    const char c = text[i];
    int v = -1;
    if (c >= '0' && c <= '9') v = c - '0';
    if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
    if (v < 0) return false;
    if (i % 2 == 0)
      out[i / 2] = static_cast<uint8_t>(v << 4);
    else
      out[i / 2] = static_cast<uint8_t>(out[i / 2] | v);
  }
  return true;
}

bool number(std::string_view text, int64_t& out) {
  if (text.empty() || text.size() > 19) return false;
  int64_t v = 0;
  for (const char c : text) {
    if (c < '0' || c > '9') return false;
    v = v * 10 + (c - '0');
  }
  out = v;
  return true;
}

std::vector<std::string_view> words(std::string_view line) {
  std::vector<std::string_view> out;
  size_t at = 0;
  while (at <= line.size()) {
    const size_t sp = line.find(' ', at);
    const size_t end = sp == std::string_view::npos ? line.size() : sp;
    out.push_back(line.substr(at, end - at));
    at = end + 1;
  }
  return out;
}

bool read_secret(const std::vector<std::string_view>& w, SecretHash& h) {
  if (w.size() == 3 && w[2] == "-") {
    h = SecretHash{};
    return true;
  }
  int64_t kib = 0, passes = 0;
  if (w.size() != 6 || !number(w[2], kib) || !number(w[3], passes)) return false;
  h.set = true;
  h.kib = static_cast<uint32_t>(kib);
  h.passes = static_cast<uint32_t>(passes);
  return unhex(w[4], h.salt.data(), h.salt.size()) && unhex(w[5], h.hash.data(), h.hash.size());
}

// One line applied; "" when it went in, else what was wrong with it.
std::string apply(Store& s, std::string_view line) {
  const auto w = words(line);
  if (w.size() < 3) return "too few words";
  int64_t id = 0;
  if (!number(w[1], id) || id < 1) return "the account's number is not a number from 1";
  const auto at = static_cast<uint64_t>(id);
  if (w[0] == "account") {
    int64_t created = 0;
    if (w.size() != 4 || !number(w[3], created)) return "want: account <id> <name> <created_s>";
    if (at != s.accounts.size() + 1) return "accounts must come in order, each one new";
    Account a;
    a.id = at;
    a.name = std::string(w[2]);
    a.key = name_key(kEveryGame, a.name);
    a.created_s = a.seen_s = created;
    s.keys[a.key] = at;
    s.accounts.push_back(std::move(a));
    return "";
  }
  if (at > s.accounts.size()) return "names an account no earlier line created";
  Account& a = s.accounts[at - 1];
  if (w[0] == "password") {
    if (!read_secret(w, a.password)) {
      return "want: password <id> <kib> <passes> <salt hex> <hash hex>";
    }
    return "";
  }
  if (w[0] == "recovery") {
    return w.size() == 3 && unhex(w[2], a.recovery.data(), 32) ? "" : "want: recovery <id> <hex>";
  }
  if (w[0] == "code") {
    LoginCode c;
    if (w.size() != 4 || !number(w[2], c.expires_us) || !unhex(w[3], c.digest.data(), 32)) {
      return "want: code <id> <expires_us> <hex>";
    }
    a.codes.push_back(c);
    return "";
  }
  if (w[0] == "spent") {
    Hash32 d{};
    if (w.size() != 3 || !unhex(w[2], d.data(), 32)) return "want: spent <id> <hex>";
    for (size_t i = 0; i < a.codes.size(); i++) {
      if (a.codes[i].digest == d) {
        a.codes.erase(a.codes.begin() + static_cast<long>(i));
        break;
      }
    }
    return "";
  }
  int64_t v = 0;
  if (w.size() != 3 || !number(w[2], v)) return "want: " + std::string(w[0]) + " <id> <number>";
  if (w[0] == "blocked")
    a.blocked = v != 0;
  else if (w[0] == "seen")
    a.seen_s = v;
  else if (w[0] == "removed") {
    a.removed_s = v;
    a.codes.clear();
    a.password = SecretHash{};
  } else {
    return "an unknown word: " + std::string(w[0]);
  }
  return "";
}

}  // namespace

std::string line_account(const Account& a) {
  return "account " + std::to_string(a.id) + " " + a.name + " " + std::to_string(a.created_s) +
         "\n";
}

std::string line_secret(const char* kind, uint64_t id, const SecretHash& h) {
  std::string out = std::string(kind) + " " + std::to_string(id) + " ";
  if (!h.set) return out + "-\n";
  return out + std::to_string(h.kib) + " " + std::to_string(h.passes) + " " +
         hex(h.salt.data(), h.salt.size()) + " " + hex(h.hash.data(), h.hash.size()) + "\n";
}

std::string line_digest(const char* kind, uint64_t id, const Hash32& d) {
  return std::string(kind) + " " + std::to_string(id) + " " + hex(d.data(), d.size()) + "\n";
}

std::string line_code(uint64_t id, const LoginCode& c) {
  return "code " + std::to_string(id) + " " + std::to_string(c.expires_us) + " " +
         hex(c.digest.data(), c.digest.size()) + "\n";
}

std::string line_number(const char* kind, uint64_t id, int64_t value) {
  return std::string(kind) + " " + std::to_string(id) + " " + std::to_string(value) + "\n";
}

std::string store_text(const Store& s, int64_t now_us) {
  std::string out;
  for (const Account& a : s.accounts) {
    out += line_account(a);
    if (a.removed_s) {
      out += line_number("removed", a.id, a.removed_s);
      continue;
    }
    out += line_secret("password", a.id, a.password);
    out += line_digest("recovery", a.id, a.recovery);
    for (const LoginCode& c : a.codes) {
      if (c.expires_us > now_us) out += line_code(a.id, c);
    }
    if (a.blocked) out += line_number("blocked", a.id, 1);
    if (a.seen_s != a.created_s) out += line_number("seen", a.id, a.seen_s);
  }
  return out;
}

bool store_read(Store& s, std::string_view text, std::string& error) {
  size_t at = 0;
  int number_of_line = 0;
  while (at < text.size()) {
    size_t end = text.find('\n', at);
    if (end == std::string_view::npos) end = text.size();
    std::string_view line = text.substr(at, end - at);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    number_of_line++;
    at = end + 1;
    if (line.empty()) continue;
    const std::string why = apply(s, line);
    if (!why.empty()) {
      error = "accounts line " + std::to_string(number_of_line) + ": " + why +
              "; next: restore the file from its snapshot, or remove the line and the ones after";
      return false;
    }
  }
  return true;
}

}  // namespace ge::account
