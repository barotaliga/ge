// `secli`: Shooter's Edge's player CLI, the engine's game CLI (engine/gamecli) over the
// game's Spec (cli/spec.hpp). `secli help` lists the commands; `skill` prints the
// SKILL.md an agent installs to drive the game.
//
// ge:summary The secli executable: the engine's game CLI over the game's Spec
// ge:keywords secli main gamecli player-cli
// ge:skill shooter agent-interface
// ge:test tests/gamecli_cli_test.cpp
#include "gamecli/gamecli.hpp"
#include "shooters_edge/cli/spec.hpp"

int main(int argc, char** argv) {
  return ge::gamecli::main(argc, argv, shooters_edge::cli::spec());
}
