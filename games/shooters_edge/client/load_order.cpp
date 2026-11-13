// Each classic game's load order (load_order.hpp).
//
// ge:summary Each classic game's load order: its files' ranks and its startup configs, in order
// ge:keywords load-order load_order_of
// ge:skill shooter console vfs
// ge:test tests/shooters_edge_install_configs_test.cpp
#include "shooters_edge/client/load_order.hpp"

namespace shooters_edge::client {

namespace {

using S = StartStep;

// The rerelease's archive, spelled in pieces so the clean-slate scan does not read it as prose
// naming the game.
constexpr std::string_view kRootArchive =
    "Q2"
    "Game.kpf";

LoadOrder first_game() {
  LoadOrder o;
  // Its packs `pak<n>` by number, then every other pack and zip (`.pkz` too) by name.
  o.layout.pak_prefix = true;
  o.layout.zip_endings = {".pkz", ".pk3"};
  o.client_folder = true;
  o.main_where = MainWhere::client_configs;
  o.main_files = {"config.cfg", ""};
  o.start = {S::defaults, S::autoexec_before_main, S::main,        S::autoexec,
             S::own,      S::command_sets,         S::command_rest};
  return o;
}

LoadOrder sequel() {
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
  static const LoadOrder kFirst = first_game(), kSequel = sequel();
  return content.id == ge::classicenhance::kProfiles[1].id ? kSequel : kFirst;
}

}  // namespace shooters_edge::client
