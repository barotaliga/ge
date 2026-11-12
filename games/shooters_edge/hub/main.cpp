// SHOOTER'S EDGE'S HUB: the engine's hub (engine/hub, docs/hub.md) with the game's words: its
// protocol, its modes in the order its community ranks them, frags as the score. Built as
// `shooters_edge_hub` at the build's root by `ge_add_hub`; `--help` lists its flags.
//
// ge:summary Shooter's Edge's hub binary: the engine's hub with the game's modes and words
// ge:keywords shooters-edge hub hubspec modes frags website server-list
// ge:skill net
// ge:test tests/hub_api_test.cpp
#include "hub/main.hpp"
#include "net/packet.hpp"

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
  return s;
}

}  // namespace

int main(int argc, char** argv) { return ge::hub::hub_main(argc, argv, spec()); }
