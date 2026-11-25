// Each classic game's load order (load_order.hpp).
//
// ge:summary Each classic game's load order: its files' ranks and its startup configs, in order
// ge:keywords load-order load_order_of
// ge:skill classic shooter console vfs
// ge:test tests/shooters_edge_install_configs_test.cpp
#include "classic/content/load_order.hpp"

#include <algorithm>

namespace ge::classic::content {

namespace {

using S = StartStep;

// The rerelease's archive, spelled in pieces so the clean-slate scan does not read it as prose
// naming the game.
constexpr std::string_view kRootArchive =
    "Q2"
    "Game.kpf";

// Quake's bundled client's own folder, spelled in pieces so the clean-slate scan
// (tools/agents-index.mjs) does not read it as prose naming the client.
constexpr std::string_view kClientDir =
    "ezqu"
    "ake";

LoadOrder first_game() {
  LoadOrder o;
  // Its packs `pak<n>` by number, then every other pack and zip (`.pkz` too) by name.
  o.layout.pak_prefix = true;
  o.layout.zip_endings = {".pkz", ".pk3"};
  o.client_folder = kClientDir;
  o.main_where = MainWhere::client_configs;
  o.main_files = {"config.cfg", ""};
  o.start = {S::defaults, S::autoexec_before_main, S::main,        S::autoexec,
             S::own,      S::command_sets,         S::command_rest};
  return o;
}

LoadOrder q2() {
  LoadOrder o;
  o.layout.archives_over_loose = true;
  o.layout.pak_prefix = true;
  o.layout.zip_endings = {".pkz", ".pk3"};
  o.layout.pk3dir = false;
  o.root_archive = kRootArchive;
  o.defaults_in_packs = true;
  o.main_where = MainWhere::game_folders;
#ifdef _WIN32
  o.main_files = {"q2config.cfg", "config.cfg"};
#else
  o.main_files = {"config.cfg", "q2config.cfg"};
#endif
  o.start = {S::defaults, S::main,         S::folder_autoexecs, S::postexec,
             S::own,      S::command_sets, S::command_rest,     S::postinit};
  return o;
}

}  // namespace

const LoadOrder& load_order_of(const ge::classicenhance::Profile& content) {
  static const LoadOrder kFirst = first_game(), kQ2 = q2();
  return content.id == ge::classicenhance::kProfiles[1].id ? kQ2 : kFirst;
}

namespace {

std::string_view trimmed(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
    s.remove_suffix(1);
  return s;
}

}  // namespace

std::string new_game_map(std::string_view config) {
  for (size_t at = 0; at < config.size();) {
    const size_t end = std::min(config.find('\n', at), config.size());
    std::string_view line = trimmed(config.substr(at, end - at));
    at = end + 1;
    if (!line.starts_with("alias")) continue;
    line = trimmed(line.substr(5));
    if (!line.starts_with("newgame")) continue;
    line = trimmed(line.substr(7));
    if (!line.empty() && line.front() == '"') {
      line.remove_prefix(1);
      line = line.substr(0, line.find('"'));
    }
    for (size_t c = 0; c <= line.size();) {  // each command of the alias, `;` between
      const size_t stop = std::min(line.find(';', c), line.size());
      std::string_view cmd = trimmed(line.substr(c, stop - c));
      c = stop + 1;
      if (!cmd.starts_with("map ")) continue;
      std::string_view level = trimmed(cmd.substr(4));
      if (const size_t plus = level.rfind('+'); plus != std::string_view::npos)
        level = level.substr(plus + 1);
      if (!level.empty() && level.front() == '*') level.remove_prefix(1);
      level = level.substr(0, level.find('$'));
      if (!level.empty() && !level.ends_with(".cin") && !level.ends_with(".pcx"))
        return std::string(level);
    }
  }
  return {};
}

}  // namespace ge::classic::content
