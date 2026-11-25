// Quake II's monsters' footsteps and frame sounds (q2_monster_steps.cpp): the step a frame
// sounds, the cry or blow a frame of a run plays, and the sounds a kind needs on the sound list.
//
// ge:summary Quake II's monsters' footsteps and frame sounds by kind, run and frame
// ge:keywords q2 monsters footsteps g_monsterfootsteps monster-steps frame-sounds insane-cries
// ge:skill shooter
// ge:test tests/shooters_edge_q2_monsters_test.cpp
#pragma once

#include <span>
#include <string_view>

namespace shooters_edge::server {

struct Q2Server;
struct Q2Monster;

// Monster `m`'s step: a sound when the frame it shows is new and one of its kind's steps in the
// run it plays. The light kinds step only under g_monsterfootsteps (read each frame); the heavy
// ones always. No ground or speed check: a step sounds wherever the frame plays.
// Also its frame's own sound, whatever g_monsterfootsteps says (a prisoner's cries and fists).
void monster_footstep(Q2Server& s, Q2Monster& m);
// The sounds kind `classname`'s steps play (none when it steps only under g_monsterfootsteps and
// `steps_on` is false).
std::span<const char* const> monster_step_sounds(std::string_view classname, bool steps_on);
// Calls `add` with each sound kind `classname`'s frames play.
void monster_cue_sounds(std::string_view classname, void (*add)(void*, const char*), void* to);

}  // namespace shooters_edge::server
