// THE CLASSIC PROTOCOL'S INFO STRINGS: `\key\value\key\value...`, a client's user info and the
// server info. No key or value holds `\` or `"`; keys are case-sensitive and at most 64 bytes; a
// user info is at most 1024 bytes and 128 keys, the server info 512 bytes. Star keys (`*name`)
// are the server's alone: a client's `setinfo` on one is refused. A key set to the empty string
// is removed.
//
// NAMES (`sanitize_name`): any of `\`, CR, LF, `$`, `#`, `"`, `;` gives the default name; leading
// and trailing spaces are trimmed; "console" (any case) and an empty name give the default too.
// A name another client holds becomes `(N)name` cut to 10 characters, N the lowest free.
//
// ge:summary Info strings: get, set, remove and check; star keys; client names cleaned and unique
// ge:keywords info-string userinfo serverinfo star-keys setinfo info_get info_set info_remove
// ge:keywords info_valid sanitize_name unique_name default-name
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_wire_test.cpp
#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace shooters_edge::net {

constexpr size_t kUserInfoBytes = 1024;
constexpr size_t kServerInfoBytes = 512;
constexpr size_t kInfoKeyBytes = 64;
constexpr int kInfoMaxKeys = 128;

std::string info_get(const std::string& info, const std::string& key);
// Sets (or removes, for an empty value) a key; false, the info unchanged, when the key or the
// value holds `\` or `"`, the key is too long, or the result would pass `limit` bytes.
bool info_set(std::string& info, const std::string& key, const std::string& value,
              size_t limit = kUserInfoBytes);
void info_remove(std::string& info, const std::string& key);
// The pairs in order; false when the string is malformed (an odd count, an empty key, a quote).
bool info_pairs(const std::string& info, std::vector<std::pair<std::string, std::string>>& out);
// True when the string is a well-formed info string within `limit` and kInfoMaxKeys.
bool info_valid(const std::string& info, size_t limit = kUserInfoBytes);
// The info without the keys a starting `_` marks private (what other clients are sent).
std::string info_public(const std::string& info);

std::string sanitize_name(const std::string& name, const std::string& fallback = "player");
// `name`, or `(N)name` cut to 10 characters when `taken` already holds it.
std::string unique_name(const std::string& name, const std::vector<std::string>& taken);

}  // namespace shooters_edge::net
