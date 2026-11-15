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
#include "cli/json_tree.hpp"
#include "cli/scaffold.hpp"
#include "core/json_read.hpp"
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

// The game's CLI that plays its mod scenario: --runner (a file, or the folder it is in), else the
// one built beside this ge, else its name for the PATH.
std::string runner_of(const ModOptions& o, std::string_view cli) {
#if defined(_WIN32)
  std::string exe = std::string(cli) + ".exe";
#else
  std::string exe(cli);
#endif
  std::error_code ec;
  if (!o.runner.empty())
    return fs::is_directory(o.runner, ec) ? (fs::path(o.runner) / exe).string() : o.runner;
#ifdef GE_CLI_BUILD_DIR
  const fs::path built = fs::path(GE_CLI_BUILD_DIR) / exe;
  if (fs::exists(built, ec)) return built.string();
#endif
  return exe;
}

// `--run`: the package played by the game's CLI in its mod scenario (`<cli> mod run <package>
// --json`); its answer joins `out` (the summary, `run` in the result) or refuses it.
void play_in_game(const ModOptions& o, const std::string& game_id, Outcome& out) {
  const ModGame* game = find_mod_game(game_id);
  if (game == nullptr || game->cli.empty()) {
    out.findings.push_back({Severity::error, game_id + " has no CLI that plays its mods",
                            "the command line", "check without --run"});
    return;
  }
  const std::string runner = runner_of(o, game->cli);
  const fs::path log = fs::temp_directory_path() / "ge-mod-run.log";
  std::error_code ec;
  fs::remove(log, ec);
  const Logged r = run_to_log(quoted(fs::path(runner)) + " mod run " + quoted(fs::absolute(o.dir)) +
                                  (o.wasm ? " --wasm" : "") + " --json",
                              log);
  // The answer is the last line that is a JSON object; anything before it is the game's log.
  JsonNode answer;
  std::string error;
  bool parsed = false;
  for (size_t end = r.output.size(); end > 0 && !parsed;) {
    const size_t start = r.output.rfind('\n', end - 1);
    const size_t from = start == std::string::npos ? 0 : start + 1;
    const std::string line = r.output.substr(from, end - from);
    if (!line.empty() && line.front() == '{') parsed = parse_json(line, answer, error);
    end = start == std::string::npos ? 0 : start;
  }
  if (!parsed) {
    out.findings.push_back(
        {Severity::error,
         "`" + runner + " mod run` gave no answer (exit " + std::to_string(r.code) + ")",
         log.generic_string(),
         "pass --runner=<the game's " + std::string(game->cli) +
             ", or the folder it is in>; a game's CLI is built beside its game"});
    return;
  }
  const JsonNode* ok = answer.find("ok");
  if (ok == nullptr || !ok->boolean) {
    const JsonNode* e = answer.find("error");
    const JsonNode* n = answer.find("next");
    out.findings.push_back({Severity::error,
                            "in the game's mod scenario: " + (e ? e->text : "refused"),
                            o.dir.generic_string(), n ? n->text : "see docs/mods.md"});
    return;
  }
  JsonNode run;
  const JsonNode* res = answer.find("result");
  if (res != nullptr) run = *res;
  const JsonNode* changed = run.find("changed");
  if (changed != nullptr && !changed->boolean)
    out.findings.push_back({Severity::warn, "the mod changed nothing in the game's mod scenario",
                            o.dir.generic_string(),
                            "check its hooks: the scenario fires rockets and grenades"});
  JsonNode result;
  if (parse_json(out.result, result, error)) {
    result.at("run") = run;
    out.result = dump_json(result);
  }
  const JsonNode* frames = run.find("frames");
  const JsonNode* calls = run.find("hook_calls");
  const JsonNode* pct = run.find("cost_percent");
  const bool noise = pct == nullptr || pct->number() <= 0;
  out.summary += "; replays in " + std::string(game->id) + "'s mod scenario over " +
                 (frames ? frames->text : "?") + " frames, " + (calls ? calls->text : "0") +
                 " hook calls costing " +
                 (noise ? std::string("too little to time") : "about " + pct->text + "% of it");
}

}  // namespace

Outcome run_mod_new(const ModOptions& o) {
  Outcome out;
  const ModGame* game = find_mod_game(o.game);
  if (!game) {
    out.findings.push_back({Severity::error, "no game `" + o.game + "` offers mods",
                            "the command line",
                            "pass --game=<id>, one of: " + toolkit::mod_games_list()});
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
  if (!fs::exists(fs::path(engine) / "cmake" / "Mods.cmake", ec)) {
    out.findings.push_back(
        {Severity::error,
         "no engine to build against at `" + engine + "` (it has no cmake/Mods.cmake)",
         "the command line",
         "pass --engine=<the engine tree, or an installed package's share/gameengine2/sdk>"});
    out.exit = 2;
    return out;
  }
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
  configure += o.wasm ? " -DGE_MOD_BUILD_WASM=ON" : " -DGE_MOD_BUILD_WASM=OFF";
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
  // The package the build wrote, checked as `mod check` checks one.
  toolkit::ModArgs check;
  check.dir = manifest;
  const Outcome checked = toolkit::mod_check("ge", check);
  out.findings.insert(out.findings.end(), checked.findings.begin(), checked.findings.end());
  if (exit_of(checked) != 0) return out;
  if (o.wasm) {  // and its module, as a store build loads it
    check.wasm = true;
    const Outcome module = toolkit::mod_check("ge", check);
    out.findings.insert(out.findings.end(), module.findings.begin(), module.findings.end());
    if (exit_of(module) != 0) return out;
  }
  core::FlatJson table;
  std::string error;
  (void)core::read_flat(checked.result, table, error);
  out.written.push_back(rel_path(dir, manifest.parent_path()));
  out.result = checked.result;
  out.summary = "built " + table["name"].text + " " + table["version"].text + " for " +
                table["platform"].text + ": " + rel_path(dir, manifest.parent_path()) +
                " is the package";
  out.next = "ge mod check " + rel_path(fs::current_path(), manifest.parent_path());
  return out;
}

toolkit::ModArgs mod_args(const ModOptions& o) {
  toolkit::ModArgs a;
  a.dir = o.dir;
  a.packages = o.packages;
  a.game = o.game;
  a.out = o.out;
  a.key_env = o.key_env;
  a.trusted = o.trusted;
  a.trusted_only = o.trusted_only;
  a.wasm = o.wasm;
  return a;
}

Outcome run_mod_check(const ModOptions& o) {
  Outcome out = toolkit::mod_check("ge", mod_args(o));
  if (o.run && exit_of(out) == 0) {
    core::FlatJson table;
    std::string error;
    (void)core::read_flat(out.result, table, error);
    play_in_game(o, table["game"].text, out);
  }
  return out;
}

Outcome run_mod_pack(const ModOptions& o) { return toolkit::mod_pack("ge", mod_args(o)); }
Outcome run_mod_key() { return toolkit::mod_key("ge"); }
Outcome run_mod_sign(const ModOptions& o) { return toolkit::mod_sign("ge", mod_args(o)); }

}  // namespace ge::cli
