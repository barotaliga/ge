// C++ MODS ON QUAKE II'S RULES (docs/mods.md § On the server, M52g): Quake II's rules keep
// their own players and map entities and mirror them into the loop's pool only to send them, so
// the program seam's hooks never reach them. This lays the same tables (sim/mod_api.hpp) over them
// instead, so one package stacks on either game's rules:
//
// - THE HOOKS at Quake II's own points: match_start once the map's entities are spawned,
//   start_frame at the top of each 100 ms rules frame, player_pre_think and player_post_think
//   around each command a player's packet brings, put_client_in_server at every spawn,
//   client_connect and client_disconnect, `damage` as a blow is about to land on a player (each
//   mod answers the points dealt, the lowest first) and `death` once a death is scored.
// - THE SERVICES over Quake II's state, numbered as the pool's rows are: 0 the world, 1..32 the
//   players (slot + 1), then the map's entities in the map's order. A player's origin, velocity,
//   health and view; an entity's origin, angles, velocity, box and health. A player's origin or
//   velocity may be set (packed to the eighth of a unit, as the move keeps it), and his health
//   (at 0 or under he dies by the world). Everything else Quake II keeps itself (a mod's own
//   entity, its functions, the movers' places, fields it has no slot for) is refused: a warning
//   in the mod's log, a getter's 0, and the match goes on.
//
// The mods' runs fold into the runner's digest (`rules_hash`), and a modded match replays to the
// bit: the hooks run at fixed points of the rules frame, in the order the mods were named.
//
// ge:summary C++ mods on Quake II's rules: the hooks at its points, the services over its state
// ge:keywords mod mods q2 q2-mods own_mods damage-hook death-hook services entity-numbers
// ge:keywords q2_start_mods m52g
// ge:skill mod shooter
// ge:test tests/shooters_edge_rules_mod_test.cpp
#pragma once

#include <string>

namespace shooters_edge::server {

struct RulesCommon;
struct Q2Server;

// The runner's `own_mods`: a host of each of `mods.tables` over Quake II's state, each
// match_start run. False with `error` naming the mod whose hooks were refused.
bool q2_start_mods(RulesCommon& common, std::string& error);

// The hooks, each a no-op with no mods. `slot` is a player's slot (0..31).
void q2_mods_start_frame(Q2Server& s);
void q2_mods_pre_think(Q2Server& s, int slot);
void q2_mods_post_think(Q2Server& s, int slot);
void q2_mods_spawn(Q2Server& s, int slot);
void q2_mods_connect(Q2Server& s, int slot);
void q2_mods_disconnect(Q2Server& s, int slot);
// The points to deal to `target` from `attacker` (a slot, -1 the world), each mod in turn.
int q2_mods_damage(Q2Server& s, int target, int attacker, int points);
// `slot` died by `killer` (a slot, -1 the world).
void q2_mods_death(Q2Server& s, int slot, int killer);

}  // namespace shooters_edge::server
