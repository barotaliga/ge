---
title: A second rules runner counts the host's time by the call and reads the map's own entities
date: 2026-11-05
scope: games/shooters_edge/server/sequel_server.cpp, games/shooters_edge/server/rules_runner.hpp, tests/shooters_edge_sequel_server_test.cpp
concepts: [rules-runner, sequel-runner, frame-100ms, host-time, mintic, map-spec, entities]
---

The host calls a runner's `frame` once a tick and passes no clock. A runner whose rules run at their own rate (the sequel's 100 ms frame) adds `sv.set.mintic` to its own host clock on each call and runs a rules frame each time that clock reaches the next 100 ms (with a 1e-9 slack, since 13 ms ticks do not sum exactly). A test of the rate counts ticks: 100 ticks of 13 ms give 13 frames. Commands are not tied to that frame; they run as their packet arrives.

A runner over a brush map does not need `MapSpec::entities`. `brush::Map::entities` already holds the parsed entity lump, worldspawn first. So `MapSpec::brushes` carries the whole map, and the runner reads its entities and brush models from it. A test that wants a world title adds `message` to `entities[0]`, because `write_brush_world` leaves the worldspawn block bare.
