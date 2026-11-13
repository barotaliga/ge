// engine/webtransport/certificate.hpp: the self-signed certificate a page trusts by its hash. Its
// shape (DER, a P-256 key in it), its hash, its validity (a browser takes at most 14 days), the hex
// both ways, and the dates it writes.
//
// ge:summary Test: the WebTransport certificate's DER, key, hash, validity and dates
// ge:keywords test webtransport certificate der sha-256 utctime validity
// ge:skill net testing
#include "webtransport/certificate.hpp"

#include <string>

#include "check.hpp"

using namespace ge::webtransport;

TEST(certificate_is_der_with_its_key_and_hash) {
  Certificate c;
  std::string err;
  CHECK_MSG(make_certificate(c, 1800000000, "forward", &err), err);
  CHECK(c.der.size() > 200 && c.der.size() < 600);
  CHECK(c.der[0] == 0x30);
  CHECK(sha256(c.der.data(), c.der.size()) == c.hash);
  std::array<uint8_t, 64> key{};
  CHECK(certificate_public_key(c.der.data(), c.der.size(), key));
  CHECK(key == c.public_key);
  // The name is in it, as issuer and subject.
  const std::string der(c.der.begin(), c.der.end());
  CHECK(der.find("forward") != std::string::npos);
  CHECK(der.find("forward") != der.rfind("forward"));
}

TEST(certificate_validity_fits_a_browser) {
  Certificate c;
  CHECK(make_certificate(c, 1800000000, "x", nullptr));
  CHECK(c.not_before < 1800000000 && c.not_after > 1800000000);
  CHECK(c.not_after - c.not_before <= int64_t{14} * 86400);
  CHECK(c.not_after - 1800000000 >= int64_t{12} * 86400);
  const std::string der(c.der.begin(), c.der.end());
  CHECK(der.find(utc_time(c.not_before)) != std::string::npos);
  CHECK(der.find(utc_time(c.not_after)) != std::string::npos);
}

TEST(certificate_each_is_new) {
  Certificate a, b;
  CHECK(make_certificate(a, 1800000000, "x", nullptr));
  CHECK(make_certificate(b, 1800000000, "x", nullptr));
  CHECK(a.hash != b.hash);
  CHECK(a.key != b.key);
}

TEST(certificate_refuses_a_bad_name) {
  Certificate c;
  std::string err;
  CHECK(!make_certificate(c, 1800000000, "", &err));
  CHECK(err.find("1 to 64 bytes") != std::string::npos);
  CHECK(!make_certificate(c, 1800000000, std::string(65, 'n'), &err));
}

TEST(certificate_hash_hex_both_ways) {
  Certificate c;
  CHECK(make_certificate(c, 1800000000, "x", nullptr));
  const std::string hex = hash_hex(c);
  CHECK(hex.size() == 64);
  std::array<uint8_t, 32> back{};
  CHECK(parse_hash_hex(hex, back));
  CHECK(back == c.hash);
  CHECK(!parse_hash_hex(hex.substr(1), back));
  CHECK(!parse_hash_hex(std::string(63, '0') + "g", back));
}

TEST(certificate_utc_time) {
  CHECK(utc_time(0) == "700101000000Z");
  CHECK(utc_time(1700000000) == "231114221320Z");
  CHECK(utc_time(951782400) == "000229000000Z");  // a leap day
  CHECK(utc_time(2524607999) == "491231235959Z");
}

TEST(certificate_sha256_known) {
  // FIPS 180-2's "abc".
  const uint8_t abc[3] = {'a', 'b', 'c'};
  const std::array<uint8_t, 32> h = sha256(abc, 3);
  CHECK(h[0] == 0xba && h[1] == 0x78 && h[31] == 0xad);
}

TEST(certificate_names_itself_again_as_a_dns_name) {
  // A browser refused a hashed certificate with no extensions; ours carries a subjectAltName.
  Certificate c;
  CHECK(make_certificate(c, 1800000000, "forward", nullptr));
  const uint8_t san_oid[] = {0x06, 0x03, 0x55, 0x1d, 0x11};
  const std::string der(c.der.begin(), c.der.end());
  CHECK(der.find(std::string(reinterpret_cast<const char*>(san_oid), sizeof san_oid)) !=
        std::string::npos);
  CHECK(der.find(std::string("\x82\x07") + "forward") != std::string::npos);
}
