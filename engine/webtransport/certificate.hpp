// THE ENDPOINT'S CERTIFICATE (docs/hub.md § Playing in a browser): a self-signed X.509 v3
// certificate over an ECDSA P-256 key, made in memory when the endpoint starts. A browser trusts it
// without any certificate authority because the page names it by its SHA-256
// (`serverCertificateHashes` in the page's `new WebTransport(...)`), which the hub hands the page.
// A browser takes such a certificate only when it is valid for at most two weeks, its key is P-256
// and it carries extensions (Chromium refused one with none: ours names the subject again as a DNS
// name), so the endpoint makes a new one well before that (`kCertificateDays`) and the hub's page
// reads the hash again with each status record.
//
//   Certificate c; make_certificate(c, unix_now, "forward", &error);
//   hash_hex(c)   the hash the page passes, as hex; c.der is what the TLS handshake sends
//
// ge:summary A self-signed P-256 certificate a browser trusts by its SHA-256 (WebTransport)
// ge:keywords webtransport certificate x509 der asn1 self-signed ecdsa p-256 secp256r1 sha-256
// ge:keywords server-certificate-hashes browser tls
// ge:skill net
// ge:test tests/webtransport_certificate_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ge::webtransport {

// A browser refuses a hashed certificate valid for longer than 14 days; ours is good for 13, from
// an hour before it was made (a client's clock a little behind still takes it).
constexpr int64_t kCertificateDays = 13;

struct Certificate {
  std::vector<uint8_t> der;              // the certificate as the handshake sends it
  std::array<uint8_t, 32> key{};         // the P-256 private key
  std::array<uint8_t, 64> public_key{};  // its public point, x then y
  std::array<uint8_t, 32> hash{};        // SHA-256 of `der`: what the page trusts
  int64_t not_before = 0;                // unix seconds
  int64_t not_after = 0;
};

// A new key and its certificate for `name` (the subject's common name), valid from an hour before
// `unix_now`. False with `error` only when the name is empty or longer than 64 bytes.
bool make_certificate(Certificate& out, int64_t unix_now, std::string_view name,
                      std::string* error);

// The hash as 64 lower-case hex digits.
std::string hash_hex(const Certificate& c);
// Hex back to a hash; false when it is not 64 hex digits.
bool parse_hash_hex(std::string_view text, std::array<uint8_t, 32>& out);

// SHA-256 of any bytes (the certificate's hash, and the client's check of one).
std::array<uint8_t, 32> sha256(const uint8_t* data, size_t size);

// The P-256 public key in a certificate this module made (its subject key info), for the client's
// check of the handshake's signature. False when there is none.
bool certificate_public_key(const uint8_t* der, size_t size, std::array<uint8_t, 64>& out);

// A date as the certificate writes it (UTCTime, "YYMMDDHHMMSSZ"), for the years 1950 to 2049.
std::string utc_time(int64_t unix_seconds);

}  // namespace ge::webtransport
