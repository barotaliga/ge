// WHAT THE ENGINE DRAWS IN PLACE OF A PACK FILE: the classic pack's effects that a modern
// effect of the engine's replaces (a sprite explosion, a flame drawn as a flickering model),
// stated once here. The models stage writes them as `replaced.txt` in the cache, one line a
// row, so the renderer that reads the cache (row M41c) draws the engine's effect where the map
// or the game asks for the pack's file, and an agent sees why.
//
// - `effect` is an id the renderer maps to its own effect: `explosion`, `flare` and `bubbles`
//   are the particle effects; `fire.<kind>` is the brush renderer's fire of that kind
//   (`wall_torch`, `large`), the flame and its flickering light.
// - `keep_glowing = false`: the file is still written to the cache, but without its glowing
//   pieces (`drop_glowing`, models.hpp): a torch keeps its stick and cage, and the engine's fire
//   burns where its painted flame was.
// - A sprite listed here is still written (every pack file stays usable); the row says the
//   engine does not draw it by default.
//
// ge:summary The pack's effects the engine replaces with its own: file, effect id, why
// ge:keywords replace effect explosion flare bubbles fire torch flame sprite model glow
// ge:skill classicenhance
// ge:test tests/classicenhance_pipeline_test.cpp
#pragma once

#include <string_view>

namespace ge::classicenhance {

struct Replacement {
  std::string_view path;    // the pack file, as the pack names it
  std::string_view effect;  // the engine's effect drawn in its place
  bool keep_glowing;        // false: the cached model drops its glowing pieces
  std::string_view why;
};

inline constexpr Replacement kReplacements[] = {
    {"progs/s_explod.spr", "explosion", true,
     "a flat sprite; the engine's explosion is a lit, shaded volume with smoke and sparks"},
    {"progs/s_light.spr", "flare", true,
     "a flat sprite; the engine's flare is a lit glow with a light that reaches the walls"},
    {"progs/s_bubble.spr", "bubbles", true,
     "a flat sprite; the engine's bubbles rise, wobble and refract"},
    {"progs/flame.mdl", "fire.wall_torch", false,
     "a painted flame; the engine's fire flickers, lights the room and casts moving shadows"},
    {"progs/flame2.mdl", "fire.large", false,
     "a painted flame; the engine's fire flickers, lights the room and casts moving shadows"},
};

// The row for a pack path; nullptr when the engine draws the file itself.
inline const Replacement* replacement_of(std::string_view path) {
  for (const Replacement& r : kReplacements) {
    if (r.path == path) return &r;
  }
  return nullptr;
}

}  // namespace ge::classicenhance
