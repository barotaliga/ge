// SHA-1 (FIPS 180-4) of a byte string: the digest a WebSocket handshake answers with (engine/tv)
// and a hashed remote console line carries (a game's rcon, so its password never travels). Not for
// anything that must resist a forger who chooses both inputs.
//
// ge:summary SHA-1 of a byte string, and its digest as hex, for handshakes and hashed rcon
// ge:keywords sha1 digest hash hex websocket rcon hashed-password sha1_hex
// ge:skill net
// ge:test tests/net_sha1_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace ge::net {

std::array<uint8_t, 20> sha1(std::span<const uint8_t> bytes);
// The digest of `text` as 40 upper-case hex digits.
std::string sha1_hex(std::string_view text);

}  // namespace ge::net
