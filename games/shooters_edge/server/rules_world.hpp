// THE RULES' WORLD, THE QC SERVER'S HALF: the world's types are the shooter plugin's
// (shooter/rules_world.hpp, named here by using-declarations); the QC server (qc_server.hpp), the
// first runner, fills them: `fill_world` copies its lists at the map's start and its entities and
// clients at the end of each frame. What an entity shows (its model, alpha, scale and tint) is
// read off the program's fields here (docs/shooters-edge.md § The rules runner).
//
// ge:summary The QC server filling the rules' world (the plugin's types): lists, rows, alpha
// ge:keywords rules-world rules-runner runner world-view entity-rows client-rows player-numbers
// ge:keywords fill_world model-list sound-list vwep statics lightstyles serverinfo frags stats
// ge:keywords model-shown taken-item hidden-item model-text world-shot shot-in-flight missiles
// ge:skill progs shooter
// ge:test tests/shooters_edge_rules_world_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "shooter/client.hpp"
#include "shooter/rules_world.hpp"

namespace shooters_edge::server {

namespace sh = ge::shooter;

// The world's types are the shooter plugin's (shooter/rules_world.hpp); this file is the QC
// server's half that fills them.
using sh::kWorldLightStyles;
using sh::PlayerNumbers;
using sh::RulesWorld;
using sh::WorldClient;
using sh::WorldEntity;
using sh::WorldShot;
using sh::WorldStatic;

struct QcServer;
struct StaticEntity;

// A static entity as the world shows it: its see-through share and tint from the extras.
WorldStatic world_static(const StaticEntity& st);

// True when entity `e` is shown: drawn by a window and sent to clients. It needs a model index
// AND a model text: a program hides a taken item by emptying its `model` (keeping the index, so
// setting the text again brings it back), and such an entity is drawn and sent by no one.
bool model_shown(const QcServer& s, int e);

// How much of entity `e` shows, 0..1 (1 solid): the mod's `alpha` field when it sets one above 0
// (DP_ENT_ALPHA), else the map's `alpha` key, else 1.
float entity_alpha(const QcServer& s, int e);
// Entity `e`'s size: the mod's `scale` field, 1 when it is 0 or the program has none.
float entity_scale(const QcServer& s, int e);
// Entity `e`'s tint: the mod's `colormod` field when not '0 0 0', else the map's key, else 1 1 1.
void entity_colormod(const QcServer& s, int e, float out[3]);

// The runner's half for the QC server: the lists, then the clients and every entity row. The lists
// are copied only at the map's start (`full`); a frame's fill rewrites the rest in place.
void fill_world(const QcServer& s, RulesWorld& w, bool full);

}  // namespace shooters_edge::server
