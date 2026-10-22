// The definitions behind scaffold.hpp: the scaffold's files written with their placeholders
// filled, the links made, the skills synced.
//
// ge:summary ge new's definition: scaffold files filled in, links made, engine skills synced
// ge:keywords cli ge new scaffold placeholders symlinks new_game valid_game_name ident_of
// ge:skill agent-interface
// ge:test tests/cli_scaffold_test.cpp
#include "cli/scaffold.hpp"

#include <algorithm>
#include <map>
#include <regex>
#include <system_error>

#include "cli/embedded.hpp"
#include "cli/skills.hpp"

namespace ge::cli {

namespace {

constexpr std::string_view kScaffold = "scaffold/";
constexpr std::string_view kOverlays = "templates/game-";

void replace_all(std::string& s, std::string_view from, std::string_view to) {
  for (size_t at = s.find(from); at != std::string::npos; at = s.find(from, at + to.size())) {
    s.replace(at, from.size(), to);
  }
}

}  // namespace

bool valid_game_name(std::string_view name) {
  static const std::regex kName(R"(^[a-z][a-z0-9]*(-[a-z0-9]+)*$)");
  return std::regex_match(std::string(name), kName);
}

std::string ident_of(std::string_view name) {
  std::string s(name);
  for (char& c : s) c = c == '-' ? '_' : c;
  return s;
}

NewResult new_game(const NewOptions& options) {
  NewResult r;
  const fs::path dir = options.dir;
  fs::path base = fs::absolute(dir).lexically_normal();
  if (!base.has_filename()) base = base.parent_path();  // "games/snow/" names "snow"
  const std::string name = options.name.empty() ? base.filename().string() : options.name;
  if (!valid_game_name(name)) {
    r.findings.push_back({Severity::error,
                          "\"" + name +
                              "\" is not a game name: lower-case kebab, starting with "
                              "a letter (snow-racer)",
                          dir.generic_string(), "rerun with --name=<kebab-name>"});
    return r;
  }
  if (!options.engine_sha256.empty() &&
      !std::regex_match(options.engine_sha256, std::regex("^[0-9a-f]{64}$"))) {
    r.findings.push_back({Severity::error, "--engine-sha256 is not 64 lower-case hex digits",
                          dir.generic_string(),
                          "copy the -src.tar.gz line's checksum from the release's SHA256SUMS"});
    return r;
  }
  std::error_code ec;
  if (fs::exists(dir, ec) && !fs::is_empty(dir, ec)) {
    r.findings.push_back({Severity::error, "the directory exists and is not empty",
                          dir.generic_string(),
                          "name a new directory: ge new <dir>; ge new never writes over a repo "
                          "(an existing one takes `make skills` and `make agents-check`)"});
    return r;
  }

  if (std::find(std::begin(kGames), std::end(kGames), options.game) == std::end(kGames)) {
    std::string games;
    for (const std::string_view g : kGames) games += std::string(games.empty() ? "" : ", ") += g;
    r.findings.push_back({Severity::error, "no starting game \"" + options.game + "\"",
                          dir.generic_string(), "rerun with --game= one of " + games});
    return r;
  }

  // The scaffold's files, each replaced by the starting game's of the same path, then the
  // starting game's own.
  const std::string overlay =
      options.game == "demo" ? "" : std::string(kOverlays) + options.game + "/";
  std::map<std::string, std::string_view> files;
  for (const EmbeddedFile& f : embedded_files()) {
    if (f.path.starts_with(kScaffold)) files.emplace(f.path.substr(kScaffold.size()), f.text);
  }
  for (const EmbeddedFile& f : embedded_files()) {
    if (!overlay.empty() && f.path.starts_with(overlay)) {
      files[std::string(f.path.substr(overlay.size()))] = f.text;
    }
  }

  const std::string ident = ident_of(name);
  for (const auto& [path, body] : files) {
    std::string rel = path;
    if (rel.ends_with(".in")) rel.resize(rel.size() - 3);
    std::string text(body);
    replace_all(text, "{{name}}", name);
    replace_all(text, "{{ident}}", ident);
    replace_all(text, "{{engine_version}}", engine_version());
    replace_all(text, "{{engine_sha256}}", options.engine_sha256);
    replace_all(text, "{{engine_repo}}", kEngineRepo);
    std::string error;
    if (!write_text(dir / rel, text, error)) {
      r.findings.push_back(
          {Severity::error, error, rel, "check that the directory is writable, then rerun"});
      return r;
    }
    r.written.push_back(rel);
  }

  for (const Link& link : kLinks) {
    const fs::path path = dir / link.path;
    fs::create_directories(path.parent_path(), ec);
    if (link.directory) {
      fs::create_directory_symlink(link.target, path, ec);
    } else {
      fs::create_symlink(link.target, path, ec);
    }
    if (ec) {
      r.findings.push_back({Severity::error,
                            "cannot create the symlink " + std::string(link.path) + " -> " +
                                std::string(link.target) + ": " + ec.message(),
                            std::string(link.path),
                            "on Windows, turn on Developer Mode (or run as administrator) and "
                            "set `git config core.symlinks true`, then rerun ge new"});
      return r;
    }
    r.written.push_back(std::string(link.path));
  }

  SyncResult sync = sync_skills(dir, false);
  r.written.insert(r.written.end(), sync.written.begin(), sync.written.end());
  r.findings.insert(r.findings.end(), sync.findings.begin(), sync.findings.end());
  return r;
}

}  // namespace ge::cli
