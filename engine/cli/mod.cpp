// `ge mod` (mod.hpp): the template written, CMake run, the engine's loader asked, packages merged.
//
// ge:summary ge mod's definitions: the template filled, the build, the loader's check, the merge
// ge:keywords cli ge mod new build check pack template cmake loader manifest merge
// ge:skill agent-interface mod
// ge:test tests/cli_mod_test.cpp
#include "cli/mod.hpp"

#include <map>

#include "cli/build.hpp"
#include "cli/embedded.hpp"
#include "cli/json.hpp"
#include "cli/scaffold.hpp"
#include "mod/loader.hpp"
#include "mod/manifest.hpp"

namespace ge::cli {

namespace {

constexpr std::string_view kTemplates = "templates/mod-";

std::string engine_tree(const std::string& flag) {
#ifdef GE_ENGINE_SOURCE_DIR
  return flag.empty() ? std::string(GE_ENGINE_SOURCE_DIR) : flag;
#else
  return flag;
#endif
}

void replace_all(std::string& s, std::string_view from, std::string_view to) {
  for (size_t at = s.find(from); at != std::string::npos; at = s.find(from, at + to.size()))
    s.replace(at, from.size(), to);
}

std::string quoted(const fs::path& p) { return "\"" + p.generic_string() + "\""; }

std::string games_list() {
  std::string s;
  for (const ModGame& g : kModGames) s += (s.empty() ? "" : ", ") + std::string(g.id);
  return s;
}

// The loader's answer as findings; `true` when it loaded.
bool loader_findings(const mod::LoadResult& r, const std::string& where, Outcome& out) {
  for (const std::string& w : r.warnings)
    out.findings.push_back(mod_finding(Severity::warn, w, where));
  for (const std::string& e : r.errors)
    out.findings.push_back(mod_finding(Severity::error, e, where));
  return r.ok();
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
                   {"hooks_bytes", json_num(hooks)}});
}

mod::GameOffer offer_of(const std::string& game) {
  const ModGame* g = find_mod_game(game);
  return g ? mod::GameOffer{g->id, g->api} : mod::GameOffer{};
}

}  // namespace

const ModGame* find_mod_game(std::string_view id) {
  for (const ModGame& g : kModGames)
    if (g.id == id) return &g;
  return nullptr;
}

Finding mod_finding(Severity s, const std::string& message, const std::string& where) {
  const size_t at = message.find("; next: ");
  if (at == std::string::npos) return {s, message, where, "see docs/mods.md"};
  return {s, message.substr(0, at), where, message.substr(at + 8)};
}

Outcome run_mod_new(const ModOptions& o) {
  Outcome out;
  const ModGame* game = find_mod_game(o.game);
  if (!game) {
    out.findings.push_back({Severity::error, "no game `" + o.game + "` offers mods",
                            "the command line", "pass --game=<id>, one of: " + games_list()});
    out.exit = 2;
    return out;
  }
  std::error_code ec;
  if (fs::exists(o.dir, ec) && !fs::is_empty(o.dir, ec)) {
    out.findings.push_back({Severity::error, o.dir.generic_string() + " exists and is not empty",
                            o.dir.generic_string(), "pick a new directory for the mod"});
    return out;
  }
  const std::string name =
      o.name.empty() ? fs::absolute(o.dir).lexically_normal().filename().string() : o.name;
  if (!valid_game_name(name)) {
    out.findings.push_back({Severity::error, "`" + name + "` is not a kebab-case mod name",
                            "the command line",
                            "name the directory (or pass --name=) in lower-case letters, digits "
                            "and '-', starting with a letter"});
    out.exit = 2;
    return out;
  }
  const std::string engine = engine_tree(o.engine);
  const std::string prefix = std::string(kTemplates) + std::string(game->id) + "/";
  const std::string ident = ident_of(name);
  for (const EmbeddedFile& f : embedded_files()) {
    if (!f.path.starts_with(prefix)) continue;
    std::string rel(f.path.substr(prefix.size()));
    if (rel.ends_with(".in")) rel.resize(rel.size() - 3);
    std::string text(f.text);
    replace_all(text, "{{name}}", name);
    replace_all(text, "{{ident}}", ident);
    replace_all(text, "{{engine}}", fs::path(engine).generic_string());
    replace_all(text, "{{game}}", game->id);
    replace_all(text, "{{game_include}}", game->include);
    replace_all(text, "{{game_header}}", game->header);
    std::string error;
    if (!write_text(o.dir / rel, text, error)) {
      out.findings.push_back(
          {Severity::error, error, rel, "check that the directory is writable, then rerun"});
      return out;
    }
    out.written.push_back(rel);
  }
  if (out.written.empty()) {
    out.findings.push_back({Severity::error, "this ge carries no template for " + o.game,
                            std::string(kTemplates) + std::string(game->id),
                            "report it: the engine's templates/ lacks the game's mod template"});
    return out;
  }
  out.result = json_obj({{"dir", json_str(o.dir.generic_string())},
                         {"name", json_str(name)},
                         {"game", json_str(game->id)},
                         {"engine", json_str(fs::path(engine).generic_string())}});
  out.summary = "a " + std::string(game->id) + " mod in " + o.dir.generic_string() +
                "; src/mod.cpp is the mod";
  out.next = "ge mod build " + o.dir.generic_string();
  return out;
}

Outcome run_mod_build(const ModOptions& o) {
  Outcome out;
  const fs::path dir = fs::absolute(o.dir).lexically_normal();
  const fs::path build = dir / "build";
  const fs::path log = build / "ge-mod-build.log";
  std::error_code ec;
  fs::create_directories(build, ec);
  fs::remove(log, ec);
  if (!fs::exists(dir / "CMakeLists.txt")) {
    out.findings.push_back({Severity::error, "no CMakeLists.txt in " + dir.generic_string(),
                            dir.generic_string(), "run ge mod new <dir> --game=<id> first"});
    return out;
  }
  std::string configure =
      "cmake -S " + quoted(dir) + " -B " + quoted(build) + " -DCMAKE_BUILD_TYPE=Release";
  if (!o.engine.empty()) configure += " -DGE_ENGINE_DIR=" + quoted(fs::absolute(o.engine));
  for (const std::string& cmd :
       {configure, "cmake --build " + quoted(build) + " --config Release"}) {
    const Logged r = run_to_log(cmd, log);
    if (r.code != 0) {
      out.findings = compiler_findings(dir, r.output, 20);
      out.findings.push_back({Severity::error,
                              "`" + cmd + "` failed (exit " + std::to_string(r.code) + ")",
                              rel_path(dir, log), "read the findings above, fix, rerun"});
      return out;
    }
  }
  // The package the build wrote: build/mods/<name>/mod.manifest (cmake/Mods.cmake).
  fs::path manifest;
  for (const auto& e : fs::directory_iterator(build / "mods", ec))
    if (fs::exists(e.path() / mod::kManifestName)) manifest = e.path() / mod::kManifestName;
  if (manifest.empty()) {
    out.findings.push_back({Severity::error, "the build wrote no package under build/mods/",
                            rel_path(dir, log),
                            "the CMakeLists.txt must call ge_add_mod (cmake/Mods.cmake)"});
    return out;
  }
  mod::LoadedMod m;
  mod::Manifest peek;
  const std::string text = read_text(manifest).value_or("");
  (void)mod::parse_manifest(text, manifest.generic_string(), peek);
  const mod::LoadResult r = mod::load_mod(manifest, offer_of(peek.game), m);
  if (!loader_findings(r, rel_path(dir, manifest), out)) return out;
  out.written.push_back(rel_path(dir, manifest.parent_path()));
  out.result = table_json(m);
  out.summary = "built " + m.manifest.name + " " + m.manifest.version + " for " + m.platform +
                ": " + rel_path(dir, manifest.parent_path()) + " is the package";
  out.next = "ge mod check " + rel_path(fs::current_path(), manifest.parent_path());
  return out;
}

Outcome run_mod_check(const ModOptions& o) {
  Outcome out;
  mod::LoadedMod m;
  mod::Manifest peek;
  const fs::path manifest = fs::is_directory(o.dir) ? o.dir / mod::kManifestName : fs::path(o.dir);
  (void)mod::parse_manifest(read_text(manifest).value_or(""), manifest.generic_string(), peek);
  const std::string game = o.game.empty() ? peek.game : o.game;
  if (!o.game.empty() && !find_mod_game(o.game)) {
    out.findings.push_back({Severity::error, "no game `" + o.game + "` offers mods",
                            "the command line", "pass --game=<id>, one of: " + games_list()});
    out.exit = 2;
    return out;
  }
  const mod::LoadResult r = mod::load_mod(o.dir, offer_of(game), m);
  if (!loader_findings(r, o.dir.generic_string(), out)) return out;
  out.result = table_json(m);
  out.summary = m.manifest.name + " " + m.manifest.version + " loads for " + m.manifest.game +
                " on " + m.platform + " (mod ABI " + std::to_string(m.manifest.abi_major) + "." +
                std::to_string(m.manifest.abi_minor) + ")";
  out.next = "ge mod pack " + o.dir.generic_string();
  return out;
}

Outcome run_mod_pack(const ModOptions& o) {
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
                            dest.generic_string(), "run ge mod check on a platform it lists"});
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
  out.next = "ge mod check " + dest.generic_string();
  return out;
}

}  // namespace ge::cli
