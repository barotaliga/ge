// THE FRAME'S LIGHTS CHOSEN (bsp_draw.hpp's v2 lit in real time): which of the map's lights a frame
// draws, in what order, which of them cast shadows, and which cube slot each shadowed one reads.
// Pure CPU work, apart from the GPU, so a lab can walk a map and count what changes from one step
// to the next (the `bsp-light-set` lab) and a test can hold it.
//
// - THE DRAWN SET: every light whose reach can touch what the eye sees: its sphere inside the cone
//   round the look that holds the widest screen, and one of the clusters its sphere touches in the
//   eye's potentially visible set. Nothing else culls a light, so the set changes only where a
//   light's change cannot show: behind the eye, behind a wall the map's visibility already hides.
//   A frame holds `most` lights at most; past it the least important are left out.
// - THE SHADOWED FEW: the drawn lights in view first, by importance (their light at the eye,
//   candela over the squared distance less the reach, so a big light across the room outranks a
//   small one beside it only as far as it lights more), then the rest by the same rule.
// - THE CUBES (`deal_cubes`): a light keeps the slot that holds its cube; the new ones take the
//   slot left unused longest, at most a budget a frame. A drawn light with no cube yet lights
//   NOTHING (it is faded in as its cube arrives, `CubeDeal::fade`), never unshadowed through the
//   walls: a light through a wall is the pop the eye catches.
//
// ge:summary The BSP's frame lights chosen: the drawn set by view and visibility, the shadowed few by
// ge:summary importance, the cube slots dealt and the lights faded in and out with their cubes
// ge:keywords bsp brush light-pick choose_lights deal_cubes light-pop light-swap stable-lights
// ge:keywords shadow-slot shadow-pick shadow-pop pvs-lights light-importance light-fade
// ge:skill brush render
// ge:test tests/brush_bsp_light_pick_test.cpp
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "brush/app/draw_data.hpp"
#include "brush/map.hpp"
#include "render/frame.hpp"

namespace ge::brush::app {

// The shadow cubes a device may hold at most, a light each.
inline constexpr int kMaxCubeSlots = 96;

// One light offered to a frame: the map's, a pool's, a teleporter's, or the moment's (dynamic:
// it moves or goes out, so it casts no shadow).
struct LightOffer {
  const PointLight* light = nullptr;
  bool dynamic = false;
};

// Where a light shines from and how far, as the GPU reads it (m): a fire's light lifted into its
// flame, a classic light's reach half again its entity's.
void light_sphere(const PointLight& light, bool bonfires, double out[4]);

// The clusters each of a scene's lights' reach touches (a sorted list each; [-1]: always seen),
// worked out once a map, its fire lifted either way (bsp_flames.hpp's bonfires).
struct LightReach {
  std::vector<std::vector<int32_t>> clusters;  // by the light's index in the list given
};
LightReach light_reach(const Map& map, std::span<const PointLight> lights);

struct PickRules {
  int most = 512;          // the lights a frame draws at most
  int shadowed = 48;       // the shadowed few at most
  bool overlay = false;    // the lightmaps' figures' shadows: the nearest kOverlay lights alone
  int overlay_most = 16;   // their count
  bool bonfires = false;   // a fire's light as a bonfire's (bsp_flames.hpp)
  double widest = 2.4;     // the widest screen's aspect a window has
};

struct LightChoice {
  std::vector<LightOffer> drawn;  // the frame's lights, in the order the GPU gets them
  std::vector<size_t> casters;    // indices into `drawn`, the shadowed few by priority
};

// Chooses the frame's lights from `offers`; `reach` (or null: no visibility test) holds the
// clusters of the first `reach->clusters.size()` offers' lights (the map's own, offered first).
void choose_lights(const Map& map, std::span<const LightOffer> offers, const LightReach* reach,
                   const r::FrameView& view, const PickRules& rules, LightChoice& out);

// THE CUBE SLOTS kept from frame to frame: each slot's light (its sphere as the GPU sees it), when
// it was last read, and each light's fade (0 dark, 1 whole) by its key.
struct CubeSlots {
  int slots = 48;
  uint64_t frame = 0;
  std::array<std::array<float, 4>, kMaxCubeSlots> key{};
  std::array<bool, kMaxCubeSlots> held{};
  std::array<uint64_t, kMaxCubeSlots> used{};
  std::array<float, kMaxCubeSlots> fade{};  // the slot's light's share shown, 0..1
};

struct CubeDeal {
  std::vector<int> slot;     // by caster: its slot, or -1 (no cube yet: it lights nothing)
  std::vector<float> fade;   // by caster: its share shown this frame
  std::vector<int> fresh;    // the slots whose cubes this frame draws
  int waiting = 0;           // casters with no cube yet
};

// Deals the slots to `keys` (the casters' spheres, by priority): a light keeps the slot holding
// its cube, the rest take the slot unused longest, at most `budget` new ones. Each light's fade
// moves toward whole by `dt_s / fade_s` once its cube is in (0 fade_s: at once).
void deal_cubes(CubeSlots& slots, std::span<const std::array<float, 4>> keys, size_t budget,
                double dt_s, double fade_s, CubeDeal& out);

}  // namespace ge::brush::app
