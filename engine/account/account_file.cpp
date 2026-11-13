// A player's account file (account/account_file.hpp).
//
// ge:summary A player's account file: the master, its key and the last ticket, read and written
// ge:keywords account-file account-txt
// ge:skill net
// ge:test tests/account_file_test.cpp
#include "account/account_file.hpp"

#include <filesystem>
#include <fstream>

namespace ge::account {

namespace fs = std::filesystem;

bool account_file_read(const std::string& path, AccountFile& out) {
  std::ifstream f(path);
  if (!f) return false;
  out = AccountFile{};
  std::string line;
  while (std::getline(f, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const size_t sp = line.find(' ');
    if (sp == std::string::npos) continue;
    const std::string k = line.substr(0, sp), v = line.substr(sp + 1);
    if (k == "master") out.master = v;
    if (k == "key") out.key = v;
    if (k == "ticket") out.ticket = v;
    if (k == "name") out.name = v;
  }
  return true;
}

bool account_file_write(const std::string& path, const AccountFile& s) {
  std::error_code ec;
  const fs::path parent = fs::path(path).parent_path();
  if (!parent.empty()) fs::create_directories(parent, ec);
  {
    std::ofstream f(path, std::ios::trunc);
    if (!f) return false;
    f << "master " << s.master << "\nkey " << s.key << "\n";
    if (!s.ticket.empty()) f << "name " << s.name << "\nticket " << s.ticket << "\n";
    if (!f) return false;
  }
  fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace,
                  ec);
  return true;
}

std::string bytes_to_hex(const std::vector<uint8_t>& bytes) {
  static const char kDigits[] = "0123456789abcdef";
  std::string s;
  s.reserve(bytes.size() * 2);
  for (const uint8_t b : bytes) {
    s += kDigits[b >> 4];
    s += kDigits[b & 15];
  }
  return s;
}

std::vector<uint8_t> hex_to_bytes(const std::string& s) {
  std::vector<uint8_t> out;
  if (s.size() % 2 != 0) return out;
  auto d = [](char c) {
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
  };
  for (size_t i = 0; i + 1 < s.size(); i += 2) {
    const int hi = d(s[i]), lo = d(s[i + 1]);
    if (hi < 0 || lo < 0) return {};
    out.push_back(static_cast<uint8_t>(hi * 16 + lo));
  }
  return out;
}

}  // namespace ge::account
