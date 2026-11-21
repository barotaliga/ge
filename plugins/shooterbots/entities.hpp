// A MAP'S ENTITIES AS THE BOTS READ THEM: the map's entity text (`{ "key" "value" ... }` blocks)
// split into blocks of pairs in the text's order (plugins/brush's `parse_entities`, which every
// map the kit loads has passed), and a block's value by key.
//
// ge:summary A map's entity text as blocks of key/value pairs, and a block's value by key
// ge:keywords entities entity-text entity-block block_value parse_entity_text classname
// ge:skill shooterbots bots
// ge:test tests/shooterbots_entities_test.cpp
#pragma once

#include <string>
#include <vector>

#include "brush/map.hpp"
#include "shooterbots/kit.hpp"

namespace ge::shooterbots {

using EntityBlock = ge::brush::Entity;

// Splits `text` into blocks; false with `error` (the line and what to fix), `blocks` then empty.
bool parse_entity_text(const char* text, std::vector<EntityBlock>& blocks, std::string& error);

// The value of `key` in block `b` (the first such pair), or nullptr when it has none.
const char* block_value(const EntityBlock& b, const char* key);

}  // namespace ge::shooterbots
