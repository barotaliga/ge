// PART OF A RUN (docs/saves.md § Part of a run): a character is a game's named set of chunks kept
// as a file of its own kind (`Kind::character`), in the same format and read by the same tools as
// a whole run's save. `extract` copies those chunks out of a run's save, byte for byte (each
// chunk's tag, instance, version and decoded bytes), under a meta that says what it is; the game
// reads the file into a run on another setup (play/saves.hpp's `import_character`).
//
// ge:summary Part of a run: a character's chunks copied out of a run's save into a file of its own
// ge:keywords character part extract chunk-set profile carry import kind
// ge:skill save
// ge:test tests/save_part_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ge::save {

// Every chunk of `run` (a whole run's save of game `game`) whose tag is one of `tags`, copied into
// `out` as a character file: the run's meta with its kind set to `character` and its title set
// to `title` (kept when empty). False with `problem` (what and the next action) when `run` does
// not open as `game`'s, is not a whole run, or holds none of a tag's chunks.
bool extract(std::span<const uint8_t> run, std::string_view game, std::span<const uint32_t> tags,
             std::string_view title, std::vector<uint8_t>& out, std::string& problem);

}  // namespace ge::save
