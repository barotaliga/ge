// A MOD PACKAGE'S SIGNATURE (`mod.sig`, docs/mods.md § Signing): the author's signature over the
// manifest's bytes, and the keys a game trusts. The manifest carries every file's SHA-256, so a
// signature over it alone covers the whole package: a changed library fails its hash (the loader
// names the file), a changed manifest fails the signature.
//
//   # A mod package's signature (docs/mods.md § Signing). Written by ge mod sign.
//   key = 3b6a27bc...          the signer's public key, 64 hex digits
//   signature = 9f1c0d2e...    the signature over mod.manifest's bytes, 128 hex digits
//
// The scheme is the engine's one signature (net/crypto.hpp: EdDSA on Curve25519 over BLAKE2b),
// the one a master signs its tickets with. A key pair grows from a 32-byte seed the author keeps
// out of the tree (`ge mod sign --key-env=<VAR>`).
//
// TRUSTED KEYS: a text file, one key a line, `<64 hex digits> <name>`, `#` comments. A game ships
// its own and its community's; a player adds theirs. A package's verdict is one of four: unsigned,
// signed by a key nobody listed, signed by a listed key (named), or a signature that does not
// hold (refused: the manifest is not the one signed).
//
// ge:summary A mod package's signature over its manifest and the trusted keys a game keeps
// ge:keywords mod signature sign mod-sig trusted-keys trust signer ed25519 eddsa verify tamper
// ge:keywords check_signature signature_text parse_trusted_keys unsigned unknown-key
// ge:skill mod
// ge:test tests/mod_signature_test.cpp
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "net/crypto.hpp"

namespace ge::mod {

inline constexpr std::string_view kSignatureName = "mod.sig";

struct TrustedKey {
  net::Key key{};
  std::string name;  // who it is: "shooters-edge", "a community's name"
};

// `text` (a trusted-keys file read from `source`) into `out`, appended. False with `error` naming
// the line when one is not a key and a name.
bool parse_trusted_keys(std::string_view text, std::string_view source,
                        std::vector<TrustedKey>& out, std::string& error);

enum class Signed : uint8_t {
  no,       // no mod.sig
  unknown,  // signed, and the signature holds, by a key not in the list
  trusted,  // signed by a listed key
  broken,   // a mod.sig that does not parse or does not hold over the manifest: refuse it
};

struct SignatureCheck {
  Signed state = Signed::no;
  net::Key key{};      // the signer's (unknown and trusted)
  std::string signer;  // the listed key's name (trusted)
  std::string error;   // why it is broken
};

// The signature in `dir` over `manifest_text` (mod.manifest's bytes), against `trusted`.
SignatureCheck check_signature(const std::filesystem::path& dir, std::string_view manifest_text,
                               const std::vector<TrustedKey>& trusted);

// mod.sig's text: `keys` signing `manifest_text`.
std::string signature_text(const net::SignKeys& keys, std::string_view manifest_text);

// What a player is about to run, in the game's words: "rocket-bounce 1.0.0, signed by
// shooters-edge", "..., signed by a key you have not trusted", "..., not signed".
std::string signature_words(const SignatureCheck& c, std::string_view name,
                            std::string_view version);

}  // namespace ge::mod
