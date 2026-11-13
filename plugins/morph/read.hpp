// THE CLASSIC MODEL AND SPRITE FILES READ into `morph::Model` and `morph::Sprite` (model.hpp),
// every count, size and offset checked against the bytes before anything is read, so a damaged or
// truncated file is refused with what was wrong and where, never read out of range.
//
// ge:summary Reads classic vertex-animated model files and sprite files, refusing damage
// ge:keywords read mdl spr md2 sp2 idp2 ids2 model sprite parse check refuse header skin frame
// ge:keywords group second-model-family skin-paths
// ge:skill morph
// ge:test tests/morph_read_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>

#include "morph/model.hpp"

namespace ge::morph {

// The model file (magic "IDPO", version 6, or the second family's "IDP2", version 8, by its
// magic). `where` names the file in a refusal.
bool read_model(std::span<const uint8_t> bytes, const std::string& where, Model* out,
                std::string* error);
// The sprite file (magic "IDSP", version 1, or the second family's "IDS2", version 2).
bool read_sprite(std::span<const uint8_t> bytes, const std::string& where, Sprite* out,
                 std::string* error);

}  // namespace ge::morph
