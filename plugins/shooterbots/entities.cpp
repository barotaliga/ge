// A map's entities as the bots read them (entities.hpp).
//
// ge:summary A map's entity text split into blocks, and a block's value by key
// ge:keywords entities parse_entity_text block_value
// ge:skill shooterbots bots
// ge:test tests/shooterbots_entities_test.cpp
#include "shooterbots/entities.hpp"

#include "brush/read.hpp"

namespace ge::shooterbots {

bool parse_entity_text(const char* text, std::vector<EntityBlock>& blocks, std::string& error) {
  if (ge::brush::parse_entities(text ? text : "", &blocks, &error)) return true;
  blocks.clear();
  return false;
}

const char* block_value(const EntityBlock& b, const char* key) {
  for (const auto& [k, v] : b.pairs)
    if (k == key) return v.c_str();
  return nullptr;
}

}  // namespace ge::shooterbots
