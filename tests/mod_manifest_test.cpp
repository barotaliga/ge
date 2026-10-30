// THE MOD MANIFEST AND ITS HASH: SHA-256 against the standard's test vectors, and the manifest's
// reader against a good package, every kind of bad line (each refused with its line and what to do
// next), unknown keys (a warning only), a game_api range, and the writer read back unchanged.
//
// ge:summary Tests: SHA-256 vectors, and the mod manifest's reader, checks and writer
// ge:keywords test mod manifest sha256 parse check write platform refuse
// ge:skill mod testing
#include <string>

#include "check.hpp"
#include "mod/manifest.hpp"
#include "mod/sha256.hpp"

namespace gm = ge::mod;

namespace {

const std::string kHash(64, 'a');

std::string good() {
  return "# a package\n"
         "name = rocket-bounce\n"
         "version = 1.2.3\n"
         "game = shooters_edge\n"
         "game_api = 1..3\n"
         "abi = 1.0\n"
         "file.linux-x64 = lib/rocket_bounce.so " +
         kHash + "\nfile.windows-x64 = rocket_bounce.dll " + kHash + "\n";
}

bool has(const gm::ManifestCheck& c, const std::string& what) {
  for (const std::string& e : c.errors)
    if (e.find(what) != std::string::npos && e.find("next:") != std::string::npos) return true;
  return false;
}

std::string replaced(std::string s, const std::string& from, const std::string& to) {
  s.replace(s.find(from), from.size(), to);
  return s;
}

}  // namespace

TEST(sha256_matches_the_standards_vectors) {
  CHECK(gm::sha256_hex("", 0) ==
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  CHECK(gm::sha256_hex("abc", 3) ==
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  const std::string two_blocks = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  CHECK(gm::sha256_hex(two_blocks.data(), two_blocks.size()) ==
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  const std::string million(1000000, 'a');
  CHECK(gm::sha256_hex(million.data(), million.size()) ==
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST(a_good_manifest_reads_every_field) {
  gm::Manifest m;
  const gm::ManifestCheck c = gm::parse_manifest(good(), "mod.manifest", m);
  CHECK(c.ok());
  CHECK(c.warnings.empty());
  CHECK(m.name == "rocket-bounce");
  CHECK(m.version == "1.2.3");
  CHECK(m.game == "shooters_edge");
  CHECK(m.game_api_min == 1 && m.game_api_max == 3);
  CHECK(m.abi_major == 1 && m.abi_minor == 0);
  CHECK(m.files.size() == 2);
  const gm::ManifestFile* f = gm::file_for(m, "linux-x64");
  CHECK(f && f->path == "lib/rocket_bounce.so" && f->sha256 == kHash);
  CHECK(gm::file_for(m, "macos-arm64") == nullptr);
}

TEST(the_writer_reads_back_unchanged) {
  gm::Manifest m;
  CHECK(gm::parse_manifest(good(), "a", m).ok());
  gm::Manifest again;
  CHECK(gm::parse_manifest(gm::write_manifest(m), "b", again).ok());
  CHECK(gm::write_manifest(again) == gm::write_manifest(m));
  CHECK(again.game_api_min == 1 && again.game_api_max == 3 && again.files.size() == 2);
}

TEST(every_bad_line_is_refused_with_what_to_do) {
  gm::Manifest m;
  CHECK(has(gm::parse_manifest(replaced(good(), "rocket-bounce", "Rocket_Bounce"), "x", m),
            "not kebab case"));
  CHECK(has(gm::parse_manifest(replaced(good(), "1.2.3", "1.2"), "x", m), "major.minor.patch"));
  CHECK(has(gm::parse_manifest(replaced(good(), "1..3", "3..1"), "x", m), "game_api"));
  CHECK(has(gm::parse_manifest(replaced(good(), "abi = 1.0", "abi = one"), "x", m), "abi"));
  CHECK(has(gm::parse_manifest(replaced(good(), "file.windows-x64", "file.amiga"), "x", m),
            "not a platform"));
  CHECK(has(gm::parse_manifest(replaced(good(), "file.windows-x64", "file.linux-x64"), "x", m),
            "listed twice"));
  CHECK(has(gm::parse_manifest(replaced(good(), "lib/rocket", "../rocket"), "x", m),
            "relative path"));
  CHECK(
      has(gm::parse_manifest(replaced(good(), "lib/rocket", "/rocket"), "x", m), "relative path"));
  CHECK(has(gm::parse_manifest(replaced(good(), kHash + "\nfile.w", "abc\nfile.w"), "x", m),
            "64 lower-case hex"));
  CHECK(has(gm::parse_manifest(replaced(good(), "game = shooters_edge\n", ""), "x", m),
            "no `game` line"));
  CHECK(has(
      gm::parse_manifest("name = a\nversion = 1.0.0\ngame = g\ngame_api = 1\nabi = 1.0\n", "x", m),
      "no `file.<platform>` line"));
  const gm::ManifestCheck c = gm::parse_manifest(replaced(good(), "abi = 1.0", "abi 1.0"), "p", m);
  CHECK(has(c, "p:6:"));
}

TEST(an_unknown_key_is_a_warning) {
  gm::Manifest m;
  const gm::ManifestCheck c = gm::parse_manifest(good() + "signature = later\n", "x", m);
  CHECK(c.ok());
  CHECK(c.warnings.size() == 1);
}

TEST(this_platform_is_one_a_manifest_may_list) {
  CHECK(gm::known_platform(gm::this_platform()));
  CHECK(!gm::known_platform("amiga"));
}
