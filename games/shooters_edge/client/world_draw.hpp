// THE WORLD AS THE WINDOW DRAWS IT: one map's frame on the GPU from the player's eye, then the
// 2D overlay (the HUD, the console, the markers) over it, into the window's surface. The map is
// drawn by the brush plugin's BSP renderer (brush/app/bsp_draw.hpp) over the engine's sky, with
// bloom and the post pass; `r_bspmode` picks its CLASSIC look (the map's own textures, lightmaps,
// warped liquids and sky: everything in one pass, the default) or V2 (our sky through the sky
// faces and real-time lights). The built-in arena has no drawn world: the sky alone.
//
// It is the client's one seam onto the renderer: a richer world pass (the effects, the looks) goes
// in behind `create_world_draw` and `draw_world` without the window's loop changing.
//
// ge:summary The window's world frame: the BSP map over the sky, bloom, post, the overlay
// ge:keywords world-draw bsp render frame sky bloom post overlay ui-pass r_bspmode classic v2
// ge:keywords create_world_draw draw_world resize_world palette
// ge:skill render brush shooter
#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "gpu/device.hpp"
#include "render/view.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "ui/draw.hpp"

namespace shooters_edge::client {

struct WorldDraw;
struct WorldDrawDeleter {
  void operator()(WorldDraw* w) const;
};
using WorldDrawPtr = std::unique_ptr<WorldDraw, WorldDrawDeleter>;

struct WorldLook {
  bool v2 = false;  // r_bspmode: v2 (our sky and real-time lights) or the classic look
};

// The frame for `map` (its textures through the search path's palette, `gfx/palette.lmp`) at
// width x height into a surface of `format`. Null with the reason in `error`.
WorldDrawPtr create_world_draw(ge::gpu::Device& device, const SearchPath& files,
                               const LoadedMap& map, uint32_t width, uint32_t height,
                               wgpu::TextureFormat format, const WorldLook& look,
                               std::string& error);
void resize_world(ge::gpu::Device& device, WorldDraw& w, uint32_t width, uint32_t height);

// One frame from `view` (its eye and look set; the sky is the map's own), the overlay over it.
void draw_world(ge::gpu::Device& device, WorldDraw& w, ge::render::FrameView view,
                const ge::ui::DrawList& overlay, const wgpu::TextureView& target);

}  // namespace shooters_edge::client
