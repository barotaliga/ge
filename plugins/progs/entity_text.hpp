// THE MAP'S ENTITY TEXT: the `{ "key" "value" ... }` blocks a map carries, read into blocks of
// pairs, and one block's keys written into an entity's fields by the progs' definitions. Which
// block becomes which entity, the spawn flags and the classname call are a game's (it allocates
// entities in its world); the reading and the key rules are any QC game's, so they live here.
//
// THE TEXT: white space and `//` comments between tokens; `{` and `}` are tokens of their own; a
// `"` starts a token that runs to the next `"` (the quotes dropped); anything else runs to white
// space. A block is `{`, then key and value tokens in pairs, then `}`. A missing `}`, a key with
// no value or a value that is a brace is a parse error naming the block.
//
// A KEY (`apply_entity_block`), in order: a key starting with `_` is ignored; `angle` becomes
// `angles` with the value "0 <value> 0"; `light` becomes `light_lev`; `alpha` and `colormod` go
// to the engine's own per-entity values (`EntityExtras`: alpha clamped 0..1; colormod three
// numbers each above 0, else ignored), never a field; then the field by name, its value read by
// the field's type: a string a new engine string with `\n` turned into a newline (a backslash
// followed by any other character becomes one backslash, the character dropped); a float or a
// vector by C's decimal reading (a vector three numbers separated by spaces, missing ones 0); an
// entity an entity number; a field a field's name and a function a function's name (an unknown
// name is a parse error that stops the map). A key that names no field is skipped with the
// message "<key> is not a field".
//
// ge:summary The map's entity text: blocks of key/value pairs, and a block's keys into fields
// ge:keywords qc progs entity-text entities map-entities ed-parse spawn key value classname
// ge:keywords angle angles light light_lev alpha colormod parse_entity_text apply_entity_block
// ge:skill progs
// ge:test tests/progs_entity_text_test.cpp
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "progs/vm.hpp"

namespace ge::progs {

struct EntityBlock {
  std::vector<std::pair<std::string, std::string>> pairs;  // in the text's order
  int line = 0;                                            // where the block starts
};

// Splits the text into blocks. False with `error` (which block, which line, what to fix).
bool parse_entity_text(const char* text, std::vector<EntityBlock>& blocks, std::string& error);

// The engine's own values a block may carry (sent to clients that take them, carried by
// makestatic).
struct EntityExtras {
  bool has_alpha = false;
  float alpha = 1;
  bool has_colormod = false;
  float colormod[3] = {1, 1, 1};
};

// The value of one key a block gives (after the renames), or nullptr.
const char* block_value(const EntityBlock& b, const char* key);

// Writes the block's keys into entity `e`'s fields (the entity must be in use). False on a parse
// error that stops the map (`error` says which key); keys that name no field are skipped, each
// with a line in `messages`.
bool apply_entity_block(Vm& vm, int e, const EntityBlock& b, EntityExtras& extras,
                        std::vector<std::string>& messages, std::string& error);

}  // namespace ge::progs
