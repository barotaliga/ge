// The way through's bosses and puzzles (way_through.hpp): a Quake trigger only a shot fires, and
// the boss only the map's lightning hurts (e1m7's Chthon), with the buttons that work it.
//
// ge:summary The way through's shot triggers and the lightning boss: its electrodes, bolt, bridge
// ge:keywords way_shot_triggers way_bolt_init way_bolt_pick shot-trigger event-lightning chthon
// ge:keywords boss electrode lightning bolt
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_companion_test.cpp
#pragma once

#include <vector>

#include "shooter/map_spec.hpp"
#include "shooterbots/bot_view.hpp"
#include "shooterbots/entities.hpp"
#include "shooterbots/fighter.hpp"
#include "shooterbots/map_nav.hpp"
#include "shooterbots/way_through.hpp"

namespace ge::shooterbots {

// (`complete`) Each Quake trigger the map gives health (it answers only a shot) as a shot button of
// the fighter's: its edict found by its box, its doors those its target fires through relays,
// shot from the nearest node within kWayShotTriggerFrom that sees it.
void way_shot_triggers(Fighter& f, const MapNav& nav, const ge::shooterbots::BotView& s,
                       const ge::shooter::MapSpec& spec,
                       const std::vector<ge::shooterbots::EntityBlock>& blocks);

// (`complete`) Quake's Chthon, killed only by the bolt the map's `event_lightning` throws between
// its two electrodes (the doors whose target is `lightning`) while both have moved: fills
// `w.bolt` (empty on a map without him) from the plan's button steps.
void way_bolt_init(WayThrough& w, const Fighter& f, const ge::shooterbots::BotView& s,
                   const std::vector<ge::shooterbots::EntityBlock>& blocks);

// (`complete`) The step the lightning boss asks for now, or -1: nothing until what wakes him is
// done or once his death's doors have moved; an electrode at rest, its nearest button; both moved,
// the bolt's button.
int way_bolt_pick(const WayThrough& w, const ge::shooterbots::BotView& s);

}  // namespace ge::shooterbots
