// A classic install's configs run in its own order (install_configs.hpp).
//
// ge:summary A classic install's configs run in its own order: default, main config, autoexec
// ge:keywords install-configs start_configs config_dirs
// ge:skill shooter console
// ge:test tests/shooters_edge_install_configs_test.cpp
#include "shooters_edge/client/install_configs.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <ranges>
#include <string_view>
#include <system_error>

#include "console/cfg.hpp"
#include "console/lex.hpp"
#include "shooters_edge/client/load_order.hpp"
#include "shooters_edge/client/settings.hpp"
#include "vfs/search.hpp"

namespace shooters_edge::client {

namespace {

namespace fs = std::filesystem;
namespace con = ge::console;

// The files Quake II's client runs after autoexec.cfg and after the command line.
constexpr std::string_view kPostexecFile = "postexec.cfg";
constexpr std::string_view kFsDefault = "fs_default";
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
// Quake II's client reads `config.cfg` on Linux and macOS and `q2config.cfg` on Windows; the
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
    if (const fs::path client = entry_named(root, order.client_folder); !client.empty())
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

// `text` run to its end, `wait`s included, as a config's lines (`from` its file).
void run_text(con::Console& c, const std::string& text, const std::string& from = "config") {
  c.append(text, con::Source::local, con::Origin{from, 1});
  for (int n = 0; n < con::kMaxFrames && c.pending(); ++n) c.run();
}

// The file at `path` run to its end; false when it is not there.
bool run_path(con::Console& c, const fs::path& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  run_text(c, std::string{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()},
           path.string());
  return true;
}

// `name` found on the console's search path and run; false when it is not there.
bool run_named(con::Console& c, std::string_view name, ConfigsRun& out) {
  std::string text;
  if (!c.read_file(name, text)) return false;
  c.past_sleep([&c, name] {  // ahead of a recipe that sleeps (`+exec` before postinit, Quake II)
    c.exec_file(name);
    for (int n = 0; n < con::kMaxFrames && c.pending(); ++n) c.run();
  });
  out.files.emplace_back(name);
  return true;
}

// Each game folder's autoexec.cfg, the base folder's then the mod's; with neither, the one the
// search path finds in their place.
void run_folder_autoexecs(con::Console& c, const fs::path& root, const std::string& game,
                          const ge::classicenhance::Profile& content, ConfigsRun& out) {
  bool ran = false;
  for (const std::string_view folder : game_folders(game, content) | std::views::reverse) {
    const fs::path dir = entry_named(root, folder);
    if (dir.empty() || !run_path(c, dir / con::kAutoexecFile)) continue;
    out.files.push_back((dir / con::kAutoexecFile).string());
    ran = true;
  }
  if (!ran) run_named(c, con::kAutoexecFile, out);
}

// The console's own keys (the one under the tilde, the ISO key) in each mode, kept across a config
// that starts with `unbindall`.
struct KeptKeys {
  std::vector<std::pair<std::pair<con::BindMode, std::string>, std::string>> binds;
};
constexpr const char* kConsoleKeys[] = {"`", "nonus"};
// The window's move keys: kept over a pack's own default.cfg (its layout moves on the arrows and
// binds A to look up and the right button to sidestep), so W A S D, SPACE and the buttons still
// move, jump and fire. A player's own config, run after, binds them as it likes.
constexpr const char* kMoveKeys[] = {"w", "a", "s", "d", "space", "mouse1", "mouse2"};

KeptKeys keep_move_keys(const con::Console& c) {
  KeptKeys k;
  for (const char* key : kMoveKeys)
    if (const std::string& b = c.binds().get(con::BindMode::game, key); !b.empty())
      k.binds.push_back({{con::BindMode::game, key}, b});
  return k;
}

void restore_move_keys(con::Console& c, const KeptKeys& k) {
  for (const auto& [at, command] : k.binds) c.binds().set(at.first, at.second, command);
}
// The window's own defaults kept over a pack's default.cfg: Quake II's turns `lookspring` on, which
// levels the view of a player who lets +mlook go; ours is off in every game. A player's own config,
// run after, sets it as he likes.
constexpr const char* kKeptSettings[] = {"lookspring"};
using KeptSettings = std::vector<std::pair<std::string, std::string>>;

KeptSettings keep_window_settings(con::Console& c) {
  KeptSettings k;
  for (const char* name : kKeptSettings)
    if (const con::Cvar* v = c.cvars().find(name); v != nullptr) k.emplace_back(name, v->value);
  return k;
}

void restore_window_settings(con::Console& c, const KeptSettings& k) {
  for (const auto& [name, value] : k) c.cvars().set(name, value, con::Source::code);
}
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

// The text of `name` in the packs of the game folder `folder` (the winner first); false when none
// holds it.
bool folder_pack_text(const fs::path& root, std::string_view folder, const LoadOrder& order,
                      std::string_view name, std::string& text, std::string& from) {
  const fs::path dir = entry_named(root, folder);
  if (dir.empty()) return false;
  const std::vector<std::string> archives =
      ge::vfs::game_archives(root.string(), dir.filename().string(), order.layout);
  for (const std::string& pack : archives | std::views::reverse) {
    std::vector<uint8_t> bytes;
    if (!ge::vfs::read_from_pack(pack, name, &bytes, nullptr)) continue;
    text.assign(bytes.begin(), bytes.end());
    from = pack + "/" + std::string(name);
    return true;
  }
  return false;
}

// A command as words in lower case, so two files' copies of one line match across spacing.
std::string command_key(std::string_view command) {
  std::string key;
  for (const std::string& w : con::tokenize(command)) key += (key.empty() ? "" : " ") + lower(w);
  return key;
}

// What a mod's own default.cfg, inside its packs, adds to the base game's (Quake's order, where
// the base's copy in its packs is the game's own and does not run): the mod's lines the base's
// file lacks (a mission pack's keys for its new weapons, its aliases), run over the window's own
// layout, which stands. Nothing when the folder played is the base or the mod's packs hold none.
void run_mod_default_additions(con::Console& c, const fs::path& root, const std::string& game,
                               const ge::classicenhance::Profile& content, const LoadOrder& order,
                               ConfigsRun& out) {
  const std::vector<std::string_view> folders = game_folders(game, content);
  if (folders.size() < 2) return;
  std::string mod_text, mod_from, base_text, base_from;
  if (!folder_pack_text(root, folders.front(), order, con::kDefaultFile, mod_text, mod_from))
    return;
  folder_pack_text(root, folders.back(), order, con::kDefaultFile, base_text, base_from);
  std::vector<std::string> base_keys;
  for (const con::Line& line : con::split_commands(base_text))
    base_keys.push_back(command_key(line.text));
  std::string added;
  for (const con::Line& line : con::split_commands(mod_text))
    if (std::ranges::find(base_keys, command_key(line.text)) == base_keys.end())
      added += line.text + "\n";
  if (added.empty()) return;
  run_text(c, added, mod_from);
  out.files.push_back(mod_from);
}

// The game's default.cfg: from its packs where its order reads it there (keeping the console's
// keys, the move keys and the window's settings over its `unbindall`), else on the search path.
void run_defaults(con::Console& c, const fs::path& root, const std::string& game,
                  const ge::classicenhance::Profile& content, const LoadOrder& order,
                  ConfigsRun& out) {
  std::string text, from;
  if (order.defaults_in_packs && defaults_from_packs(root, game, content, order, text, from)) {
    const KeptKeys kept = keep_console_keys(c);
    const KeptKeys moves = keep_move_keys(c);
    const KeptSettings settings = keep_window_settings(c);
    run_text(c, text, from);
    restore_console_keys(c, kept);
    restore_move_keys(c, moves);
    restore_window_settings(c, settings);
    out.files.push_back(from);
  } else {
    run_named(c, con::kDefaultFile, out);
    if (!order.defaults_in_packs) run_mod_default_additions(c, root, game, content, order, out);
  }
}

// Whether default.cfg runs (`fs_default`): the command line's `+set fs_default` first, as it runs
// only after the configs, else the setting as it stands (on when it is not registered).
bool defaults_wanted(const con::Console& c, const std::vector<std::string>& sets) {
  for (size_t i = 0; i + 2 < sets.size(); ++i)
    if (lower(sets[i]) == "+set" && lower(sets[i + 1]) == kFsDefault)
      return std::atof(sets[i + 2].c_str()) != 0;
  const con::Cvar* on = c.cvars().find(kFsDefault);
  return on == nullptr || on->on();
}

}  // namespace

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
  if (!load_order_of(content).client_folder.empty()) add(load_order_of(content).client_folder);
  add(content.base_dir);
  dirs.emplace_back(".");
  return dirs;
}

bool run_game_settings(con::Console& c, const std::string& basedir, const std::string& game,
                       const ge::classicenhance::Profile& content) {
  const fs::path root = basedir.empty() ? fs::path(".") : fs::path(basedir);
  ConfigsRun out;
  if (defaults_wanted(c, {})) run_defaults(c, root, game, content, load_order_of(content), out);
  const bool ran = run_path(c, settings_dir(basedir, game) / con::kConfigFile);
  const con::Cvar* again = c.cvars().find("fs_autoexec");
  if (again == nullptr || !again->on()) return ran;
  run_folder_autoexecs(c, root, game, content, out);
  run_named(c, kPostexecFile, out);
  return ran;
}

ConfigsRun start_configs(con::Console& c, const std::string& basedir, const std::string& game,
                         const ge::classicenhance::Profile& content,
                         const std::vector<std::string>& plus, const SeriesConfig& series) {
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
  // While the files run, a line no command takes is counted and passed over; the host's own
  // handler for such lines comes back after.
  const std::function<bool(const con::Args&)> host = c.unknown();
  const auto counting = [&c, &out, &host](bool on) {
    if (on) {
      c.set_unknown([&out](const con::Args&) {
        out.passed_over++;
        return true;
      });
    } else {
      c.set_unknown(host);
    }
  };
  const auto run_file = [&](const fs::path& p) {
    if (run_path(c, p)) out.files.push_back(p.string());
  };
  // The window's saved settings: the series' one config, then the game's own file.
  const auto run_own = [&] {
    if (!series.file.empty()) {
      if (series.outside)
        series.outside([&] { run_file(series.file); });
      else
        run_file(series.file);
    }
    run_file(own);
  };
  counting(true);
  for (const StartStep step : order.start) {
    switch (step) {
      case StartStep::defaults:
        if (defaults_wanted(c, sets)) run_defaults(c, root, game, content, order, out);
        break;
      case StartStep::autoexec_before_main:
        if (!main.empty()) run_named(c, con::kAutoexecFile, out);
        break;
      case StartStep::main:
        if (main.empty())
          run_own();
        else
          run_file(main);
        break;
      case StartStep::autoexec:
        run_named(c, con::kAutoexecFile, out);
        break;
      case StartStep::folder_autoexecs:
        run_folder_autoexecs(c, root, game, content, out);
        break;
      case StartStep::postexec:
        run_named(c, kPostexecFile, out);
        break;
      case StartStep::own:
        if (!main.empty()) run_own();
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

std::string mod_startdemos(const std::string& basedir, const std::string& game,
                           const ge::classicenhance::Profile& content, std::string_view fallback) {
  const std::vector<std::string_view> folders = game_folders(game, content);
  if (folders.size() < 2)
    return std::string(fallback);  // the base folder: the game's own list stands
  const fs::path root = basedir.empty() ? fs::path(".") : fs::path(basedir);
  const fs::path dir = entry_named(root, folders.front());
  if (dir.empty()) return std::string(fallback);
  constexpr std::string_view kStartFile = "quake.rc";
  std::string text;
  const std::vector<std::string> archives =
      ge::vfs::game_archives(root.string(), dir.filename().string(), load_order_of(content).layout);
  for (const std::string& pack : archives | std::views::reverse) {  // the winner first
    std::vector<uint8_t> bytes;
    if (!ge::vfs::read_from_pack(pack, kStartFile, &bytes, nullptr)) continue;
    text.assign(bytes.begin(), bytes.end());
    break;
  }
  if (text.empty()) {
    const fs::path loose = entry_named(dir, kStartFile);
    if (loose.empty()) return std::string(fallback);
    std::ifstream f(loose, std::ios::binary);
    text.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  }
  std::string list;
  for (const con::Line& line : con::split_commands(text)) {
    const std::vector<std::string> words = con::tokenize(line.text);
    if (words.size() < 2 || lower(words.front()) != "startdemos") continue;
    list.clear();  // the last one wins, as it would run last
    for (size_t i = 1; i < words.size(); ++i) list += (i > 1 ? " " : "") + words[i];
  }
  return list.empty() ? std::string(fallback) : list;
}

}  // namespace shooters_edge::client
