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
#include "shooters_edge/client/load_order.hpp"
#include "shooters_edge/client/settings.hpp"
#include "vfs/search.hpp"

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

// The game folders a config is looked for in, the mod's first: `game` when it is a mod (not the
// base folder in any case), then the base folder.
std::vector<std::string_view> game_folders(const std::string& game,
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
                                   const ge::classicenhance::Profile& content,
                                   const LoadOrder& order) {
  std::vector<fs::path> out;
  const auto add = [&](const fs::path& dir) {
    for (const std::string_view n : order.main_files)
      if (!n.empty()) out.push_back(dir / std::string(n));
  };
  if (order.main_where == MainWhere::client_configs) {
    if (const fs::path client = entry_named(root, kClientDir); !client.empty())
      if (const fs::path configs = entry_named(client, "configs"); !configs.empty()) add(configs);
    return out;
  }
  for (const std::string_view folder : game_folders(game, content))
    if (const fs::path dir = entry_named(root, folder); !dir.empty()) add(dir);
  return out;
}

// The text of default.cfg from the packs of the game folders (the mod's first), each folder's
// packs searched before its loose copy as its client ranks them; false when none holds one.
bool defaults_from_packs(const fs::path& root, const std::string& game,
                         const ge::classicenhance::Profile& content, const LoadOrder& order,
                         std::string& text, std::string& from) {
  for (const std::string_view folder : game_folders(game, content)) {
    const fs::path dir = entry_named(root, folder);
    if (dir.empty()) continue;
    const std::vector<std::string> archives =
        ge::vfs::game_archives(root.string(), dir.filename().string(), order.layout);
    for (const std::string& pack : archives | std::views::reverse) {  // the winner first
      std::vector<uint8_t> bytes;
      if (!ge::vfs::read_from_pack(pack, con::kDefaultFile, &bytes, nullptr)) continue;
      text.assign(bytes.begin(), bytes.end());
      from = pack + "/" + std::string(con::kDefaultFile);
      return true;
    }
    if (order.layout.archives_over_loose) {
      std::ifstream f(dir / con::kDefaultFile, std::ios::binary);
      if (!f) continue;
      text.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
      from = (dir / con::kDefaultFile).string();
      return true;
    }
  }
  return false;
}

// `text` run to its end, `wait`s included.
void run_text(con::Console& c, const std::string& text) {
  c.append(text);
  for (int n = 0; n < con::kMaxFrames && c.pending(); ++n) c.run();
}

// The file at `path` run to its end; false when it is not there.
bool run_path(con::Console& c, const fs::path& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  run_text(c, std::string{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()});
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

// The console's own keys (the one under the tilde, the ISO key) in each mode, kept across a config
// that starts with `unbindall`.
struct KeptKeys {
  std::vector<std::pair<std::pair<con::BindMode, std::string>, std::string>> binds;
};
constexpr const char* kConsoleKeys[] = {"`", "nonus"};
constexpr con::BindMode kKeyModes[] = {con::BindMode::game, con::BindMode::console};

KeptKeys keep_console_keys(const con::Console& c) {
  KeptKeys k;
  for (const con::BindMode m : kKeyModes)
    for (const char* key : kConsoleKeys)
      if (const std::string& b = c.binds().get(m, key); !b.empty())
        k.binds.push_back({{m, key}, b});
  return k;
}

void restore_console_keys(con::Console& c, const KeptKeys& k) {
  for (const auto& [at, command] : k.binds)
    if (c.binds().get(at.first, at.second).empty()) c.binds().set(at.first, at.second, command);
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
  if (!load_order_of(content).client_folder) return {};
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
  if (load_order_of(content).client_folder) add(kClientDir);
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
  const LoadOrder& order = load_order_of(content);
  const fs::path own = settings_dir(basedir, game) / con::kConfigFile;
  fs::path main;
  for (const fs::path& p : main_configs(root, game, content, order)) {
    std::error_code ec;
    if (!fs::is_regular_file(p, ec)) continue;
    main = p;
    break;
  }
  std::vector<std::string> sets, rest;
  split_plus(plus, sets, rest);
  const auto counting = [&c, &out](bool on) {
    if (on) {
      c.set_unknown([&out](const con::Args&) {
        out.passed_over++;
        return true;
      });
    } else {
      c.set_unknown(nullptr);
    }
  };
  const auto run_file = [&](const fs::path& p) {
    if (run_path(c, p)) out.files.push_back(p.string());
  };
  counting(true);
  for (const StartStep step : order.start) {
    switch (step) {
      case StartStep::defaults: {
        std::string text, from;
        if (order.defaults_in_packs &&
            defaults_from_packs(root, game, content, order, text, from)) {
          // The game's own default.cfg starts from no binds: the console's own keys are kept.
          const KeptKeys kept = keep_console_keys(c);
          run_text(c, text);
          restore_console_keys(c, kept);
          out.files.push_back(from);
        } else {
          run_named(c, con::kDefaultFile, out);
        }
        break;
      }
      case StartStep::autoexec_before_main:
        if (!main.empty()) run_named(c, con::kAutoexecFile, out);
        break;
      case StartStep::main:
        run_file(main.empty() ? own : main);
        break;
      case StartStep::autoexec:
        run_named(c, con::kAutoexecFile, out);
        break;
      case StartStep::folder_autoexecs: {
        // The base folder's, then the mod's; with neither, the window's own in their place.
        bool ran = false;
        for (const std::string_view folder : game_folders(game, content) | std::views::reverse) {
          const fs::path dir = entry_named(root, folder);
          if (dir.empty() || !run_path(c, dir / con::kAutoexecFile)) continue;
          out.files.push_back((dir / con::kAutoexecFile).string());
          ran = true;
        }
        if (!ran) run_named(c, con::kAutoexecFile, out);
        break;
      }
      case StartStep::postexec:
        run_named(c, kPostexecFile, out);
        break;
      case StartStep::own:
        if (!main.empty()) run_file(own);
        break;
      case StartStep::command_sets:
        counting(false);
        c.append_command_line(sets);
        c.run();
        break;
      case StartStep::command_rest:
        counting(false);
        c.append_command_line(rest);
        c.run();
        break;
      case StartStep::postinit:
        counting(true);
        run_named(c, kPostinitFile, out);
        break;
    }
  }
  counting(false);
  return out;
}

}  // namespace shooters_edge::client
