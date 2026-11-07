// THE MAP'S BUTTONS: which doors each opens and which items those doors keep, read off the map's
// own entities (a `func_button`'s `target` names the `func_door`s it opens, itself or through the
// relays it fires: a `trigger_relay`, or a `trigger_once` or `trigger_multiple` another fires; one
// with `health` over 0 is opened by a shot, the rest by a touch), and how each kind of player uses
// them:
// - an AVERAGE or EXPERT player knows them: an item behind a shut door is gone for through its
//   button (`ItemSpot::button_*`), from the nearest place that reaches it (a touch) or sees it (a
//   shot);
// - a NOVICE knows none of it: now and then he shoots at a button he sees, or walks into it, to
//   see what it does (`button_poke`).
//
// THE MAP'S EXITS too (`exits_init`): each `trigger_changelevel`'s box, the nodes a standing
// player there would touch it from (`kExitMargin` round it), and the nearest node clear of it by
// `kExitClear`, where a companion stands to show the leader the way (companion.hpp).
//
// An item is a door's when it stands inside the door's box grown by `kDoorKeeps`; read once a map,
// off the hot path.
//
// ge:summary The map's buttons: the doors each opens, the items they keep, a novice's random pokes
// ge:keywords button func_button shootable-button door func_door secret armour-door buttons_init
// ge:keywords button_poke button_node poke novice exit trigger_changelevel exits_init stand-by
// ge:skill shooter bots
// ge:test tests/shooters_edge_footwork_test.cpp
#pragma once

#include <string>
#include <vector>

#include "progs/entity_text.hpp"
#include "shooter/usercmd.hpp"
#include "shooters_edge/bots/fighter.hpp"

namespace shooters_edge::bots {

constexpr float kDoorKeeps = 160;     // an item this near a door's box is behind it
constexpr int kRelayDepth = 4;        // relays followed from a button to the doors it opens
constexpr float kExitMargin = 16;     // a node whose player box comes this near an exit is in it
constexpr float kExitClear = 48;      // the stand-by node: at least this clear of the exit's box
constexpr float kExitStandFar = 320;  // and no further than this from it

// The buttons off the map's entity text, linked to the server's entities, and the items behind
// their doors (a novice's items get no link: he knows no button).
// The targetnames a button's `target` fires (`target` first), through the relays between: a
// `trigger_relay`, `trigger_once` or `trigger_multiple` with a `targetname` fires its own `target`
// on, `kRelayDepth` deep. e1m2's floor plate fires a delayed trigger_once that opens the doors to
// the mega's teleporters.
std::vector<std::string> button_fires(const std::vector<ge::progs::EntityBlock>& blocks,
                                      const char* target);

void buttons_init(Fighter& f, const ge::shooter::NavGraph& g, const server::RulesCommon& s,
                  const char* entities);

// The exits off the map's entity text: the boxes of their brush models, the nodes in them, the
// stand-by node beside each.
void exits_init(Fighter& f, const ge::shooter::NavGraph& g, const server::RulesCommon& s,
                const char* entities);

// A novice's whim, at a think: a button in sight now and then becomes where he goes and what he
// shoots at. True while he is on one.
bool button_poke(Fighter& f, const server::RulesCommon& s, int me, double t);

// The poke itself, once there: the view on the button and the trigger (or the walk into it).
void button_poke_cmd(Fighter& f, const server::RulesCommon& s, int me, double t,
                     ge::shooter::MoveCmd& cmd);

}  // namespace shooters_edge::bots
