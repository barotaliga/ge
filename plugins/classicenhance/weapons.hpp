// THE CLASSIC PACK'S HELD WEAPONS, put into the engine's view-model layer (engine/viewmodel): the
// first-person models the pack carries (`progs/v_*.mdl`) read by the classic model reader
// (plugins/morph) and turned into a `viewmodel::HeldModel`, placed the way the classic game places
// them, and lit by the map the player stands in.
//
// - **The form** (`held_from_morph`): every frame, metres, in the held frame (+x right, +y up, +z
//   forward). The file authors a held weapon at the EYE (its origin is the eye, +x forward, +y
//   left, +z up, map units), so the model needs no grip mount: the eye is the held frame's origin
//   and the classic row (`classic_weapon_row`) puts it there. The file's axes are right-handed and
//   the held frame's left-handed, so the triangles are wound the other way round to keep their
//   fronts out. Normals are the frame's own smooth ones (`morph::frame_normals`).
// - **The skin**: the palette indices through the pack's palette, and the GLOW mask from the
//   palette's fullbright range (`kFullbrightFirst` up): a sight or a nail gun's lamp lights itself.
// - **The light** (`probe_under`): the classic game lights a model by the floor under it, so the
//   probe is the lightmap of the first surface straight below the player, read where the line down
//   meets it. A luxel of 128 is a surface as painted, so 128 is 1 in the view model's light unit;
//   it is split into an ambient share and a direct share from above. The classic game never lets
//   the held weapon go darker than 24 of those 128, which is the floor `kClassicFloor`.
// - **The dynamic lights** (`muzzle_flash_light`, `flame_lights`): a shot's flash at the muzzle,
//   the classic size and life; and every torch and flame the map places as a live light, for the
//   enhanced mode whose torches are lights of their own (otherwise their light is in the lightmaps
//   already and adding them counts it twice).
//
// Everything here reads what the pack holds; nothing it makes is committed or shipped
// (docs/classic-enhance.md).
//
// ge:summary The classic pack's held weapons as view models: the form, the skin, the floor's light
// ge:keywords viewmodel view-model held-weapon first-person weapon mdl morph classic palette
// ge:keywords fullbright glow light-point lightmap probe floor muzzle-flash torch flame dlight
// ge:keywords held_from_morph probe_under classic_weapon_row
// ge:skill classicenhance
// ge:test tests/classicenhance_weapons_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "classicenhance/texture.hpp"
#include "morph/model.hpp"
#include "viewmodel/held.hpp"
#include "viewmodel/light.hpp"
#include "viewmodel/place.hpp"

namespace ge::classicenhance {

// The least light on the held weapon, as a share of white: the classic 24 of a luxel's 128.
inline constexpr float kClassicFloor = 24.0f / 128.0f;
// How far down the light under the player is looked for, map units.
inline constexpr double kProbeDepth = 2048;
// A classic frame's time, s: the held weapons animate at ten frames a second.
inline constexpr double kFrameSeconds = 0.1;

// `model` as a held model: every frame into the held frame in metres, the skin `skin` through
// `palette` with its fullbright texels as the glow mask.
viewmodel::HeldModel held_from_morph(const morph::Model& model, const Palette& palette,
                                     int skin = 0);

// The classic placement: the weapon at the eye (no offset), riding forward with the bob, a little
// lag on a turn, the classic kick.
viewmodel::PlaceRow classic_weapon_row();

// The light under `at` (map units, z up): the lightmap of the first surface straight below it,
// within kProbeDepth, as a probe in the engine's frame. `found` (optional) says whether a lit
// surface was there (without one the probe is dark: the gun gets the floor alone), and
// `floor_z` (optional) the height it was met at, map units.
viewmodel::Probe probe_under(const brush::Map& map, const float at[3], bool* found = nullptr,
                             float* floor_z = nullptr);

// The shot's light at `muzzle` (engine frame, m): the classic flash, warm, reaching 5 m.
viewmodel::PointLight muzzle_flash_light(const double muzzle[3]);

// Every torch and flame the map places (its `light_torch*` and `light_flame*` entities), as live
// point lights in the engine's frame, each as bright as its `light` key says.
std::vector<viewmodel::PointLight> flame_lights(const brush::Map& map);

// The pack's held weapons by name (`progs/v_*.mdl`, sorted), read; a model the reader refuses is
// skipped with its reason in `*skipped`.
struct PackWeapon {
  std::string name;  // the file's path in the pack
  morph::Model model;
};
std::vector<PackWeapon> pack_weapons(std::span<const uint8_t> pack,
                                     std::vector<std::string>* skipped = nullptr);

}  // namespace ge::classicenhance
