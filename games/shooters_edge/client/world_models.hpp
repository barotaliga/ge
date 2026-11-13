// THE THINGS IN THE WORLD AS THE WINDOW DRAWS THEM: every model the server precached (the items,
// the armour, the health and ammo boxes, the weapons lying about, the torches' flames, the other
// players, the monsters, the missiles) loaded once a map from the packs, then drawn each frame
// where the server's entities and its static entities stand.
//
// - LOADED (`read_models` on any thread, then `create_models` on the GPU, while the world's frame
//   is made): each name on the server's model
//   list read by the game's adapter (game_adapter.hpp's `open_model`) into its form: a
//   vertex-animated model as one morph model a skin, a sprite as sprite frames, a brush item (a
//   health box, an ammo crate) as one morph model of its faces (app/classic_models.hpp). A skin
//   the game's files hold a picture for is drawn from that picture, its transparent texels holes
//   (render/morph.hpp's `kMarkHole`); the rest are the enhanced cache's upscaled copies when
//   `cache` names one and the atlas holds them all (else the pack's own, every one). The map's own
//   brush models
//   ("*1", ...) are the map's, not these. A torch's flame (`flame.mdl`, `flame2.mdl`) is also
//   loaded bare, its stick and cage without the painted flame, for the frames whose effects burn
//   the flame instead.
// - PLACED (`place_models`, each frame from the server): every live entity with a model but the
//   local player (whose eyes the camera is), then the static ones; its frame, skin, angles and
//   colours as the server holds them.
// - DRAWN (`prepare_models`, `encode_models`): a group of frames plays at its intervals (flowing
//   between them with `Placed::lerp`), an animation drawn between its last two frames
//   (`Placed::frame_from`, model_lerp.hpp: the GPU blends the two frames' vertices); a model
//   flagged to spin turns; each one lit by the lightmap on the floor under it (taken again only
//   when it moves), shaded from above, the frame's dynamic lights (a muzzle's flash, a rocket's
//   glow, an explosion) added from their side as they are to the floor, a fullbright texel glowing;
//   a player's shirt and pants in his own colours. The nearest `kMaxMorphDraws` are drawn. One
//   short of whole (`Placed::warp`, a spawn or a teleport) is drawn warped and holds still for TAA.
//
// ge:summary The window's models: items, weapons, flames, players, loaded from the packs and drawn
// ge:keywords world-models create_models read_models place_models prepare_models encode_models
// ge:keywords placed-model effect-models s_explod bolt effect_model
// ge:keywords items armour health weapons flames torches players statics entity-models morph
// ge:keywords sprites bare-flames topcolor bottomcolor place_brushes doors plats
// ge:keywords ce_models enhanced-skins cached_skins replacement-skin skin-stand-in
// ge:keywords body_triangles shapes warp-ash dynamic-lights model-lighting
// ge:skill render shooter morph
#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
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
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/listen.hpp"

namespace shooters_edge::client {

struct Burning;  // client/burning.hpp: each body's burn, drawn on its skin

// One entity as the server holds it, map units and degrees.
struct Placed {
  int entity = 0;  // a static: -1 - its index
  int modelindex = 0;
  int frame = 0, skin = 0;
  int carried = 0;  // a second model drawn with it on its frame (a carried weapon); 0 none
  // Its animation drawn between frames (model_lerp.hpp): `frame_from` the frame group it is
  // leaving (-1: none) and `frame_f` the share of the way to `frame` (1: there); `lerp` flows a
  // looping group (a torch's flame) between its own frames too. Off: each frame as the server
  // holds it.
  int frame_from = -1;
  float frame_f = 1;
  bool lerp = false;
  int effects = 0;     // the entity's effect bits (kGlow...)
  bool turns = false;  // it turns about its yaw on the picture's clock (the rules' world says so)
  float alpha = 1;     // how much of it shows, 0..1 (a mod's or the map's `alpha`; 1 solid)
  float scale = 1;     // its size about the origin (a mod's `scale`)
  float colormod[3] = {1, 1, 1};  // its light tinted, glowing texels kept (a mod's `colormod`)
  sh::Vec3f origin, angles;
  uint32_t top = models::kShirtRow, bottom = models::kPantsRow;  // palette rows for the tints
  // A player's `skin` (its user info's: the picture `skins/<name>.pcx` its figure wears, chosen
  // by `player_skin_morph`); empty: none named.
  std::string skin_name;
  // Its warp (world_fx.hpp, warps.hpp): short of whole an alias model is drawn dissolving to ash
  // or out of it (render/morph.hpp's MorphWarp).
  ge::render::MorphWarp warp;
};

// A player's box, map units above and below his origin: the warp's dissolve spans it.
inline constexpr float kBoxLowUnits = -24, kBoxHighUnits = 32;

// THE EFFECTS' OWN MODELS: what the classic client draws for a temporary entity, loaded by the
// window after the server's list whether the program precached them or not (a pack without one
// draws nothing in its place, silently): the explosion's sprite and the three bolts' segments. A
// placed one's modelindex is the server's list's size plus its slot here (`effect_model`).
enum class EffectModel : uint8_t { explosion, bolt, bolt2, bolt3 };
inline constexpr const char* kEffectModels[] = {"progs/s_explod.spr", "progs/bolt.mdl",
                                                "progs/bolt2.mdl", "progs/bolt3.mdl"};
inline int effect_model(const Listen& l, EffectModel m) {
  return (l.server ? static_cast<int>(l.server->models.size()) : 0) + static_cast<int>(m);
}

// The effect bits a power held lights its holder with (the mod's EF_DIMLIGHT, EF_BLUE, EF_RED).
inline constexpr int kGlow = 8, kGlowBlue = 64, kGlowRed = 128;

// A carried model's entity number: its holder's plus this, so its last pose is its own.
inline constexpr int kCarriedEntity = 1 << 20;

// Every entity to draw this frame (not the local player); a carried model right after its holder.
void place_models(const Listen& l, std::vector<Placed>& out);

struct ModelLoad {
  std::span<const std::string> names;  // the server's model list
  // The enhanced cache (`ce_models`: classicenhance/cached.hpp's `find_cache`), or empty: a
  // model's skins drawn from its enhanced copies when they are all there and the atlas holds them.
  std::filesystem::path cache;
};

// One server model as drawn.
struct ModelKind {
  enum class Is : uint8_t { none, alias, sprite, brush } is = Is::none;
  uint32_t first = 0;  // its first morph model (a skin each) or sprite frame
  static constexpr uint32_t kNone = ~0u;
  uint32_t bare = kNone;  // a flame's first morph model without its painted flame
  // The engine's effect drawn in its place (classicenhance/replace.hpp's table: "explosion",
  // "flare", "bubbles", "fire.<kind>"; empty: none), and for a flare or bubble sprite the made
  // frame drawn instead (render/sprite_shapes.hpp).
  std::string_view effect;
  uint32_t made = kNone;
  models::Groups frames, skins;
  bool spin = false, synced = true;
  uint32_t flags = 0;  // a model file's effect flags (effects.hpp's kTrailGib, ...)
  // The colour its pack skin's glowing texels paint (linear, brightest channel 1) and their share
  // of the skin (0: nothing glows): what a grenade's red ends light round it (world_fx).
  float glow_rgb[3] = {0, 0, 0};
  float glow_share = 0;
  ge::render::SpriteFacing facing = ge::render::SpriteFacing::parallel;
  bool player = false;  // the players' own figure (kPlayerModel): it wears each one's skin
};

// THE PLAYERS' SKINS: the figure every player is drawn as, and the most pictures under `skins/`
// read with a map (each a skin-only morph model, kept while the atlas holds them).
inline constexpr std::string_view kPlayerModel = "progs/player.mdl";
inline constexpr size_t kMaxPlayerSkins = 32;
using models::SkinChoice;

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
  // Each morph model's frames and triangles kept on the CPU (no skin): where a warp's ash lands.
  std::vector<ge::render::MorphModel> shapes;
  std::vector<ge::render::MorphDraw> morph_draws;
  std::vector<ge::render::SpriteDraw> sprite_draws;
  std::vector<std::pair<double, size_t>> near;  // scratch
  // The skins read (skin_key -> its skin-only morph model) and the player's choice this frame.
  std::unordered_map<std::string, uint32_t> player_skins;
  SkinChoice skin_choice;
};

// The models read and their skins decoded, on the CPU (any thread: it reads the files and the
// cache, no GPU), for `create_models` to upload.
struct ModelsRead {
  std::vector<ModelKind> kinds;
  std::vector<uint32_t> brush_of;
  uint32_t brushes = 0;
  std::vector<ge::render::MorphModel> morphs;
  std::vector<ge::render::SpriteFrame> frames;
  ge::render::TintRamps ramps;
  models::LightProbe probe;
  std::unordered_map<std::string, uint32_t> player_skins;
};
// Reads `load.names` through `game`'s adapter and `palette`; a file missing or refused is
// reported once and drawn as nothing.
ModelsRead read_models(const SearchPath& files, std::span<const uint8_t> palette,
                       const ge::brush::Map& map, const ModelLoad& load,
                       const GameAdapter& game = classic_adapter());
// The models read uploaded; the passes join `pipelines` (finished by the caller).
void create_models(ge::gpu::Device& device, ge::gpu::PipelineSet& pipelines, WorldModels& m,
                   const wgpu::BindGroupLayout& frame, ModelsRead read);
// Where the map's own brush models stand this frame (doors, plats, buttons): each one an entity
// of `placed` holds offset by that entity's origin; one no entity holds (removed by the game) is
// hidden. Empty when the models were loaded with no server's list.
void place_brushes(const WorldModels& m, std::span<const Placed> placed,
                   std::vector<ge::brush::app::Mover>& out);
// This frame's draws of `placed` at `view` (its time the animations' clock), lit `brightness`
// times the lightmap under each and never under `minlight` (the world's own: ba::Options's
// brightness and minlight), `smooth` texels; with `in_place` the pack's effects the engine draws
// instead: a torch's flame drawn bare (the modern look's burning gas, `ce_flames`, is the flame),
// a flare a soft added glow (`ce_flares`), a bubble a clear ball with a bright rim that wobbles as
// it rises (`ce_bubbles`); of the bubbles the game blows only `bubbles_share` is drawn, the pack's
// sprites and the made ones alike (`ce_bubbles_share`).
struct InPlace {
  // explosions: the engine's blast is drawn where the game shows the pack's explosion sprite (an
  // entity the game's own logic turns into one as its shot bursts), so the sprite is left out.
  bool flames = false, flares = false, bubbles = false, explosions = false;
  double bubbles_share = 1;
};
// `dynamic` is the frame's dynamic lights (a muzzle's flash, a rocket's glow, an explosion, a
// power's glow: what bsp_draw adds to the walls' lightmaps), at `dynamic_strength`: each adds to
// every model in its reach as it adds to the floor under it (effects.hpp's `dynamic_light_at`),
// from its side.
void prepare_models(ge::gpu::Device& device, WorldModels& m, const ge::brush::Map& map,
                    std::span<const Placed> placed, const ge::render::FrameView& view,
                    double brightness, double minlight, bool smooth, const InPlace& in_place,
                    std::span<const ge::brush::app::DynamicLight> dynamic = {},
                    double dynamic_strength = 1, const Burning* burning = nullptr);

// The triangles of `p`'s model as `prepare_models` draws it at `time_s` (its frame and skin
// group): nine floats a triangle, its model's own frame (m). False for a sprite, a brush or none.
bool body_triangles(const WorldModels& m, const Placed& p, double time_s, std::vector<float>& out);
// The tip of what `p` holds out at `time_s`: of its model's points as drawn, the one furthest along
// `forward` (world, unit), into `tip` (world, m). A monster's gun barrel, as its frame moves it.
// False for a sprite, a brush or none.
bool held_tip(const WorldModels& m, const Placed& p, double time_s, const double forward[3],
              double tip[3]);

// THE MOVING CASTERS (the modern look's real-time lights, M54): the alias models of `placed`
// within kCasterReachM of the eye (players, monsters, items; not a torch's flame, which is light),
// and `self` when given (the first-person player, never drawn, still casting), posed at `time_s`
// as prepare_models poses them, into `out` as world triangles (m, nine floats a triangle) for the
// BSP renderer's caster cubes (bsp_draw.hpp's set_shadow_casters).
inline constexpr double kCasterReachM = 24;
void shadow_casters(const WorldModels& m, std::span<const Placed> placed, const Placed* self,
                    const ge::render::FrameView& view, std::vector<float>& out,
                    std::vector<ge::brush::app::CasterBox>& boxes);

// The models after the opaque world; the sprites where the particles are drawn.
void encode_models(const WorldModels& m, const ge::gpu::PipelineSet& pipelines,
                   ge::render::FrameRecorder& rec, wgpu::CommandEncoder& encoder,
                   const ge::render::FrameTargets& targets, const wgpu::BindGroup& frame);
void encode_model_sprites(const WorldModels& m, const ge::gpu::PipelineSet& pipelines,
                          ge::render::FrameRecorder& rec, wgpu::CommandEncoder& encoder,
                          const ge::render::FrameTargets& targets, const wgpu::BindGroup& frame);

}  // namespace shooters_edge::client
