// Quake II's server console lines about the map (q2_server.hpp states the rules): what a map's
// entities lack or carry wrong, said as the map loads, and the counts said after it.
// Only the console sees them; no player does.
//
// ge:summary Quake II's server console lines: a map's faults at load, the inhibited and teams count
// ge:keywords q2 console server-console spawn-warnings inhibited teams picktarget position
// ge:keywords invalid-spawnflags path-corner lightramp trigger-key fixed-triggered init-game
// ge:skill shooter
// ge:test tests/shooters_edge_q2_lines_test.cpp
#pragma once

#include <string>
#include <vector>

#include "shooter/vec.hpp"

namespace shooters_edge::server {

struct Q2Server;

// A position as the console prints it: `(x y z)`, each rounded to the nearest whole number with
// halves to the even one, and a small negative that rounds to nothing printed `-0`.
std::string q2_pos(ge::shooter::Vec3f p);

// The lines a map's entities earn as they spawn (before the movers and monsters are set up), in the
// map's order; an entity in `inhibited` (left out by skill or mode, by the map's order) earns none.
// Some put the fault right as they say it (a path corner with no name is left out, a func_wall
// switched on gets its toggle).
void q2_spawn_lines(Q2Server& s, const std::vector<char>& inhibited);

// The two lines after every entity has spawned: `<n> entities inhibited`, then `<n> teams with <m>
// entities` (the distinct `team` names among what is left, and how many carry one).
void q2_count_lines(Q2Server& s, int inhibited);

// `G_PickTarget: target <t> not found`: the line before a caller's own, when a name finds nothing.
// With `then`, the caller's own line follows it; an empty `target` says nothing (a caller with no
// name to look up has said so already, or has nothing to say).
void q2_pick_missing(Q2Server& s, const std::string& target);
void q2_pick_missing(Q2Server& s, const std::string& target, const std::string& then);

}  // namespace shooters_edge::server
