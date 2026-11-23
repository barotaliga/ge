// GLOW DISCS FOR THE MOMENT'S LIGHTS (Quake's and QuakeWorld's `gl_flashblend`): instead of
// lighting the walls, each live dynamic light (a muzzle's flash, a rocket, an explosion, a power's
// glow) is drawn as a flat disc turned to the eye, added to the picture: its middle the light's
// glow, its rim black, smoothly shaded between. The walls and the doors and lifts then take no
// light from it; the figures, monsters and items still do.
//
// - THE DISC: 0.35 of the light's radius across from its middle to its rim, its middle pulled
//   that same distance toward the eye, drawn after the world and the models, depth tested.
// - ITS COLOUR: Quake's one dim orange (0.2, 0.1, 0) for every light; QuakeWorld's the light's
//   own of four: dim orange (0.2, 0.1, 0.05), blue (0.05, 0.05, 0.3), red (0.5, 0.05, 0.05) or
//   purple (0.5, 0.05, 0.4), picked here as the nearest to the light's colour.
// - THE EYE INSIDE IT (nearer the light than the disc's radius): no disc, but the whole screen
//   tinted orange (1, 0.5, 0) by the light's radius in units times 0.0003 (about 0.1 for a
//   blast of 350 units).
// - QUAKEWORLD'S OWN: the player's own lights (his power's glow, his gun's flash) make no disc.
//
// The rule alone, no GPU: the window turns a `Glow` into an added sprite (world_models.cpp) and the
// tint into the screen's blend (the game's look.cpp).
//
// ge:summary Glow discs for the moment's lights (gl_flashblend): size, place, colour, eye inside
// ge:keywords gl_flashblend flashblend flash-blend glow-disc dynamic-light dlight glow tint
// ge:keywords flash_glow flash_tint
// ge:skill classic render
// ge:test tests/classic_flash_blend_test.cpp
#pragma once

namespace ge::classic {

inline constexpr double kFlashDiscShare = 0.35;      // the disc's radius, of the light's
inline constexpr double kFlashTintPerUnit = 0.0003;  // the inside tint's strength a unit of radius
inline constexpr double kFlashTint[3] = {1, 0.5, 0};

enum class FlashGame : unsigned char { quake, quakeworld };

struct FlashIn {
  double light[3] = {0, 0, 0};   // the light's place, map units
  double radius_units = 0;       // where its light ends (0: none)
  double colour[3] = {1, 1, 1};  // its colour, any scale (QuakeWorld picks the nearest of four)
  double eye[3] = {0, 0, 0};     // map units
  FlashGame game = FlashGame::quake;
  bool own = false;  // the player's own light (QuakeWorld makes no disc of it)
};

struct Glow {
  bool disc = false;             // a disc drawn
  double centre[3] = {0, 0, 0};  // its middle, map units (pulled toward the eye)
  double radius_units = 0;       // middle to rim
  double colour[3] = {0, 0, 0};  // its middle's colour, added; the rim black
  double tint = 0;               // the eye inside: the screen's orange tint, 0..1
};

Glow flash_glow(const FlashIn& in);

// The screen's blend `rgba` (colour 0..1, alpha its share) with `tint` of kFlashTint laid over it.
void flash_tint(double tint, double rgba[4]);

}  // namespace ge::classic
