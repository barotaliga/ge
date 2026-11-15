// A MAP'S INTENT, DRAFTED from what lies on it: one `area` a room that holds a power item or a key
// weapon, for a map nobody wrote a plan for (a proven map studied before a new one, map-design
// skill section 0). The text is an `.intent` file (plugins/brush/compile/plan.hpp's `read_intent`)
// the map-overview and map-intent labs read like a written one.
//
// - The items it looks for, by classname (the first game's and the sequel's names): the best
//   armour (a CAMP: one player holds it), the second armour (HOLD), the powers and the mega health
//   (POWER), the rocket launcher, the lightning gun and the rail (FIGHT).
// - An item's ROOM is the floor round it: the map cut level at 24 units over the item
//   (`slice_map`, 16-unit cells), the open cells joined to the item's flooded out to `reach` units
//   at most each way, their box. The area spans 40 under the item to 200 over it.
// - Two items whose rooms overlap by more than 60 % (of the smaller), within 96 units of height,
//   are one area named for the more telling intent (camp, then power, hold, fight); its note
//   lists both.
//
// It is a DRAFT: the rooms a flood finds are rough where a room opens wide, and the play a map is
// known for (its middle's fights, its routes) is not on its items. Read the overview and edit it.
//
// ge:summary A map's intent drafted from its items: an area a room round each power and weapon
// ge:keywords intent draft-intent auto-intent area camp hold power fight room flood-fill slice
// ge:keywords map-design reference-maps proven-maps items
// ge:skill map-design shooter
// ge:test tests/shooter_intent_draft_test.cpp
#pragma once

#include <string>

#include "brush/map.hpp"

namespace ge::shooter {

struct IntentDraftOptions {
  double reach = 352;  // how far a room's flood goes from its item each way, units
};

// The `.intent` text drafted from `map`'s items: a header comment and one `area` line an area
// (none when the map has none of the items). Never fails; a room the slice cannot find is a
// 256-unit box round its item.
std::string draft_intent(const ge::brush::Map& map, const IntentDraftOptions& opt = {});

}  // namespace ge::shooter
