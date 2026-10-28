// THE STORE'S CONTROLLER LAYER: a store client (Steam Input) that reads the player's controller
// itself and hands the game ACTIONS, not buttons. The game declares its action map once (the
// contexts become the store's action sets, the actions its digital and analog actions); the
// store binds them to physical controls by its own configuration (the defaults the game ships,
// then the player's edits in the store's configurator), and each frame the game reads every
// connected controller's action values and merges them into its own (`merge_store_values`), so
// a keyboard and a store-read pad drive the same actions at once. `origins` says which physical
// controls an action is on now, with a glyph name and the label the controller's family prints,
// for the HUD's prompts. Steam's backend (engine/steam, docs/goals.md M20b) implements this over
// its SDK; `SimControllers` (sim_controllers.hpp) is the store's controller layer in memory.
//
// A store-read controller is NOT also read by engine/input's platform backend: the store hides
// it (Steam does) or the game drops it from `input::Sources::only`, so one pad is never heard
// twice. Feedback goes through `rumble` for a store-read controller.
//
// ge:summary The store's controller layer: action sets, store-read action values, origins, glyphs
// ge:keywords services steam-input controller-layer action-set store-actions origin origins
// ge:keywords glyph prompt configurator binding-panel in-game-actions action-manifest merge
// ge:keywords rumble store-controller
// ge:skill services input
// ge:test tests/services_controllers_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "input/actions.hpp"
#include "input/device.hpp"
#include "services/backend.hpp"

namespace ge::services {

using ControllerHandle = uint64_t;  // the store's number for a connected controller; 0 is none

struct ControllerInfo {
  ControllerHandle handle = 0;
  input::Family family = input::Family::generic;
  std::string name;
};

// A physical control an action is bound to, as the engine's source, with what a prompt draws:
// `glyph` names the picture (`xbox_a`, `ps_cross`, `stick_left`, ...: a HUD's art is keyed on
// it) and `label` is the word the family prints ("A", "cross", "left stick").
struct Origin {
  input::Source source;
  std::string glyph;
  std::string label;
};

class ControllerLayer {
 public:
  virtual ~ControllerLayer() = default;

  // The game's actions, once at start-up, before any read. `invalid` when a name breaks the
  // store's rules (an action or a context named empty, or twice). Every context of the map is an
  // action set. The map's active contexts with a store-readable action start the reading: the
  // first is the set, the others layers over it.
  virtual Result declare(const input::ActionMap& map) = 0;
  // Make `context` the action set the store reads (the menu's, the ride's), with no layers;
  // `unknown_id` when the declared map has no such context. A store reads ONE set at a time,
  // plus the layers laid over it (a context a game keeps on beside it: the app's keys).
  virtual Result activate(std::string_view context) = 0;
  virtual Result set_layer(std::string_view context, bool on) = 0;
  // The controllers the store reads now, in the order they connected.
  virtual std::span<const ControllerInfo> controllers() const = 0;
  // One controller's action values this frame, one per declared action (the inactive sets'
  // actions read zero); false when the handle is gone. Call once a frame after the backend's
  // poll; `dt_s` is the frame's length, for a delta read at a rate.
  virtual bool read(ControllerHandle c, double dt_s, input::ActionValues& out) = 0;
  // The physical controls `action` (an index into the declared defs) is bound to on `c` now.
  virtual std::vector<Origin> origins(ControllerHandle c, uint16_t action) const = 0;
  // The two motors, 0 .. 1, until the next call.
  virtual Result rumble(ControllerHandle c, float low, float high) = 0;
  // Open the store's configurator for `c` (the player rebinds there); `unsupported` when the
  // store's overlay cannot show it now.
  virtual Result show_bindings(ControllerHandle c) = 0;
};

// What `family` prints on a pad source, and the glyph a HUD draws for it.
Origin origin_of(input::Family family, const input::Source& source);

// Whether a source is one a store's controller layer reads (a pad's button, axis or stick).
bool store_readable(const input::Source& source);

// Fold a controller's values into the game's own, by the rules every source of an action is
// read by: buttons OR'd (with their edges), the larger push wins on an axis or a stick, deltas
// add. `into` and `store` are both one value per def of `map`.
void merge_store_values(const input::ActionMap& map, const input::ActionValues& store,
                        input::ActionValues& into);

// The store's action manifest for the map: Steam's in-game actions file, which the store reads
// to list the game's sets and actions in its configurator, and its default configuration's
// bindings, one line per pad source the map binds ("ride steer = stick:left"). Written from the
// map so the two never drift; `ge` writes them beside the build (M20b).
std::string action_manifest_text(const input::ActionMap& map);
std::string default_config_text(const input::ActionMap& map);

}  // namespace ge::services
