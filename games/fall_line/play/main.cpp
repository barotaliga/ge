// Fall Line's `play` executable: the game registered on the play contract, and the engine's
// play command line (engine/play/main.hpp) as its main. `ge play`, `ge sim`, `ge replay`,
// `ge state` and `ge diverge` run it; `build/debug/play describe` lists its axes and options.
//
// ge:summary Fall Line's play executable: GE_GAME(kGame) and the engine's play_main
// ge:keywords play executable main register ge_game play_main
// ge:skill sim agent-interface
// ge:test tests/fall_line_play_test.cpp
#include "play/main.hpp"
#include "fall_line/sim/play.hpp"

GE_GAME(fall_line::sim::kGame);

int main(int argc, char** argv) { return ge::play::play_main(argc, argv); }
