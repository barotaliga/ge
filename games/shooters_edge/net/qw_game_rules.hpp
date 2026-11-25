// THE QUAKEWORLD SERVER'S SETTINGS as the server core lists them (net/qw_game.cpp's game): the
// mod's own rules a server owner sets, the move's and the loop's settings, then the endpoint's own
// (every one in the order `--explain` lists them), each QuakeWorld game's own names for them, and
// what `sv_modern` sets.
//
// ge:summary The QuakeWorld server's settings, their other names and sv_modern, for the core
// ge:keywords qw_game_rules qw_setting_names qw_game_metas server-settings sv_modern explain
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_game_test.cpp
#pragma once

#include <span>
#include <vector>

#include "server/server.hpp"

namespace shooters_edge::net {

// Every rule, in the order `--explain` lists them: built once.
const std::vector<ge::server::GameRule>& qw_game_rules();
// Each QuakeWorld game's own names for the server's settings.
std::span<const ge::server::SettingName> qw_setting_names();
// `sv_modern`'s settings.
std::span<const ge::server::GameMeta> qw_game_metas();

}  // namespace shooters_edge::net
