// A package's signature checked and written; the trusted keys read (signature.hpp).
//
// ge:summary The mod signature's reader, check and writer; the trusted keys' reader
// ge:keywords mod signature check_signature signature_text parse_trusted_keys trust
// ge:skill mod
// ge:test tests/mod_signature_test.cpp
#include "mod/signature.hpp"

#include <fstream>
#include <sstream>

namespace ge::mod {

namespace {

std::string_view trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
    s.remove_suffix(1);
  return s;
}

int hex_digit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool from_hex(std::string_view hex, uint8_t* out, size_t size) {
  if (hex.size() != size * 2) return false;
  for (size_t k = 0; k < size; k++) {
    const int hi = hex_digit(hex[2 * k]), lo = hex_digit(hex[2 * k + 1]);
    if (hi < 0 || lo < 0) return false;
    out[k] = static_cast<uint8_t>(hi * 16 + lo);
  }
  return true;
}

std::string to_hex(const uint8_t* bytes, size_t size) {
  static const char* digits = "0123456789abcdef";
  std::string s;
  s.reserve(size * 2);
  for (size_t k = 0; k < size; k++) {
    s.push_back(digits[bytes[k] >> 4]);
    s.push_back(digits[bytes[k] & 15]);
  }
  return s;
}

}  // namespace

bool parse_trusted_keys(std::string_view text, std::string_view source,
                        std::vector<TrustedKey>& out, std::string& error) {
  size_t line_no = 0;
  while (!text.empty()) {
    const size_t end = text.find('\n');
    std::string_view line = text.substr(0, end);
    text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
    line_no++;
    const size_t hash = line.find('#');
    line = trim(line.substr(0, hash));
    if (line.empty()) continue;
    const size_t gap = line.find_first_of(" \t");
    TrustedKey k;
    const std::string_view name =
        gap == std::string_view::npos ? std::string_view{} : trim(line.substr(gap));
    if (gap == std::string_view::npos || name.empty() ||
        !from_hex(line.substr(0, gap), k.key.data(), k.key.size())) {
      error = std::string(source) + ":" + std::to_string(line_no) +
              ": a trusted key is 64 hex digits and a name; next: write `<key> <name>` "
              "(ge mod sign prints the key)";
      return false;
    }
    k.name = std::string(name);
    out.push_back(std::move(k));
  }
  return true;
}

SignatureCheck check_signature(const std::filesystem::path& dir, std::string_view manifest_text,
                               const std::vector<TrustedKey>& trusted) {
  SignatureCheck c;
  const std::filesystem::path path = dir / kSignatureName;
  std::ifstream f(path, std::ios::binary);
  if (!f) return c;
  std::stringstream ss;
  ss << f.rdbuf();
  const std::string text = ss.str();
  const std::string where = path.generic_string();
  bool have_key = false, have_sig = false;
  net::Signature sig{};
  std::string_view rest = text;
  while (!rest.empty()) {
    const size_t end = rest.find('\n');
    std::string_view line = rest.substr(0, end);
    rest = end == std::string_view::npos ? std::string_view{} : rest.substr(end + 1);
    line = trim(line.substr(0, line.find('#')));
    if (line.empty()) continue;
    const size_t eq = line.find('=');
    if (eq == std::string_view::npos) continue;
    const std::string_view key = trim(line.substr(0, eq)), value = trim(line.substr(eq + 1));
    if (key == "key") have_key = from_hex(value, c.key.data(), c.key.size());
    if (key == "signature") have_sig = from_hex(value, sig.data(), sig.size());
  }
  if (!have_key || !have_sig) {
    c.state = Signed::broken;
    c.error = where +
              ": the signature file needs a key (64 hex digits) and a signature (128); next: "
              "sign the package again (ge mod sign)";
    return c;
  }
  if (!net::sign_check(c.key, sig, reinterpret_cast<const uint8_t*>(manifest_text.data()),
                       manifest_text.size())) {
    c.state = Signed::broken;
    c.error = where + ": the signature does not hold over " +
              (dir / "mod.manifest").generic_string() +
              ": the manifest is not the one its author signed; next: get the package again "
              "from its author, or sign it again if it is yours (ge mod sign)";
    return c;
  }
  c.state = Signed::unknown;
  for (const TrustedKey& t : trusted)
    if (t.key == c.key) {
      c.state = Signed::trusted;
      c.signer = t.name;
      break;
    }
  return c;
}

std::string signature_text(const net::SignKeys& keys, std::string_view manifest_text) {
  const net::Signature sig =
      net::sign(keys, reinterpret_cast<const uint8_t*>(manifest_text.data()), manifest_text.size());
  return "# A mod package's signature (docs/mods.md § Signing). Written by ge mod sign.\n"
         "key = " +
         to_hex(keys.pub.data(), keys.pub.size()) +
         "\nsignature = " + to_hex(sig.data(), sig.size()) + "\n";
}

std::string signature_words(const SignatureCheck& c, std::string_view name,
                            std::string_view version) {
  std::string s = std::string(name) + " " + std::string(version);
  switch (c.state) {
    case Signed::trusted:
      return s + ", signed by " + c.signer;
    case Signed::unknown:
      return s + ", signed by a key you have not trusted";
    case Signed::broken:
      return s + ", whose signature does not hold";
    case Signed::no:
      break;
  }
  return s + ", not signed";
}

}  // namespace ge::mod
