// THE CLASSIC ENDPOINT'S OWN SETTINGS: the rules a server.cfg sets that the mod never sees (the
// rate's cap and floor, downloads, antilag, voice, the guards, the slots, the pings, the remote
// console, VIPs), their words, and their values read into the endpoint's ServerConfig.
//
// ge:summary The classic endpoint's own settings: their table and their reading into its config
// ge:keywords endpoint-settings endpoint_rules endpoint_settings is_endpoint_rule qtv_settings
// ge:keywords sv_maxrate sv_minrate sv_antilag sv_voip sv_fullspectate sv_maxping sv_minping
// ge:keywords sv_antilag_projectiles sv_antilag_max
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_game_test.cpp
#pragma once

#include <span>
#include <string>

#include "server/server.hpp"
#include "shooters_edge/net/classic_qtv.hpp"
#include "shooters_edge/net/classic_server.hpp"

namespace shooters_edge::net {

// The endpoint's rules, in the order `--explain` lists them.
std::span<const ge::server::GameRule> endpoint_rules();
bool is_endpoint_rule(const std::string& name);
// The endpoint's own settings from the rules: false with `error` on a value out of range.
bool endpoint_settings(const ge::server::ServerConfig& c, ServerConfig& cfg, std::string& error);
// The stream port's settings from the rules: false with `error` on a value out of range.
bool qtv_settings(const ge::server::ServerConfig& c, int& port, QtvConfig& q, std::string& error);

}  // namespace shooters_edge::net
