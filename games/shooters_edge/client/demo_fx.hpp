// WHAT A DEMO THROWS INTO THE WORLD: a played recording's (or a joined server's, read as a live
// demo) temporary entities and missiles, kept the way the effects (world_fx.hpp) read a listen
// server's, so a demo and a joined game draw the same explosions, trails, hits, blood, splashes
// and beams as a game played here.
//
// - EVERY ONE THIS LEVEL, IN ORDER: each frame's temporary entities (DemoState::effects) are
//   appended; an explosion's point goes to `explosions`, the rest to `effects` as the listen
//   server's `Effect`s (a gunshot's kind is the hit). A new level (DemoState::levels moved)
//   empties both, which the effects read as a new map.
// - THE MISSILES IN FLIGHT: an entity drawn this frame whose model is the rocket's, the grenade's
//   or a spike's (by its precache name; the recording carries no classname), the others left out
//   of the trails.
//
// ge:summary A demo's temporary entities and missiles kept for the effects, as a listen server's
// ge:keywords demo-fx demo-effects temp-entity explosion trail missile rocket grenade spike
// ge:keywords demo_fx_frame effect_of joined-server-effects playdemo timedemo
// ge:skill shooter render
// ge:test tests/shooters_edge_demo_test.cpp
#pragma once

#include <string_view>
#include <vector>

#include "shooters_edge/client/campaign_view.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/listen.hpp"

namespace shooters_edge::client {

struct DemoFx {
  int levels = -1;                    // the level these are of (DemoState::levels)
  std::vector<sh::Vec3f> explosions;  // every one this level, in order
  std::vector<Effect> effects;        // every other temporary entity this level, in order
  std::vector<Missile> missiles;      // in flight this frame
};

// The listen server's form of a demo's temporary entity (not an explosion: those are points).
Effect effect_of(const DemoEffect& e);
// What a model by its precache name flies as: a rocket, a grenade, a spike; false: none of them.
bool missile_model(std::string_view name, Missile& kind);
// `s`'s frame (or frames: a joined server's step) added to `fx`.
void demo_fx_frame(const DemoState& s, DemoFx& fx);

}  // namespace shooters_edge::client
