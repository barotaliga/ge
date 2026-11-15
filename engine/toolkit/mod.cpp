// The mod group's bodies and rows (mod.hpp): the engine's loader asked, manifests merged, packages
// signed.
//
// ge:summary The mod group's bodies: the loader's check, the merge of packages, the key, the sign
// ge:keywords toolkit mod check pack key sign loader manifest merge signature table_json
// ge:skill agent-interface mod
// ge:test tests/toolkit_mod_test.cpp
#include "toolkit/mod.hpp"

#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <system_error>

#include "core/json_write.hpp"
#include "mod/loader.hpp"
#include "mod/manifest.hpp"
#include "mod/signature.hpp"
#include "net/crypto.hpp"
#include "net/token.hpp"

namespace ge::toolkit {

namespace fs = std::filesystem;
using core::json_num;
using core::json_obj;
using core::json_str;
using core::json_strs;

namespace {

std::optional<std::string> read_text(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return std::nullopt;
  std::ostringstream s;
  s << in.rdbuf();
  return s.str();
}

bool write_text(const fs::path& path, std::string_view text, std::string& error) {
  std::error_code ec;
  if (path.has_parent_path()) fs::create_directories(path.parent_path(), ec);
  std::ofstream out(path, std::ios::binary);
  if (out) out.write(text.data(), static_cast<std::streamsize>(text.size()));
  if (!out) {
    error = "cannot write " + path.generic_string();
    return false;
  }
  return true;
}

// The loader's answer as findings; `true` when it loaded.
bool loader_findings(const mod::LoadResult& r, const std::string& where, Outcome& out) {
  for (const std::string& w : r.warnings)
    out.findings.push_back(mod_finding(Severity::warn, w, where));
  for (const std::string& e : r.errors)
    out.findings.push_back(mod_finding(Severity::error, e, where));
  return r.ok();
}

const char* signed_word(mod::Signed s) {
  switch (s) {
    case mod::Signed::trusted:
      return "trusted";
    case mod::Signed::unknown:
      return "unknown-key";
    case mod::Signed::broken:
      return "broken";
    case mod::Signed::no:
      break;
  }
  return "unsigned";
}

std::string table_json(const mod::LoadedMod& m) {
  const ge_mod_v1* t = m.table;
  const uint32_t hooks = t && t->hooks ? *static_cast<const uint32_t*>(t->hooks) : 0;
  return json_obj({{"name", json_str(m.manifest.name)},
                   {"version", json_str(m.manifest.version)},
                   {"game", json_str(m.manifest.game)},
                   {"platform", json_str(m.platform)},
                   {"abi", json_str(std::to_string(GE_MOD_ABI_MAJOR_OF(t->abi)) + "." +
                                    std::to_string(GE_MOD_ABI_MINOR_OF(t->abi)))},
                   {"game_api", json_num(t->game_api)},
                   {"state_bytes", json_num(t->state_bytes)},
                   {"hooks_bytes", json_num(hooks)},
                   {"signed", json_str(signed_word(m.signature.state))},
                   {"signer", json_str(m.signature.signer)}});
}

mod::GameOffer offer_of(const std::string& game) {
  const ModGame* g = find_mod_game(game);
  return g ? mod::GameOffer{g->id, g->api} : mod::GameOffer{};
}

Outcome run_check(const Call& c) {
  ModArgs a;
  a.dir = c.words.at(0);
  a.game = flag_value(c, "game");
  a.trusted = flag_value(c, "trusted");
  a.trusted_only = has_flag(c, "trusted-only");
  return mod_check(c.program, a);
}
Outcome run_pack(const Call& c) {
  ModArgs a;
  for (const std::string& w : c.words) a.packages.emplace_back(w);
  a.out = flag_value(c, "out");
  return mod_pack(c.program, a);
}
Outcome run_key(const Call& c) { return mod_key(c.program); }
Outcome run_sign(const Call& c) {
  ModArgs a;
  a.dir = c.words.at(0);
  a.key_env = flag_value(c, "key-env");
  return mod_sign(c.program, a);
}

constexpr Flag kCheckFlags[] = {
    {"game", "<id>", "check it against this game (default: the game its manifest names)"},
    {"trusted", "<file>", "the trusted keys (`<key> <name>` a line): name the package's signer"},
    {"trusted-only", "", "refuse a package no trusted key signed, as a game that asks for that"}};
constexpr Flag kSignFlags[] = {
    {"key-env", "<VAR>", "the environment variable holding the secret (default GE_MOD_KEY)"}};
constexpr Flag kPackFlags[] = {
    {"out", "<dir>", "where the package goes (default dist/<name>-<version>)"}};

constexpr Row kRows[] = {
    {"mod check", "<package>", 1, 1, "build",
     "a mod package (a directory or its mod.manifest) through the engine's loader: the manifest, "
     "the ABI, the game and its API, the library's hash, the entry table",
     kCheckFlags, "mod check faster-rockets/build/mods/faster-rockets", "mod", run_check, "",
     "mod"},
    {"mod pack", "<package>...", 1, -1, "build",
     "one package from several platforms' builds of one mod: manifests merged, libraries copied, "
     "the result checked",
     kPackFlags, "mod pack linux/faster-rockets windows/faster-rockets --out=dist/fr", "mod",
     run_pack, "", "mod"},
    {"mod key",
     "",
     0,
     0,
     "create",
     "a new signing key for mod packages: the secret to keep out of every repository and the "
     "public key a game's trusted keys list",
     {},
     "mod key",
     "mod",
     run_key,
     "",
     "mod"},
    {"mod sign", "<package>", 1, 1, "build",
     "the package checked, then signed: mod.sig beside its manifest, the signature over the "
     "manifest (which holds every file's hash) by the key in the environment",
     kSignFlags, "mod sign dist/fr --key-env=MY_MOD_KEY", "mod", run_sign, "", "mod"},
};

}  // namespace

const ModGame* find_mod_game(std::string_view id) {
  for (const ModGame& g : kModGames)
    if (g.id == id) return &g;
  return nullptr;
}

std::string mod_games_list() {
  std::string s;
  for (const ModGame& g : kModGames) s += (s.empty() ? "" : ", ") + std::string(g.id);
  return s;
}

Finding mod_finding(Severity s, const std::string& message, const std::string& where) {
  const size_t at = message.find("; next: ");
  if (at == std::string::npos) return {s, message, where, "see docs/mods.md"};
  return {s, message.substr(0, at), where, message.substr(at + 8)};
}

Outcome mod_check(const std::string& program, const ModArgs& o) {
  Outcome out;
  mod::LoadedMod m;
  mod::Manifest peek;
  const fs::path manifest = fs::is_directory(o.dir) ? o.dir / mod::kManifestName : fs::path(o.dir);
  (void)mod::parse_manifest(read_text(manifest).value_or(""), manifest.generic_string(), peek);
  const std::string game = o.game.empty() ? peek.game : o.game;
  if (!o.game.empty() && !find_mod_game(o.game)) {
    out.findings.push_back({Severity::error, "no game `" + o.game + "` offers mods",
                            "the command line", "pass --game=<id>, one of: " + mod_games_list()});
    out.exit = 2;
    return out;
  }
  std::vector<mod::TrustedKey> trusted;
  if (!o.trusted.empty()) {
    std::string error;
    const std::optional<std::string> text = read_text(o.trusted);
    if (!text || !mod::parse_trusted_keys(*text, o.trusted, trusted, error)) {
      out.findings.push_back(mod_finding(
          Severity::error,
          text ? error : o.trusted + " cannot be read; next: pass a trusted-keys file", o.trusted));
      out.exit = 2;
      return out;
    }
  }
  mod::GameOffer offer = offer_of(game);
  offer.trusted = &trusted;
  offer.trusted_only = o.trusted_only;
  const mod::LoadResult r = mod::load_mod(o.dir, offer, m);
  if (!loader_findings(r, o.dir.generic_string(), out)) return out;
  out.result = table_json(m);
  out.summary = mod::signature_words(m.signature, m.manifest.name, m.manifest.version) +
                "; loads for " + m.manifest.game + " on " + m.platform + " (mod ABI " +
                std::to_string(m.manifest.abi_major) + "." + std::to_string(m.manifest.abi_minor) +
                ")";
  out.next = program + " mod pack " + o.dir.generic_string();
  return out;
}

Outcome mod_pack(const std::string& program, const ModArgs& o) {
  Outcome out;
  mod::Manifest merged;
  std::map<std::string, fs::path> sources;  // platform -> the library's path
  for (const fs::path& p : o.packages) {
    const fs::path manifest = fs::is_directory(p) ? p / mod::kManifestName : p;
    mod::Manifest m;
    const mod::ManifestCheck c =
        mod::parse_manifest(read_text(manifest).value_or(""), manifest.generic_string(), m);
    if (!c.ok()) {
      for (const std::string& e : c.errors)
        out.findings.push_back(mod_finding(Severity::error, e, manifest.generic_string()));
      return out;
    }
    if (merged.name.empty()) {
      merged = m;
      merged.files.clear();
    } else if (m.name != merged.name || m.version != merged.version || m.game != merged.game ||
               m.game_api_min != merged.game_api_min || m.game_api_max != merged.game_api_max ||
               m.abi_major != merged.abi_major || m.abi_minor != merged.abi_minor) {
      out.findings.push_back({Severity::error,
                              manifest.generic_string() + " is " + m.name + " " + m.version +
                                  ", not the " + merged.name + " " + merged.version +
                                  " (same game, game_api and abi) the first package is",
                              manifest.generic_string(),
                              "pack only builds of one version of one mod"});
      return out;
    }
    for (const mod::ManifestFile& f : m.files) {
      if (sources.count(f.platform)) {
        out.findings.push_back({Severity::error, "two packages list " + f.platform,
                                manifest.generic_string(), "pack one build a platform"});
        return out;
      }
      sources[f.platform] = manifest.parent_path() / f.path;
      merged.files.push_back(f);
    }
  }
  const fs::path dest =
      o.out.empty() ? fs::path("dist") / (merged.name + "-" + merged.version) : fs::path(o.out);
  std::error_code ec;
  fs::create_directories(dest, ec);
  for (mod::ManifestFile& f : merged.files) {
    const fs::path to = dest / fs::path(f.path).filename();
    fs::copy_file(sources[f.platform], to, fs::copy_options::overwrite_existing, ec);
    if (ec) {
      out.findings.push_back(
          {Severity::error,
           "cannot copy " + sources[f.platform].generic_string() + ": " + ec.message(),
           dest.generic_string(), "check the package's files, then rerun"});
      return out;
    }
    f.path = fs::path(f.path).filename().generic_string();
    out.written.push_back(to.generic_string());
  }
  std::string error;
  if (!write_text(dest / mod::kManifestName, mod::write_manifest(merged), error)) {
    out.findings.push_back({Severity::error, error, dest.generic_string(), "check --out"});
    return out;
  }
  out.written.push_back((dest / mod::kManifestName).generic_string());
  // The merged package is checked as a game would load it, when it holds this platform's file.
  if (!mod::file_for(merged, mod::this_platform())) {
    out.findings.push_back({Severity::note,
                            "the package has no library for " + std::string(mod::this_platform()) +
                                ", so it was not loaded here",
                            dest.generic_string(),
                            "run " + program + " mod check on a platform it lists"});
  }
  mod::LoadedMod m;
  if (mod::file_for(merged, mod::this_platform())) {
    const mod::LoadResult r = mod::load_mod(dest, offer_of(merged.game), m);
    if (!loader_findings(r, dest.generic_string(), out)) return out;
  }
  std::vector<std::string> platforms;
  platforms.reserve(merged.files.size());
  for (const mod::ManifestFile& f : merged.files) platforms.push_back(f.platform);
  out.result = json_obj({{"dir", json_str(dest.generic_string())},
                         {"name", json_str(merged.name)},
                         {"version", json_str(merged.version)},
                         {"platforms", json_strs(platforms)}});
  out.summary = merged.name + " " + merged.version + " packed for " +
                std::to_string(platforms.size()) + " platform(s) in " + dest.generic_string();
  out.next = program + " mod check " + dest.generic_string();
  return out;
}

Outcome mod_key(const std::string& program) {
  Outcome out;
  net::Key seed{};
  net::os_random_bytes(seed.data(), seed.size());
  net::SignKeys keys = net::sign_keys(seed);
  const std::string secret = net::key_to_hex(seed), pub = net::key_to_hex(keys.pub);
  net::wipe(seed.data(), seed.size());
  net::wipe(keys.secret.data(), keys.secret.size());
  out.result = json_obj({{"secret", json_str(secret)}, {"public", json_str(pub)}});
  out.summary =
      "a new signing key: keep the secret out of every repository; the public key is "
      "the line a game's trusted keys list: " +
      pub + " <your name>";
  out.next = "export GE_MOD_KEY=<the secret>, then " + program + " mod sign <package>";
  return out;
}

Outcome mod_sign(const std::string& program, const ModArgs& o) {
  Outcome out;
  const std::string var = o.key_env.empty() ? "GE_MOD_KEY" : o.key_env;
  net::Key seed{};
  std::string error;
  if (!net::key_from_env(var.c_str(), seed, error)) {
    out.findings.push_back({Severity::error, error, "the environment",
                            program + " mod key makes a key; export its secret as " + var});
    out.exit = 2;
    return out;
  }
  const fs::path dir = fs::is_directory(o.dir) ? o.dir : o.dir.parent_path();
  const fs::path manifest = dir / mod::kManifestName;
  const std::optional<std::string> text = read_text(manifest);
  if (!text) {
    out.findings.push_back(
        {Severity::error, manifest.generic_string() + ": no manifest there",
         manifest.generic_string(),
         "point " + program + " mod sign at a package (a mod's build writes one)"});
    return out;
  }
  // The package is checked first: a signature vouches for what is there, so it must load.
  mod::LoadedMod m;
  mod::Manifest peek;
  (void)mod::parse_manifest(*text, manifest.generic_string(), peek);
  if (mod::file_for(peek, mod::this_platform())) {
    const mod::LoadResult r = mod::load_mod(dir, offer_of(peek.game), m);
    if (!r.ok() && m.signature.state != mod::Signed::broken) {
      loader_findings(r, dir.generic_string(), out);
      return out;
    }
  }
  net::SignKeys keys = net::sign_keys(seed);
  net::wipe(seed.data(), seed.size());
  const std::string sig = mod::signature_text(keys, *text);
  net::wipe(keys.secret.data(), keys.secret.size());
  if (!write_text(dir / mod::kSignatureName, sig, error)) {
    out.findings.push_back({Severity::error, error, dir.generic_string(), "check the package"});
    return out;
  }
  out.written.push_back((dir / mod::kSignatureName).generic_string());
  const std::string pub = net::key_to_hex(keys.pub);
  out.result = json_obj({{"name", json_str(peek.name)},
                         {"version", json_str(peek.version)},
                         {"public", json_str(pub)}});
  out.summary = peek.name + " " + peek.version + " signed by " + pub;
  out.next = program + " mod check " + dir.generic_string() + " --trusted=<a file with: " + pub +
             " <your name>>";
  return out;
}

std::span<const Row> mod_rows() { return kRows; }

}  // namespace ge::toolkit
