// The certificate's DER, written by hand (a fixed shape: version 3, a serial, ecdsa-with-SHA256,
// the name as issuer and subject, the validity, the P-256 key, the name again as a subjectAltName),
// signed by micro-ecc and hashed by picotls's SHA-256.
//
// ge:summary The self-signed certificate's DER writer, key, signature and hash
// ge:keywords webtransport certificate der asn1 utctime ecdsa-signature uecc sha-256
// ge:skill net
// ge:test tests/webtransport_certificate_test.cpp
#include "webtransport/certificate.hpp"

#include <cstdio>
#include <cstring>

#include "picotls.h"
#include "picotls/minicrypto.h"
#include "uECC.h"

namespace ge::webtransport {

namespace {

int uecc_random(uint8_t* dest, unsigned size) {
  ptls_minicrypto_random_bytes(dest, size);
  return 1;
}

// DER pieces: a tag, its length and its content.
void put_length(std::vector<uint8_t>& out, size_t n) {
  if (n < 0x80) {
    out.push_back(static_cast<uint8_t>(n));
  } else if (n < 0x100) {
    out.push_back(0x81);
    out.push_back(static_cast<uint8_t>(n));
  } else {
    out.push_back(0x82);
    out.push_back(static_cast<uint8_t>(n >> 8));
    out.push_back(static_cast<uint8_t>(n & 0xff));
  }
}

void put(std::vector<uint8_t>& out, uint8_t tag, const std::vector<uint8_t>& content) {
  out.push_back(tag);
  put_length(out, content.size());
  out.insert(out.end(), content.begin(), content.end());
}

void put_raw(std::vector<uint8_t>& out, std::initializer_list<uint8_t> bytes) {
  out.insert(out.end(), bytes.begin(), bytes.end());
}

// An unsigned big-endian integer as DER: leading zeros dropped, a zero put back when the top bit
// is set.
void put_uint(std::vector<uint8_t>& out, const uint8_t* bytes, size_t size) {
  size_t i = 0;
  while (i + 1 < size && bytes[i] == 0) ++i;
  std::vector<uint8_t> v;
  if (bytes[i] & 0x80) v.push_back(0);
  v.insert(v.end(), bytes + i, bytes + size);
  put(out, 0x02, v);
}

// ecdsa-with-SHA256 (1.2.840.10045.4.3.2) as an algorithm identifier.
void put_ecdsa_sha256(std::vector<uint8_t>& out) {
  put_raw(out, {0x30, 0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02});
}

// A Name of one common name (2.5.4.3) as a UTF8String.
void put_name(std::vector<uint8_t>& out, std::string_view name) {
  std::vector<uint8_t> atv;
  put_raw(atv, {0x06, 0x03, 0x55, 0x04, 0x03});
  put(atv, 0x0c, std::vector<uint8_t>(name.begin(), name.end()));
  std::vector<uint8_t> seq, set;
  put(seq, 0x30, atv);
  put(set, 0x31, seq);
  put(out, 0x30, set);
}

// The subject public key info's fixed prefix for an uncompressed P-256 point: the algorithm
// (id-ecPublicKey, prime256v1) and the bit string's header, then 0x04, x and y.
constexpr uint8_t kP256Info[] = {0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48,
                                 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a, 0x86, 0x48,
                                 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00, 0x04};

// Days since 1970-01-01 to the civil date (proleptic Gregorian).
void civil_from_days(int64_t z, int& y, int& m, int& d) {
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const int64_t doe = z - era * 146097;
  const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int64_t mp = (5 * doy + 2) / 153;
  d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  y = static_cast<int>(yoe + era * 400 + (m <= 2 ? 1 : 0));
}

}  // namespace

std::array<uint8_t, 32> sha256(const uint8_t* data, size_t size) {
  std::array<uint8_t, 32> out{};
  ptls_calc_hash(&ptls_minicrypto_sha256, out.data(), data, size);
  return out;
}

std::string utc_time(int64_t unix_seconds) {
  int64_t days = unix_seconds / 86400;
  int64_t rest = unix_seconds % 86400;
  if (rest < 0) {
    rest += 86400;
    --days;
  }
  int y = 0, m = 0, d = 0;
  civil_from_days(days, y, m, d);
  char text[64];
  std::snprintf(text, sizeof text, "%02d%02d%02d%02d%02d%02dZ", y % 100, m, d,
                static_cast<int>(rest / 3600), static_cast<int>(rest / 60 % 60),
                static_cast<int>(rest % 60));
  return text;
}

bool make_certificate(Certificate& out, int64_t unix_now, std::string_view name,
                      std::string* error) {
  if (name.empty() || name.size() > 64) {
    if (error) {
      *error = "certificate: the name must be 1 to 64 bytes, not " + std::to_string(name.size()) +
               "; pass a short name such as the program's";
    }
    return false;
  }
  uECC_set_rng(uecc_random);
  const uECC_Curve curve = uECC_secp256r1();
  if (!uECC_make_key(out.public_key.data(), out.key.data(), curve)) {
    if (error) *error = "certificate: the P-256 key could not be made (the random source failed)";
    return false;
  }
  out.not_before = unix_now - 3600;
  out.not_after = out.not_before + kCertificateDays * 86400;

  std::vector<uint8_t> tbs;
  put_raw(tbs, {0xa0, 0x03, 0x02, 0x01, 0x02});  // version 3
  uint8_t serial[16];
  ptls_minicrypto_random_bytes(serial, sizeof serial);
  serial[0] = static_cast<uint8_t>((serial[0] & 0x7f) | 0x01);  // positive, never zero
  put_uint(tbs, serial, sizeof serial);
  put_ecdsa_sha256(tbs);
  put_name(tbs, name);
  std::vector<uint8_t> validity;
  const std::string from = utc_time(out.not_before), to = utc_time(out.not_after);
  put(validity, 0x17, std::vector<uint8_t>(from.begin(), from.end()));
  put(validity, 0x17, std::vector<uint8_t>(to.begin(), to.end()));
  put(tbs, 0x30, validity);
  put_name(tbs, name);
  tbs.insert(tbs.end(), std::begin(kP256Info), std::end(kP256Info));
  tbs.insert(tbs.end(), out.public_key.begin(), out.public_key.end());
  // Extensions: the name again as a DNS name (subjectAltName, 2.5.29.17).
  std::vector<uint8_t> dns, names, ext, exts, wrapped, octets;
  dns.push_back(0x82);  // [2] dNSName
  put_length(dns, name.size());
  dns.insert(dns.end(), name.begin(), name.end());
  put(names, 0x30, dns);
  put_raw(ext, {0x06, 0x03, 0x55, 0x1d, 0x11});
  put(ext, 0x04, names);
  put(exts, 0x30, ext);
  put(wrapped, 0x30, exts);
  put(tbs, 0xa3, wrapped);
  std::vector<uint8_t> tbs_seq;
  put(tbs_seq, 0x30, tbs);

  const std::array<uint8_t, 32> digest = sha256(tbs_seq.data(), tbs_seq.size());
  uint8_t sig[64];
  if (!uECC_sign(out.key.data(), digest.data(), digest.size(), sig, curve)) {
    if (error) *error = "certificate: signing failed (the random source failed)";
    return false;
  }
  std::vector<uint8_t> rs, sig_seq, sig_bits{0x00};
  put_uint(rs, sig, 32);
  put_uint(rs, sig + 32, 32);
  put(sig_seq, 0x30, rs);
  sig_bits.insert(sig_bits.end(), sig_seq.begin(), sig_seq.end());

  std::vector<uint8_t> body = tbs_seq;
  put_ecdsa_sha256(body);
  put(body, 0x03, sig_bits);
  out.der.clear();
  put(out.der, 0x30, body);
  out.hash = sha256(out.der.data(), out.der.size());
  return true;
}

std::string hash_hex(const Certificate& c) {
  static const char kHex[] = "0123456789abcdef";
  std::string s;
  s.reserve(64);
  for (uint8_t b : c.hash) {
    s.push_back(kHex[b >> 4]);
    s.push_back(kHex[b & 15]);
  }
  return s;
}

bool parse_hash_hex(std::string_view text, std::array<uint8_t, 32>& out) {
  if (text.size() != 64) return false;
  auto nibble = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  for (size_t i = 0; i < 32; ++i) {
    const int hi = nibble(text[2 * i]), lo = nibble(text[2 * i + 1]);
    if (hi < 0 || lo < 0) return false;
    out[i] = static_cast<uint8_t>(hi * 16 + lo);
  }
  return true;
}

bool certificate_public_key(const uint8_t* der, size_t size, std::array<uint8_t, 64>& out) {
  const size_t n = sizeof kP256Info;
  for (size_t i = 0; i + n + 64 <= size; ++i) {
    if (std::memcmp(der + i, kP256Info, n) == 0) {
      std::memcpy(out.data(), der + i + n, 64);
      return true;
    }
  }
  return false;
}

}  // namespace ge::webtransport
