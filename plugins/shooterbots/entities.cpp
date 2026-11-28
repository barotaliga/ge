// A map's entities as the bots read them (entities.hpp).
//
// ge:summary A map's entity text split into blocks, and a block's value by key
// ge:keywords entities parse_entity_text block_value
// ge:skill shooterbots bots
// ge:test tests/shooterbots_entities_test.cpp
#include "shooterbots/entities.hpp"

#include <cstring>

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

bool name_fired(const std::vector<EntityBlock>& blocks, const char* name) {
  if (!name) return false;
  for (const EntityBlock& d : blocks)
    for (const char* key : {"target", "killtarget", "pathtarget"}) {
      const char* v = block_value(d, key);
      if (v && std::strcmp(v, name) == 0) return true;
    }
  return false;
}

}  // namespace ge::shooterbots
