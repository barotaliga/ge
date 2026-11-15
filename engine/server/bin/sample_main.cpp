// `gesample`: the sample game's dedicated server (server/sample.hpp: bots walking a ring), the
// engine's dedicated process with no real game linked, so starting, joining, finding and stopping
// a server is tried on the engine alone (the `server-launch` lab, server/spawn.hpp).
//
// voice: agent (a command-line tool's main: its errors are for whoever runs it;
// docs/architecture.md § Player text).
//
// ge:summary The sample game's dedicated server binary gesample, for the server-launch lab
// ge:keywords gesample sample-server dedicated-server sample-game server-launch
// ge:skill net
// ge:test tests/server_spawn_test.cpp
#include "server/main.hpp"
#include "server/sample.hpp"

int main(int argc, char** argv) {
  return ge::server::server_main(argc, argv, ge::server::sample_game());
}
