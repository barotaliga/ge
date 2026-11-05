// THE WORLD AS THE WINDOW DRAWS IT: one map's frame on the GPU from the player's eye, then the
// 2D overlay (the HUD, the console) over it, into the window's surface. The map is drawn by the
// brush plugin's BSP renderer (brush/app/bsp_draw.hpp) over the engine's sky, the game's effects
// (world_fx.hpp: the particles, their dynamic lights on the walls, the screen's blend) in it, with
// bloom and the post pass, in the game's look (app/look.hpp: `r_look` classic or modern and the
// picture's `r_` cvars: the look and the effects' strength read every frame, the post's when the
// map loads), and the models standing in it (world_models.hpp: the items, the flames, the other
// players). The classic look grades the picture to the packs' palette. Over it all, the weapon
// in hand (view/hand.hpp's frame, the game's free models) on the engine's view-model pass, lit by
// the map's baked light on the floor under the player (brush/light_probe.hpp) and by its own
// muzzle flash. The built-in arena has no drawn world: the sky, the effects and the weapon alone.
//
// It is the client's one seam onto the renderer: `create_world_draw` and `draw_world`. A window
// that keeps drawing while a map loads makes it in three steps instead: `read_world` on any
// thread (the map's textures decoded, its rebaked light and its models read, the enhanced cache
// found: the seconds of it), `begin_world` on the GPU's thread (the uploads, the pipelines
// started), then `finish_world` once `world_compiled` says the pipelines are done.
//
// ge:summary The window's world frame: the BSP map, the effects, sky, bloom, post, the overlay
// ge:keywords world-draw bsp render frame sky bloom post overlay ui-pass r_look classic modern
// ge:keywords create_world_draw draw_world resize_world palette particles dynamic-lights blend
// ge:keywords models items flames read_world begin_world world_compiled finish_world own_sky
// ge:keywords background-load async-load title
// ge:keywords viewmodel held-weapon light-probe
// ge:skill render brush shooter
#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>

#include "gpu/device.hpp"
#include "render/view.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/timedemo.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/client/world_models.hpp"
#include "shooters_edge/view/hand.hpp"
#include "ui/draw.hpp"
#include "viewmodel/held.hpp"

namespace shooters_edge::client {

struct WorldDraw;
struct WorldDrawDeleter {
  void operator()(WorldDraw* w) const;
};
using WorldDrawPtr = std::unique_ptr<WorldDraw, WorldDrawDeleter>;

// The frame for `map` (its textures through the search path's palette, `gfx/palette.lmp`) at
// width x height into a surface of `format`, drawn in `picture`'s look (read every frame: it must
// outlive the frame), with `weapons` (view/weapons.hpp's load_weapon_models, in `view::Weapon`
// order; empty: no weapon is drawn) and the models on `models.names` (the server's list:
// world_models.hpp) standing in it; the map's textures come from the game's adapter
// (game_adapter.hpp's `map_textures`). Null with the reason in `error`.
WorldDrawPtr create_world_draw(ge::gpu::Device& device, const SearchPath& files,
                               const LoadedMap& map, uint32_t width, uint32_t height,
                               wgpu::TextureFormat format, const look::Picture& picture,
                               std::span<const ge::viewmodel::HeldModel> weapons,
                               std::string& error, const ModelLoad& models = {},
                               const GameAdapter& game = classic_adapter());

// The world's CPU half, read on any thread: `picture` is copied (the world is made in it), and
// `models.names` must outlive the call.
struct WorldRead;
struct WorldReadDeleter {
  void operator()(WorldRead* r) const;
};
using WorldReadPtr = std::unique_ptr<WorldRead, WorldReadDeleter>;
WorldReadPtr read_world(const SearchPath& files, const LoadedMap& map, const look::Picture& picture,
                        const ModelLoad& models, std::string& error,
                        const GameAdapter& game = classic_adapter());
// Its GPU half on the device's thread: the uploads made and the pipelines started; drawn once
// `finish_world` is done (`world_compiled` true, or it waits). Null with the reason in `error`.
WorldDrawPtr begin_world(ge::gpu::Device& device, WorldReadPtr read, uint32_t width,
                         uint32_t height, wgpu::TextureFormat format, const look::Picture& picture,
                         std::span<const ge::viewmodel::HeldModel> weapons, std::string& error);
bool world_compiled(ge::gpu::Device& device, WorldDraw& w);
// The pipelines finished and the targets made at the window's size now. False with the reason.
bool finish_world(ge::gpu::Device& device, WorldDraw& w, uint32_t width, uint32_t height,
                  std::string& error);

void resize_world(ge::gpu::Device& device, WorldDraw& w, uint32_t width, uint32_t height);
// The models it draws (world_models.hpp): their shapes, for where a warp's ash lands.
const WorldModels& world_models(const WorldDraw& w);
// With `own` the frame's sky is the view's as the caller dressed it (the title's), not the map's.
void own_sky(WorldDraw& w, bool own);
// Whether a setting read only when the world is made changed since it was made (`ce_textures`,
// `ce_models`, `ce_rebake`, `ce_bloom`, `r_bloom`): the window makes it again (the menu closed).
bool world_stale(const WorldDraw& w);

// One frame of `dt` s from `view` (its eye and look set; the sky is the map's own) with this
// frame's effects `fx` and its models (`fx.models`) and the weapon in hand `held` (null: none),
// the overlay over it.
void draw_world(ge::gpu::Device& device, WorldDraw& w, ge::render::FrameView view,
                const FxFrame& fx, const shooters_edge::view::HandFrame* held, double dt,
                const ge::ui::DrawList& overlay, const wgpu::TextureView& target);

// The last GPU reading of the frame's span (a frame or more late) into `p`; nothing when the GPU
// has no timestamp queries.
void record_span(const WorldDraw& w, PassTimes& p);

}  // namespace shooters_edge::client
