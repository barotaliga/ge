// MOD PACKAGE SIGNATURES (engine/mod/signature.hpp) on the sample package built by this tree: a
// package signed over its manifest loads and names its signer when the key is trusted; a manifest
// changed after signing is refused naming the manifest; a library changed after signing is
// refused naming the library (its hash, which the signed manifest holds); a game that loads only
// trusted packages refuses an unsigned one and one signed by a key it does not list; the trusted
// keys' reader names a bad line; what a player reads has no agent syntax.
//
// ge:summary Tests: a mod package signed, checked, tampered with and refused; the trusted keys
// ge:keywords test mod signature sign mod-sig trusted-keys tamper refuse signer trusted-only
// ge:skill mod testing
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "check.hpp"
#include "mod/loader.hpp"
#include "mod/signature.hpp"
#include "net/crypto.hpp"
#include "shooters_edge/sim/mod_api.hpp"

namespace fs = std::filesystem;
namespace gm = ge::mod;
namespace gn = ge::net;

namespace {

std::string read(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  std::stringstream s;
  s << f.rdbuf();
  return s.str();
}

void write(const fs::path& p, const std::string& text) {
  std::ofstream(p, std::ios::binary) << text;
}

gn::SignKeys keys_of(uint8_t fill) {
  gn::Key seed{};
  seed.fill(fill);
  return gn::sign_keys(seed);
}

// A copy of the sample package in a scratch directory, signed by `keys`.
fs::path signed_copy(const std::string& tag, const gn::SignKeys& keys) {
  const fs::path src = fs::path(GE_MOD_DIR) / "rocket-bounce";
  const fs::path dst = fs::temp_directory_path() / ("ge_mod_signature_test_" + tag);
  fs::remove_all(dst);
  fs::create_directories(dst);
  for (const auto& e : fs::directory_iterator(src))
    if (e.path().filename() != gm::kSignatureName)
      fs::copy_file(e.path(), dst / e.path().filename(), fs::copy_options::overwrite_existing);
  write(dst / gm::kSignatureName, gm::signature_text(keys, read(dst / gm::kManifestName)));
  return dst;
}

gm::LoadResult load(const fs::path& dir, const std::vector<gm::TrustedKey>& trusted,
                    bool trusted_only, gm::LoadedMod& m) {
  gm::GameOffer offer{SE_MOD_GAME, SE_MOD_API};
  offer.trusted = &trusted;
  offer.trusted_only = trusted_only;
  return gm::load_mod(dir, offer, m);
}

bool error_has(const gm::LoadResult& r, const std::string& a, const std::string& b = "") {
  for (const std::string& e : r.errors)
    if (e.find(a) != std::string::npos && (b.empty() || e.find(b) != std::string::npos))
      return true;
  return false;
}

}  // namespace

TEST(a_signed_package_loads_and_names_a_trusted_signer) {
  const gn::SignKeys author = keys_of(7);
  const fs::path dir = signed_copy("trusted", author);
  gm::LoadedMod m;
  const gm::LoadResult unknown = load(dir, {}, false, m);
  CHECK_MSG(unknown.ok(), unknown.errors.empty() ? "" : unknown.errors.front());
  CHECK(m.signature.state == gm::Signed::unknown);
  CHECK(m.signature.key == author.pub);
  const std::vector<gm::TrustedKey> trusted = {{author.pub, "the sample's author"}};
  gm::LoadedMod t;
  const gm::LoadResult r = load(dir, trusted, true, t);
  CHECK_MSG(r.ok(), r.errors.empty() ? "" : r.errors.front());
  CHECK(t.signature.state == gm::Signed::trusted);
  CHECK(t.signature.signer == "the sample's author");
  CHECK(gm::signature_words(t.signature, "rocket-bounce", "1.0.0") ==
        "rocket-bounce 1.0.0, signed by the sample's author");
}

TEST(a_manifest_changed_after_signing_is_refused_naming_it) {
  const fs::path dir = signed_copy("manifest", keys_of(7));
  std::string text = read(dir / gm::kManifestName);
  const size_t at = text.find("version = ");
  CHECK(at != std::string::npos);
  text.replace(at, 15, "version = 9.9.9");  // "version = 1.0.0"
  write(dir / gm::kManifestName, text);
  gm::LoadedMod m;
  const gm::LoadResult r = load(dir, {}, false, m);
  CHECK(!r.ok());
  CHECK(m.signature.state == gm::Signed::broken);
  CHECK_MSG(error_has(r, "mod.manifest", "not the one its author signed"),
            r.errors.empty() ? "no error" : r.errors.front());
}

TEST(a_library_changed_after_signing_is_refused_naming_the_file) {
  const fs::path dir = signed_copy("library", keys_of(7));
  gm::Manifest man;
  CHECK(gm::parse_manifest(read(dir / gm::kManifestName), "mod.manifest", man).ok());
  const gm::ManifestFile* file = gm::file_for(man, gm::this_platform());
  CHECK(file != nullptr);
  if (file == nullptr) return;
  std::string bytes = read(dir / file->path);
  bytes.push_back('\0');
  write(dir / file->path, bytes);
  gm::LoadedMod m;
  const gm::LoadResult r = load(dir, {}, false, m);
  CHECK(!r.ok());
  CHECK(m.signature.state == gm::Signed::unknown);  // the manifest is still the signed one
  CHECK_MSG(error_has(r, fs::path(file->path).filename().string(), file->sha256),
            r.errors.empty() ? "no error" : r.errors.front());
}

TEST(trusted_only_refuses_unsigned_and_unknown_packages) {
  const gn::SignKeys author = keys_of(7), stranger = keys_of(9);
  const std::vector<gm::TrustedKey> trusted = {{author.pub, "the sample's author"}};
  const fs::path dir = signed_copy("unknown", stranger);
  gm::LoadedMod m;
  const gm::LoadResult unknown = load(dir, trusted, true, m);
  CHECK(!unknown.ok());
  CHECK(error_has(unknown, "signed by a key you have not trusted", "loads only packages"));
  fs::remove(dir / gm::kSignatureName);
  gm::LoadedMod u;
  const gm::LoadResult unsigned_ = load(dir, trusted, true, u);
  CHECK(!unsigned_.ok());
  CHECK(error_has(unsigned_, "not signed", "loads only packages"));
  gm::LoadedMod open;
  CHECK(load(dir, trusted, false, open).ok());  // a game that allows unsigned packages
  CHECK(open.signature.state == gm::Signed::no);
}

TEST(a_signature_file_that_does_not_parse_is_refused) {
  const fs::path dir = signed_copy("garbled", keys_of(7));
  write(dir / gm::kSignatureName, "key = 12\n");
  gm::LoadedMod m;
  const gm::LoadResult r = load(dir, {}, false, m);
  CHECK(!r.ok());
  CHECK(error_has(r, "mod.sig", "ge mod sign"));
}

TEST(the_trusted_keys_are_read_and_a_bad_line_is_named) {
  const gn::SignKeys a = keys_of(1);
  std::vector<gm::TrustedKey> keys;
  std::string error;
  const std::string text = "# the game's keys\n" + gn::key_to_hex(a.pub) + " shooters-edge\n\n";
  CHECK_MSG(gm::parse_trusted_keys(text, "trusted.keys", keys, error), error);
  CHECK(keys.size() == 1 && keys[0].name == "shooters-edge" && keys[0].key == a.pub);
  CHECK(!gm::parse_trusted_keys(text + "not-a-key someone\n", "trusted.keys", keys, error));
  CHECK_MSG(error.find("trusted.keys:4") != std::string::npos, error);
}

TEST(what_a_player_reads_about_a_signature_has_no_agent_syntax) {
  gm::SignatureCheck c;
  for (gm::Signed s : {gm::Signed::no, gm::Signed::unknown, gm::Signed::trusted}) {
    c.state = s;
    c.signer = "shooters-edge";
    const std::string w = gm::signature_words(c, "rocket-bounce", "1.0.0");
    CHECK_MSG(w.find("next:") == std::string::npos && w.find('`') == std::string::npos, w);
  }
}
