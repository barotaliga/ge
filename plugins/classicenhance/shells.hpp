// SPENT CASES (ce_shells): what a classic game's guns throw out as they fire, made here because the
// classic packs have none: the cases' kinds and sizes, how each bounces (engine/detail/debris.hpp's
// shapes), each drawn as a small vertex-animated model coloured from the game's own palette, and
// the sound each makes landing on a surface of each class (engine/detail/surfaces.hpp), made
// from its modes. Which gun throws which, and when, is each game's own (the game's table); the
// cases are the same in every classic game.
//
// - A SHOTGUN SHELL (`Casing::hull`): a red plastic hull on a brass head, 80 mm by 25 mm; it lands
//   with a dull knock and a short ring of its head.
// - A RIFLE'S CASE (`Casing::brass`): brass, 60 mm by 15 mm, ringing bright when it lands on
//   something hard.
// - A HEAVY GUN'S CASE (`Casing::heavy`): brass, 125 mm by 25 mm, ringing lower and longer.
// Each a little larger than life, as the games' own things are, so a case reads at a few metres.
//
// A case's long axis is its model's +x (the brass head at -x), so the debris pool's body axes are
// the model's.
//
// ge:summary Spent cases: their kinds, bounce, palette-coloured model and landing sounds
// ge:keywords classicenhance shells spent-case casing shotgun-shell brass eject ce_shells
// ge:keywords casing_model casing_sound casing_shapes landing-sound tink
// ge:skill classicenhance
// ge:test tests/classicenhance_shells_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "audio/pcm.hpp"
#include "detail/debris.hpp"
#include "detail/surfaces.hpp"
#include "morph/model.hpp"

namespace ge::classicenhance {

enum class Casing : uint8_t { hull, brass, heavy };
inline constexpr int kCasings = 3;

// A case's size, in map units (2.5 cm), and its name in a sound's name.
struct CasingSize {
  float length_u = 0, radius_u = 0;
  std::string_view name;
};
CasingSize casing_size(Casing c);

// Each case's bounce, by Casing (the debris pool's kinds).
std::span<const detail::DebrisShape> casing_shapes();

// The case drawn: one frame, one skin of the palette's (768 bytes) nearest colours, the glowing
// end of the palette (indices 224 and up) never used.
morph::Model casing_model(Casing c, std::span<const uint8_t> palette);

// The sound of a case landing on a surface of class `on`, variant `variant` (any number: the same
// number, the same sound), at kCasingRate.
inline constexpr int kCasingRate = 48000;
inline constexpr int kCasingVariants = 4;
audio::Pcm casing_sound(Casing c, detail::SurfaceClass on, int variant);

// The name a game asks its sounds for one by ("ce/shell/<case>/<class>/<variant>"), and back:
// false for a name that is not one.
std::string casing_sound_name(Casing c, detail::SurfaceClass on, int variant);
bool parse_casing_sound(std::string_view name, Casing* c, detail::SurfaceClass* on, int* variant);

}  // namespace ge::classicenhance
