// SHOOTER'S EDGE'S HUB: the engine's hub (engine/hub, docs/hub.md) with the game's words: its
// protocol, its modes in the order its community ranks them, frags as the score, and the classic
// status query, so `--browse-master` lists the classic servers already running
// (engine/hub/browse.hpp), each live match's card its map's still under its board in the game's
// own charset (hub/cards.hpp, hub/still.hpp; --content), and each demo's content pack names only
// the files the demo draws from (client/demo_content.hpp), and a map played in the page its own
// (client/play_content.hpp: the Play page). Built as `sehub` at the build's root by `ge_add_hub`;
// `--help` lists its flags.
//
// ge:summary Shooter's Edge's hub binary: the engine's hub with the game's modes and words
// ge:keywords shooters-edge hub hubspec modes frags website server-list classic browse play
// ge:skill net
// ge:test tests/hub_api_test.cpp
#include <cstdio>

#include "hub/card.hpp"
#include "hub/main.hpp"
#include "net/packet.hpp"
#include "shooters_edge/client/demo_content.hpp"
#include "shooters_edge/client/play_content.hpp"
#include "shooters_edge/hub/cards.hpp"
#include "shooters_edge/net/qw_browse.hpp"
#include "shooters_edge/net/qw_game.hpp"
#include "shooters_edge/net/qw_report.hpp"
#if defined(SE_HUB_STILLS)
#include "shooters_edge/hub/still.hpp"
#endif

namespace {

ge::hub::HubSpec spec() {
  ge::hub::HubSpec s;
  s.game = "Shooter's Edge";
  s.id = "shooters_edge";
  s.protocol = ge::net::protocol_id("shooters_edge", 1);
  s.modes = {{"4on4", 9}, {"2on2", 8}, {"1on1", 7}, {"ctf", 6},
             {"coop", 5}, {"race", 4}, {"ffa", 3}};
  s.score = "frags";
  // The aim spread (shooter/aim_spread.hpp): thousandths in the report, read as a half-box.
  s.columns = {{shooters_edge::net::kAimSpreadColumn, "aim spread", 1000, 2,
                "Aim spread: how far a player's shots stray over the players they hit, edge to "
                "edge 1. A hand reads 0.6 to 0.8; near 0 every shot sits on one point."}};
  s.look = "arena";
  s.http_port = 28080;
  s.browse = shooters_edge::net::qw_query();  // the classic servers already out there
  s.demo_files = shooters_edge::client::demo_files;
  s.file_needs = shooters_edge::client::content_needs;
  // Playing a map in the page: what a match on it with bots draws from (client/play_content.hpp).
  s.map_files = [](std::string_view map, const ge::vfs::SearchPath& files,
                   std::vector<std::string>& names, std::string& error) {
    shooters_edge::client::SearchPath sp;
    sp.vfs = files;
    return shooters_edge::client::play_files(sp, map, shooters_edge::net::listen_options(), names,
                                             error);
  };
  s.map_shown = shooters_edge::client::map_playable;
  // The match cards (hub/cards.hpp): each match drawn over its map's intermission view, its board
  // in its game's own charset; the stills drawn where the GPU is built (hub/still.hpp).
  s.stills.game_of = shooters_edge::hub::hub_game_of;
  s.stills.look = [](std::string_view content, std::string_view game, ge::hub::CardLook& out) {
    std::string error;
    if (shooters_edge::hub::hub_card_look(content, game, out, error)) return true;
    std::fprintf(stderr, "sehub: no board for %.*s: %s; next: check --content=%.*s/%.*s\n",
                 int(game.size()), game.data(), error.c_str(), int(content.size()), content.data(),
                 int(game.size()), game.data());
    return false;
  };
#if defined(SE_HUB_STILLS)
  s.stills.key = shooters_edge::hub::still_key;
  s.stills.render = [](std::string_view content, std::string_view game, std::string_view map,
                       int width, int height, ge::hub::CardPicture& out, std::string& error) {
    out.width = width;
    out.height = height;
    return shooters_edge::hub::draw_still(content, game, map, width, height, out.rgba, error);
  };
#endif
  return s;
}

}  // namespace

int main(int argc, char** argv) { return ge::hub::hub_main(argc, argv, spec()); }
