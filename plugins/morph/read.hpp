// THE CLASSIC MODEL AND SPRITE FILES READ into `morph::Model` and `morph::Sprite` (model.hpp),
// every count, size and offset checked against the bytes before anything is read, so a damaged or
// truncated file is refused with what was wrong and where, never read out of range.
//
// ge:summary Reads classic vertex-animated model files and sprite files, refusing damage
// ge:keywords read mdl spr model sprite parse check refuse header skin frame group
// ge:skill morph
// ge:test tests/morph_read_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>

#include "morph/model.hpp"

namespace ge::morph {

// The model file (magic "IDPO", version 6). `where` names the file in a refusal.
bool read_model(std::span<const uint8_t> bytes, const std::string& where, Model* out,
                std::string* error);
// The sprite file (magic "IDSP", version 1).
bool read_sprite(std::span<const uint8_t> bytes, const std::string& where, Sprite* out,
                 std::string* error);

}  // namespace ge::morph
