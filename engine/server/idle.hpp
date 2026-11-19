// A PLAYER LEFT IDLE IS DROPPED (`sv_idletime`), the same rule for every game's server
// (docs/servers.md § The settings): a seat whose player has done nothing for that many seconds is
// freed, so a full server does not keep slots for players who walked away.
//
// - ACTIVITY: a change in the buttons held or in the move asked (forward, side, up), or a chat
//   line (`say`, `say_team`). Standing still with nothing pressed, or holding the same keys, is
//   idle; a spectator idles as a player does.
// - THE CLOCK is the caller's (the server's own time, s): nothing reads a wall clock, so a session
//   over the simulated network replays to the bit. A watch is a double and a word a seat, set when
//   the seat is taken (`idle_start`).
// - THE WORDS: the player dropped is told "You were dropped for idling."; the others "<name> was
//   dropped for idling.".
//
// The rule is `kIdleRules` (an endpoint's, read when the server starts); 0 (the default) never
// drops. (The engine's `sv_idlekick` is the lobby's other rule: the unready's time.)
//
// ge:summary sv_idletime: a seat with no buttons, move or chat for that long is dropped
// ge:keywords sv_idletime idle idling idle-drop afk inactivity idle_start idle_move idle_chat
// ge:keywords idle_over
// ge:skill net
// ge:test tests/server_idle_test.cpp
#pragma once

#include <cstdint>

#include "server/server.hpp"

namespace ge::server {

inline constexpr GameRule kIdleRules[] = {
    {"sv_idletime", "The seconds a player may do nothing before being dropped.", nullptr,
     "0: never (the default); N: that many seconds with no button, move or chat"},
};

// One seat's watch: when its player last did something, and what he last held and asked.
struct IdleWatch {
  double active_at = 0;
  uint32_t held = 0;  // the buttons and the move, packed
};

inline void idle_start(IdleWatch& w, double now) { w = {now, 0}; }

// A command: active when the buttons or the move differ from the last command's.
inline void idle_move(IdleWatch& w, double now, uint32_t buttons, int forward, int side, int up) {
  const auto sign = [](int v) { return v > 0 ? 1u : v < 0 ? 2u : 0u; };
  const uint32_t held = (buttons & 0xffffu) << 6 | sign(forward) << 4 | sign(side) << 2 | sign(up);
  if (held != w.held) w.active_at = now;
  w.held = held;
}

inline void idle_chat(IdleWatch& w, double now) { w.active_at = now; }

// Whether the seat has idled past `limit_s` (0 or less: never).
inline bool idle_over(const IdleWatch& w, double now, double limit_s) {
  return limit_s > 0 && now - w.active_at > limit_s;
}

}  // namespace ge::server
