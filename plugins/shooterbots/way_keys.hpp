// The way through's keys (way_through.hpp): Quake's key items and the locks that want them
// beyond a key door, such as Scourge of Armagon's keycard hatch.
//
// ge:summary Quake's keys' item bits and the keycard hatches shut to him until he holds the key
// ge:keywords way_key_hatches key-door keycard trigger_usekey func_rotate_door func_movewall hatch
// ge:keywords silver-key gold-key hip1m2
// ge:skill shooterbots bots
// ge:test tests/shooters_edge_companion_test.cpp
#pragma once

#include <vector>

#include "shooterbots/bot_view.hpp"
#include "shooterbots/entities.hpp"
#include "shooterbots/way_through.hpp"

namespace ge::shooterbots {

constexpr int kKeySilver = 131072, kKeyGold = 262144;  // Quake's keys' item bits

// Whether the map's keys are Quake's (items, `item_key*`), whose doors open to a touch.
bool way_quake_keys(const std::vector<EntityBlock>& blocks);

// Scourge of Armagon's lock: a `trigger_usekey` that, touched with its key, fires turning doors
// (`func_rotate_door`) that carry the walls (`func_movewall`) closing a hatch round with them.
// Each such wall is shut to him like a key door's (`WayThrough::key_doors`, `locked`, and
// `gates`, the doors barred by the links through them) until he holds the key: hip1m2's
// underwater hatchway was swum into for minutes without the silver keycard. The key is spawnflag
// 16 (silver) or 8 (gold), else the one the lock's message names. Only on maps whose keys are
// Quake's items.
void way_key_hatches(WayThrough& w, const BotView& s, const std::vector<EntityBlock>& blocks,
                     std::vector<int>& gates);

}  // namespace ge::shooterbots
