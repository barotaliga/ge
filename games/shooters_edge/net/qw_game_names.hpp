// THE QUAKEWORLD SERVER'S SETTINGS BY THE GAMES' OWN NAMES: the names each QuakeWorld game gives
// the server's settings (a server.cfg, `+set`, rcon or vote line by either name sets the one
// setting), `sv_modern`'s meaning, the `*mods` server info, the settings a mod reads by its game's
// name, and the loop's client slots. Split out of qw_game.cpp by subject.
//
// ge:summary The QuakeWorld server's settings by the games' names: synonyms, sv_modern, slots
// ge:keywords qw-setting-names sv_modern setting-synonyms mods-info game-names client-slots
// ge:keywords qw_setting_names qw_game_metas qw_mods_info qw_game_names qw_slots
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_game_test.cpp
#pragma once

#include <memory>
#include <span>
#include <string>
#include <vector>

#include "mod/loader.hpp"
#include "server/server.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::net {

// Each game's names for the server's settings, and the meta-settings (`sv_modern`).
std::span<const ge::server::SettingName> qw_setting_names();
std::span<const ge::server::GameMeta> qw_game_metas();
// `*mods`: each package's name and version, cut short past what one server info value takes.
std::string qw_mods_info(const std::vector<std::unique_ptr<ge::mod::LoadedMod>>& packages);
// The server's settings a mod reads by its game's name, put in the rules' own table.
void qw_game_names(shooters_edge::server::RulesCommon& rules, const ge::server::ServerConfig& c);
// The loop's client slots: the players' and, when spectators have their own, theirs.
int qw_slots(const ge::server::ServerConfig& c);

}  // namespace shooters_edge::net
