// THE SAMPLE GAME: the smallest game a server can run, so the server core is tested and measured
// on its own (tests/server_core_test.cpp, the `server-core` lab) without any real game linked.
// Each bot walks a ring of 1000 cells a step left, right or not at all each frame, off the run's
// seed; the hash folds every position and the frame count. 60 frames a second.
//
// ge:summary The server core's sample game: bots walking a ring, a hash, for the tests and lab
// ge:keywords server sample-game servergame test-fixture walkers ring sample_game
// ge:skill net
// ge:test tests/server_core_test.cpp
#pragma once

#include "server/server.hpp"

namespace ge::server {

const ServerGame& sample_game();

}  // namespace ge::server
