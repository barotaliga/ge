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
// THE MAP'S EXITS too (`exits_init`): each `trigger_changelevel`'s box (or a `trigger_multiple`'s
// or `trigger_once`'s that fires a `target_changelevel`), the nodes a standing
// player there would touch it from (`kExitMargin` round it), and the nearest node clear of it by
// `kExitClear`, where a companion stands to show the leader the way (companion.hpp).
//
// An item is a door's when it stands inside the door's box grown by `kDoorKeeps`; read once a map,
// off the hot path.
//
// ge:summary The map's buttons: the doors each opens, the items they keep, a novice's random pokes
// ge:keywords button func_button shootable-button door func_door secret armour-door buttons_init
// ge:keywords button_poke button_node poke novice exit trigger_changelevel exits_init stand-by
// ge:keywords target_changelevel
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_footwork_test.cpp
#pragma once

#include <string>
#include <vector>

#include "shooter/map_spec.hpp"
#include "shooter/usercmd.hpp"
#include "shooterbots/entities.hpp"
#include "shooterbots/fighter.hpp"

namespace ge::shooterbots {

constexpr float kDoorKeeps = 160;       // an item this near a door's box is behind it
constexpr int kRelayDepth = 4;          // relays followed from a button to the doors it opens
constexpr float kExitMargin = 16;       // a node whose player box comes this near an exit is in it
constexpr float kExitClear = 48;        // the stand-by node: at least this clear of the exit's box
constexpr float kExitStandFar = 320;    // and no further than this from it
constexpr float kButtonPress_s = 0.3f;  // (`fields`) a touched button's press on a way, s
constexpr float kButtonShot_s = 0.8f;   // and a shot one's: the turn, the aim and the shot
constexpr float kShotFrom = 900;        // (`fields`) a shot button is shot from this near
constexpr float kDoorPushDmg = 2;  // a door's push on a blocker; one that hurts more is a crusher
constexpr double kDoorErrand_s = 12;  // a shut door's button gone for this long at most (`locked`)
constexpr double kDoorAvoid_s = 20;   // and a door whose button only it leads to, left this long

// The buttons off the map's entity text, linked to the server's entities, and the items behind
// their doors (a novice's items get no link: he knows no button).
// The targetnames a button's `target` fires (`target` first), through the relays between: a
// `trigger_relay`, `trigger_once` or `trigger_multiple` with a `targetname` fires its own `target`
// on, `kRelayDepth` deep. e1m2's floor plate fires a delayed trigger_once that opens the doors to
// the mega's teleporters.
std::vector<std::string> button_fires(const std::vector<ge::shooterbots::EntityBlock>& blocks,
                                      const char* target);

constexpr float kDoomDmg = 1000;  // a blast that hurts this much kills anyone in its reach

// Whether what a button's `target` fires (`button_fires`) reaches a killbox or a blast no one lives
// through: biggun's evacuate button, two relays and 22 s on, sets off both over the whole base.
bool button_dooms(const std::vector<ge::shooterbots::EntityBlock>& blocks, const char* target);
// Whether button `b` shows itself pressed by its look: its world row in `s`'s frame or effects
// changed from those it first had (`ButtonSpot::rest_frame`, noted on the first call). A button
// whose travel is all lip never moves: boss1's floor plates by the boss, pressed, show it only by
// the texture they switch to, and were walked round for minutes as never pressed until he ran off
// into the lava beside them.
bool button_looks_pressed(ButtonSpot& b, const ge::shooterbots::BotView& s);

// (`fields`) The map's buttons as the fields' errands (shooterbots/nav_field.hpp), one for each of
// `f.button_spots` in order (an errand's index is its button's): the node it is pressed or shot
// from, the doors that are a way once fired (`ButtonSpot::way_doors`), the press and the doors'
// time open.
std::vector<ge::shooterbots::NavErrand> field_errands(const Fighter& f,
                                                      const ge::shooterbots::NavGraph& g,
                                                      const ge::shooterbots::BotView& s);

void buttons_init(Fighter& f, const ge::shooterbots::NavGraph& g, const ge::shooterbots::BotView& s,
                  const char* entities);

// The exits off the map's entity text: the boxes of their brush models, the nodes in them, the
// stand-by node beside each. The boxes come from the server's brush models, else (a runner that
// keeps none, as Quake II's) from `spec`'s.
void exits_init(Fighter& f, const ge::shooterbots::NavGraph& g, const ge::shooterbots::BotView& s,
                const char* entities, const sh::MapSpec* spec = nullptr);

// A novice's whim, at a think: a button in sight now and then becomes where he goes and what he
// shoots at. True while he is on one.
bool button_poke(Fighter& f, const ge::shooterbots::BotView& s, int me, double t);

// (`pokelook`) The poke's view as the bot's look, set before its step: the step's hand turns onto
// the button as the one writer of the view (no second stroke after the step fighting the calm
// view's), held while the button was in sight within kPokeLookHold_s.
void button_poke_look(Fighter& f, const ge::shooterbots::BotView& s, int me, double t);

// The poke itself, once there: the view on the button and the trigger (or the walk into it).
void button_poke_cmd(Fighter& f, const ge::shooterbots::BotView& s, int me, double t,
                     ge::shooter::MoveCmd& cmd);

}  // namespace ge::shooterbots
