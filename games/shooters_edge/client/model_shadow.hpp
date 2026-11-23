// THE MODELS' GROUND SHADOWS (Quake II's `gl_shadows`, Quake's and QuakeWorld's `r_shadows`): each
// animated model's dark copy squashed flat onto the floor under it, half see through. Quake II's
// leans along the way the model faces; Quake's (`fixed_light`) all lean the same way, as if lit
// from the map's +x at 45 degrees, whichever way the model faces, and lie on any floor however
// steep. The window draws it as render/morph.hpp's flat shadow (`MorphDraw::flat`); this file holds
// the rule alone, with no GPU.
//
// | Mode | What is drawn |
// | --- | --- |
// | 0 | nothing (the default) |
// | 1 | the shadow under each animated model at half strength, however high it stands |
// | 2 | the same, fading as the model rises: gone at four of its radii above the floor |
//
// None under a sprite or a brush model, a see-through model, one whose render bits say it casts
// none (`Placed::no_shadow`), a shell, or the held weapon (drawn by another pass); none when no
// floor is found under it, or the floor is steeper than sixty degrees. The shadow lies a unit above
// the floor so the two never fight for the same depth.
//
// ge:summary The models' ground shadows: the mode, the floor, the fade with height, the lean
// ge:keywords shadow gl_shadows ground-shadows blob-shadow flat-shadow model-shadow no-shadow
// ge:keywords rf_noshadow shadow_of shadow-fade shadow-lean r_shadows fixed-light
// ge:skill render shooter
// ge:test tests/shooters_edge_model_shadow_test.cpp
#pragma once

namespace shooters_edge::client {

inline constexpr float kShadowAlpha = 0.5f;    // how dark a shadow is at its darkest
inline constexpr float kShadowLean = 0.7f;     // across the floor for each unit of height
inline constexpr float kShadowSteepUp = 0.5f;  // a floor's upward share under which none is cast
inline constexpr float kShadowFadeRadii = 4;   // mode 2: gone this many radii above the floor
inline constexpr float kShadowLiftUnits = 1;   // the shadow above its floor
// Quake's fixed light: from the map's +x at 45 degrees, so every point slides toward -x by its
// height times the cosine of 45 degrees.
inline constexpr float kShadowFixedLean = 0.70710678f;

struct ShadowIn {
  int mode = 0;              // 0, 1, 2 (the table above)
  bool animated = false;     // a vertex-animated model (not a sprite or a brush model)
  bool see_through = false;  // drawn see-through (its alpha under 1)
  bool no_shadow = false;    // its render bits say it casts none
  bool floor_found = false;  // a floor under it
  float floor_up = 1;        // that floor's upward share (1 level)
  float height_units = 0;    // its origin above the floor
  float radius_units = 0;    // its model's radius
  float yaw_deg = 0;         // the way it faces, about the map's up
  bool fixed_light = false;  // Quake's rule: the lean toward -x whatever the yaw, any floor
};

struct Shadow {
  float alpha = 0;               // 0: none
  float lean_x = 0, lean_y = 0;  // map units across for each unit of height
};

Shadow shadow_of(const ShadowIn& in);

}  // namespace shooters_edge::client
