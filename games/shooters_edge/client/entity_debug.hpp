// THE ENTITIES IN VIEW: a model the eye stands inside is never drawn (`encloses_eye`); and, FOR A
// RECIPE (docs/console.md § Recipes): what a joined server or a demo
// puts in the world, listed by number with its model, place, effects, alpha and scale
// (`entlist`), and hidden by number or model name (`enthide`, `entshow`) so a picture shows what
// one of them draws. Registered only in a window started with -recipes; a player's window has
// none of it, and with nothing hidden `entity_hidden` is always false.
//
// ge:summary A recipe's entity tools: entlist, enthide, entshow on the view watched
// ge:keywords recipe entlist enthide entshow entities-in-view debug-entities hide-entity
// ge:keywords encloses-eye inside-model spawn-marker
// ge:skill window-recipes shooter
// ge:test tests/shooters_edge_entity_debug_test.cpp
#pragma once

#include <string>
#include <vector>

#include "console/console.hpp"
#include "shooter/vec.hpp"

namespace shooters_edge::client {

struct DemoState;
struct PlaceSeen;

// What a recipe hid: entity numbers and model names ("progs/flame.mdl", "*12" a brush model).
struct HiddenEntities {
  std::vector<int> numbers;
  std::vector<std::string> models;
};

// Whether `eye` stands inside a player's box at `origin` (16 units either way, 24 down, 32 up):
// demo_placed draws no model there, since a model round the eye shows only its open insides.
bool encloses_eye(const ge::shooter::Vec3f& origin, const ge::shooter::Vec3f& eye);

// The window's one set (empty unless a recipe hid something).
HiddenEntities& hidden_entities();
// Whether entity `number` with model `model` is hidden.
bool entity_hidden(const HiddenEntities& h, int number, const std::string& model);

// `entlist [radius]`: the present entities of `s` nearest the eye first (within `radius` units,
// default all), one line each: number, model, origin, distance, frame, skin, effects, alpha,
// scale. Players (client and multiview demos) are listed by slot as "p<slot>".
std::string entity_list(const DemoState& s, float radius);

// `entlist`, `enthide <number|model>...`, `entshow [<number|model>...]` (none: all shown again),
// on the view `seen` watches. `seen` must outlive `con`.
void add_entity_debug_commands(ge::console::Console& con, const PlaceSeen& seen);

}  // namespace shooters_edge::client
