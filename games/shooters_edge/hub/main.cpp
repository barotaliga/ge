// SHOOTER'S EDGE'S HUB: the engine's hub (engine/hub, docs/hub.md) with the game's words: its
// protocol, its modes in the order its community ranks them, frags as the score, and the classic
// status query, so `--browse-master` lists the classic servers already running
// (engine/hub/browse.hpp), and each demo's content pack names only the files the demo draws from
// (client/demo_content.hpp). Built as `shooters_edge_hub` at the build's root by `ge_add_hub`;
// `--help` lists its flags.
//
// ge:summary Shooter's Edge's hub binary: the engine's hub with the game's modes and words
// ge:keywords shooters-edge hub hubspec modes frags website server-list classic browse
// ge:skill net
// ge:test tests/hub_api_test.cpp
#include "hub/main.hpp"
#include "net/packet.hpp"
#include "shooters_edge/client/demo_content.hpp"
#include "shooters_edge/net/classic_browse.hpp"

namespace {

ge::hub::HubSpec spec() {
  ge::hub::HubSpec s;
  s.game = "Shooter's Edge";
  s.id = "shooters_edge";
  s.protocol = ge::net::protocol_id("shooters_edge", 1);
  s.modes = {{"4on4", 9}, {"2on2", 8}, {"1on1", 7}, {"ctf", 6},
             {"coop", 5}, {"race", 4}, {"ffa", 3}};
  s.score = "frags";
  s.http_port = 28080;
  s.browse = shooters_edge::net::classic_query();  // the classic servers already out there
  s.demo_files = shooters_edge::client::demo_files;
  s.file_needs = shooters_edge::client::content_needs;
  return s;
}

}  // namespace

int main(int argc, char** argv) { return ge::hub::hub_main(argc, argv, spec()); }
