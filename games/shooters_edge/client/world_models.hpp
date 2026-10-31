// THE THINGS IN THE WORLD AS THE WINDOW DRAWS THEM: every model the server precached (the items,
// the armour, the health and ammo boxes, the weapons lying about, the torches' flames, the other
// players, the monsters, the missiles) loaded once a map from the packs, then drawn each frame
// where the server's entities and its static entities stand.
//
// - LOADED (`create_models`, while the world's frame is made): each name on the server's model
//   list through the search path: a `.mdl` as one morph model a skin, a `.spr` as sprite frames,
//   a `.bsp` item (a health box, an ammo crate) as one morph model of its faces
//   (app/classic_models.hpp). The map's own brush models ("*1", ...) are the map's, not these.
//   A torch's flame (`flame.mdl`, `flame2.mdl`) is also loaded bare, its stick and cage without
//   the painted flame, for the frames whose effects burn the flame instead.
// - PLACED (`place_models`, each frame from the server): every live entity with a model but the
//   local player (whose eyes the camera is), then the static ones; its frame, skin, angles and
//   colours as the server holds them.
// - DRAWN (`prepare_models`, `encode_models`): a group of frames plays at its intervals; a model
//   flagged to spin turns; each one lit by the lightmap on the floor under it (taken again only
//   when it moves), shaded from above, a fullbright texel glowing; a player's shirt and pants in
//   his own colours. The nearest `kMaxMorphDraws` are drawn.
//
// ge:summary The window's models: items, weapons, flames, players, loaded from the packs and drawn
// ge:keywords world-models create_models place_models prepare_models encode_models placed-model
// ge:keywords items armour health weapons flames torches players statics entity-models morph
// ge:keywords sprites bare-flames topcolor bottomcolor place_brushes doors plats
// ge:skill render shooter morph
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "brush/app/bsp_draw.hpp"
#include "brush/map.hpp"
#include "gpu/device.hpp"
#include "render/frame.hpp"
#include "render/morph.hpp"
#include "render/sprites.hpp"
#include "shooters_edge/app/classic_models.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"

namespace shooters_edge::client {

// One entity as the server holds it, map units and degrees.
struct Placed {
  int entity = 0;  // a static: -1 - its index
  int modelindex = 0;
  int frame = 0, skin = 0;
  int effects = 0;  // the entity's effect bits (kGlow...)
  sh::Vec3f origin, angles;
  uint32_t top = models::kShirtRow, bottom = models::kPantsRow;  // palette rows for the tints
};

// The effect bits a power held lights its holder with (the mod's EF_DIMLIGHT, EF_BLUE, EF_RED).
inline constexpr int kGlow = 8, kGlowBlue = 64, kGlowRed = 128;

// Every entity to draw this frame (not the local player).
void place_models(const Listen& l, std::vector<Placed>& out);

struct ModelLoad {
  std::span<const std::string> names;  // the server's model list
};

// One server model as drawn.
struct ModelKind {
  enum class Is : uint8_t { none, alias, sprite, brush } is = Is::none;
  uint32_t first = 0;  // its first morph model (a skin each) or sprite frame
  static constexpr uint32_t kNone = ~0u;
  uint32_t bare = kNone;  // a flame's first morph model without its painted flame
  models::Groups frames, skins;
  bool spin = false, synced = true;
  ge::render::SpriteFacing facing = ge::render::SpriteFacing::parallel;
};

struct WorldModels {
  std::vector<ModelKind> kinds;    // by modelindex
  std::vector<uint32_t> brush_of;  // by modelindex: the map's own brush model ("*N": N), or 0
  uint32_t brushes = 0;            // the map's brush models, the world's [0] among them
  ge::render::Morph morph;
  ge::render::Sprites sprites;
  bool has_morph = false, has_sprites = false;
  models::LightProbe probe;
  struct Lit {
    sh::Vec3f at;
    float rgb[3] = {1, 1, 1};
  };
  std::unordered_map<int, Lit> lit;                          // by entity
  std::unordered_map<int, ge::render::MorphPose> last_pose;  // by entity
  std::vector<ge::render::MorphDraw> morph_draws;
  std::vector<ge::render::SpriteDraw> sprite_draws;
  std::vector<std::pair<double, size_t>> near;  // scratch
};

// Loads `load.names` from the search path through `palette`; a file missing or refused is
// reported once and drawn as nothing. The passes join `pipelines` (finished by the caller).
void create_models(ge::gpu::Device& device, ge::gpu::PipelineSet& pipelines, WorldModels& m,
                   const wgpu::BindGroupLayout& frame, const SearchPath& files,
                   std::span<const uint8_t> palette, const ge::brush::Map& map,
                   const ModelLoad& load);
// Where the map's own brush models stand this frame (doors, plats, buttons): each one an entity
// of `placed` holds offset by that entity's origin; one no entity holds (removed by the game) is
// hidden. Empty when the models were loaded with no server's list.
void place_brushes(const WorldModels& m, std::span<const Placed> placed,
                   std::vector<ge::brush::app::Mover>& out);
// This frame's draws of `placed` at `view` (its time the animations' clock), lit `brightness`
// times the lightmap under each and never under `minlight` (the world's own: ba::Options's
// brightness and minlight), `smooth` texels; with `bare_flames` a torch's flame is drawn bare
// (the modern look's burning gas, `ce_flames`, is the flame).
void prepare_models(ge::gpu::Device& device, WorldModels& m, const ge::brush::Map& map,
                    std::span<const Placed> placed, const ge::render::FrameView& view,
                    double brightness, double minlight, bool smooth, bool bare_flames);
// The models after the opaque world; the sprites where the particles are drawn.
void encode_models(const WorldModels& m, const ge::gpu::PipelineSet& pipelines,
                   ge::render::FrameRecorder& rec, wgpu::CommandEncoder& encoder,
                   const ge::render::FrameTargets& targets, const wgpu::BindGroup& frame);
void encode_model_sprites(const WorldModels& m, const ge::gpu::PipelineSet& pipelines,
                          ge::render::FrameRecorder& rec, wgpu::CommandEncoder& encoder,
                          const ge::render::FrameTargets& targets, const wgpu::BindGroup& frame);

}  // namespace shooters_edge::client
