// THE `play` EXECUTABLE: a game's play target is its GE_GAME file, its simulation and a `main`
// that calls `play_main`. `ge play`, `ge sim`, `ge replay`, `ge state` and `ge diverge` build it
// and run it; it can be run by hand too:
//
//   play [--stdio] [--seed=S] [--<option>=V] [--inputs=F]   the line protocol on stdin/stdout;
//                                                           --inputs writes the log at the end
//   play describe                                           the game: axes, options, step rate
//   play sim [--seeds=A,B] [--<option>=V] [--out=DIR]       the pilot over each seed; logs in DIR
//   play replay <log>                                       the log played back to its digest
//   play state <log> [--step=N]                             the state after N steps (default all)
//   play diverge <log-a> <log-b>                            the first step two runs part
//
// Every command but the protocol prints exactly one JSON object on one line. A problem prints
// `{"error":...,"where":...,"next":...}`; the exit code is 0 done, 1 a problem (a replay that
// misses its digest, two runs that diverge), 2 a usage error.
//
// ge:summary The play executable's main: the protocol, describe, sim, replay, state, diverge
// ge:keywords play main executable command-line describe sim replay state diverge play_main
// ge:skill agent-interface
// ge:test tests/play_session_test.cpp
#pragma once

#include <iosfwd>

namespace ge::play {

// The `play` command line over the registered game (game.hpp's GE_GAME).
int play_main(int argc, char** argv);

// The same over explicit streams (the tests' way in).
int play_main(int argc, char** argv, std::istream& in, std::ostream& out);

}  // namespace ge::play
