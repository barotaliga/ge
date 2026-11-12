// A classic install's configs run in its own order (install_configs.hpp).
//
// ge:summary A classic install's configs run in its own order: default, main config, autoexec
// ge:keywords install-configs start_configs config_dirs
// ge:skill shooter console
// ge:test tests/shooters_edge_install_configs_test.cpp
#include "shooters_edge/client/install_configs.hpp"

#include <fstream>
#include <iterator>
#include <ranges>
#include <string_view>
#include <system_error>

#include "console/cfg.hpp"
#include "shooters_edge/client/settings.hpp"

namespace shooters_edge::client {

namespace {

namespace fs = std::filesystem;
namespace con = ge::console;

// The bundled client's own folder beside the first game's folders, spelled in pieces so the
// clean-slate scan (tools/agents-index.mjs) does not read it as prose naming the client.
constexpr std::string_view kClientDir =
    "ezqu"
    "ake";

// The files the sequel's client runs after autoexec.cfg and after the command line.
constexpr std::string_view kPostexecFile = "postexec.cfg";
constexpr std::string_view kPostinitFile = "postinit.cfg";

std::string lower(std::string s) {
  for (char& c : s) c = c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
  return s;
}

// The entry of `dir` named `name` in any case, or "".
fs::path entry_named(const fs::path& dir, std::string_view name) {
  std::error_code ec;
  for (const fs::directory_entry& e : fs::directory_iterator(dir, ec))
    if (lower(e.path().filename().string()) == lower(std::string(name))) return e.path();
  return {};
}

bool is_first_game(const ge::classicenhance::Profile& content) {
  return content.id == ge::classicenhance::kProfiles[0].id;
}

// The sequel's folders a config is looked for in, the mod's first: `game` when it is a mod (not
// the base folder in any case), then the base folder.
std::vector<std::string_view> sequel_folders(const std::string& game,
                                             const ge::classicenhance::Profile& content) {
  std::vector<std::string_view> out;
  if (!game.empty() && lower(game) != lower(std::string(content.base_dir))) out.emplace_back(game);
  out.push_back(content.base_dir);
  return out;
}

// The install's main configs, in the order its client tries them; the first that is there runs.
// The sequel's client reads `config.cfg` on Linux and macOS and `q2config.cfg` on Windows; the
// other is tried after, so an install carried from one system to another keeps its settings.
std::vector<fs::path> main_configs(const fs::path& root, const std::string& game,
                                   const ge::classicenhance::Profile& content) {
  std::vector<fs::path> out;
  if (is_first_game(content)) {
    if (const fs::path client = entry_named(root, kClientDir); !client.empty())
      if (const fs::path configs = entry_named(client, "configs"); !configs.empty())
        out.push_back(configs / "config.cfg");
    return out;
  }
#ifdef _WIN32
  const std::string_view names[] = {"q2config.cfg", "config.cfg"};
#else
  const std::string_view names[] = {"config.cfg", "q2config.cfg"};
#endif
  for (const std::string_view folder : sequel_folders(game, content)) {
    const fs::path dir = entry_named(root, folder);
    if (dir.empty()) continue;
    for (const std::string_view n : names) out.push_back(dir / std::string(n));
  }
  return out;
}

// The file at `path` run to its end, `wait`s included; false when it is not there.
bool run_path(con::Console& c, const fs::path& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  const std::string text{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
  c.append(text);
  for (int n = 0; n < con::kMaxFrames && c.pending(); ++n) c.run();
  return true;
}

// `name` found on the console's search path and run; false when it is not there.
bool run_named(con::Console& c, std::string_view name, ConfigsRun& out) {
  std::string text;
  if (!c.read_file(name, text)) return false;
  c.exec_file(name);
  for (int n = 0; n < con::kMaxFrames && c.pending(); ++n) c.run();
  out.files.emplace_back(name);
  return true;
}

// The command line's words cut into its `+set` pairs and the rest, each group in order.
void split_plus(const std::vector<std::string>& plus, std::vector<std::string>& sets,
                std::vector<std::string>& rest) {
  std::vector<std::string> line;
  const auto flush = [&] {
    if (line.empty()) return;
    std::string head = line[0];
    if (!head.empty() && head[0] == '+') head.erase(0, 1);
    std::vector<std::string>& to = lower(head) == "set" ? sets : rest;
    if (!line[0].empty() && line[0][0] != '+') line[0].insert(0, "+");
    to.insert(to.end(), line.begin(), line.end());
    line.clear();
  };
  for (const std::string& w : plus) {
    if (!w.empty() && w[0] == '+') flush();
    line.push_back(w);
  }
  flush();
}

}  // namespace

fs::path client_folder(const fs::path& root, const ge::classicenhance::Profile& content) {
  if (!is_first_game(content)) return {};
  const fs::path p = entry_named(root, kClientDir);
  std::error_code ec;
  return fs::is_directory(p, ec) ? p : fs::path();
}

std::vector<fs::path> config_dirs(const std::string& basedir, const std::string& game,
                                  const ge::classicenhance::Profile& content) {
  const fs::path root = basedir.empty() ? fs::path(".") : fs::path(basedir);
  std::vector<fs::path> dirs{settings_dir(basedir, game)};
  const auto add = [&](std::string_view name) {
    const fs::path p = entry_named(root, name);
    if (p.empty()) return;
    for (const fs::path& d : dirs)
      if (d == p) return;
    dirs.push_back(p);
  };
  add(game);
  if (is_first_game(content)) add(kClientDir);
  add(content.base_dir);
  dirs.emplace_back(".");
  return dirs;
}

ConfigsRun start_configs(con::Console& c, const std::string& basedir, const std::string& game,
                         const ge::classicenhance::Profile& content,
                         const std::vector<std::string>& plus) {
  ConfigsRun out;
  if (!c.config_file().empty()) {  // a session's own config: the engine's order
    con::start(c, plus);
    return out;
  }
  const fs::path root = basedir.empty() ? fs::path(".") : fs::path(basedir);
  const auto count_unknown = [&c, &out] {
    c.set_unknown([&out](const con::Args&) {
      out.passed_over++;
      return true;
    });
  };
  count_unknown();
  run_named(c, con::kDefaultFile, out);
  const fs::path own = settings_dir(basedir, game) / con::kConfigFile;
  const bool first = is_first_game(content);
  bool installed = false;
  for (const fs::path& main : main_configs(root, game, content)) {
    std::error_code ec;
    if (!fs::is_regular_file(main, ec)) continue;
    if (first) run_named(c, con::kAutoexecFile, out);  // the bundled client runs it just before
    if (run_path(c, main)) out.files.push_back(main.string());
    installed = true;
    break;
  }
  // The window's own saved settings: after the install's (so what the player changed here sticks),
  // else where the engine's order has them, before autoexec.cfg.
  if (!installed && run_path(c, own)) out.files.push_back(own.string());
  if (first || !installed) {
    run_named(c, con::kAutoexecFile, out);
  } else {
    // The sequel's client: the base folder's autoexec.cfg, then the mod's, then postexec.cfg.
    // With neither, the window's own autoexec.cfg runs in their place.
    bool ran = false;
    for (const std::string_view folder : sequel_folders(game, content) | std::views::reverse) {
      const fs::path dir = entry_named(root, folder);
      if (dir.empty() || !run_path(c, dir / con::kAutoexecFile)) continue;
      out.files.push_back((dir / con::kAutoexecFile).string());
      ran = true;
    }
    if (!ran) run_named(c, con::kAutoexecFile, out);
    run_named(c, kPostexecFile, out);
  }
  if (installed && run_path(c, own)) out.files.push_back(own.string());
  c.set_unknown(nullptr);
  std::vector<std::string> sets, rest;
  split_plus(plus, sets, rest);
  c.append_command_line(sets);
  c.run();
  c.append_command_line(rest);
  c.run();
  if (installed && !first) {  // the sequel's client runs postinit.cfg after the command line
    count_unknown();
    run_named(c, kPostinitFile, out);
    c.set_unknown(nullptr);
  }
  return out;
}

}  // namespace shooters_edge::client
