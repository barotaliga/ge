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
// ge:keywords body_triangles shapes warp-ash dynamic-lights model-lighting filtered_models
// ge:keywords player-figures choose_look worn_slot drawn_kind carried-weapons skin-files
// ge:keywords place_returning hologram_of returning-items item-hologram ce_respawns
// ge:skill render shooter morph
#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "brush/app/bsp_draw.hpp"
#include "brush/map.hpp"
#include "classicenhance/hologram.hpp"
#include "gpu/device.hpp"
#include "render/frame.hpp"
#include "render/morph.hpp"
#include "render/sprites.hpp"
#include "shooters_edge/app/classic_models.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/model_filters.hpp"

namespace shooters_edge::client {

struct Burning;  // client/burning.hpp: each body's burn, drawn on its skin

// One entity as the server holds it, map units and degrees.
struct Placed {
  int entity = 0;  // a static: -1 - its index
  int modelindex = 0;
  int frame = 0, skin = 0;
  int carried = 0;  // a second model drawn with it on its frame (a carried weapon); 0 none
  // Quake's visible weapon (plugins/classic/vweps.hpp): the server's index into its list
  // of the weapon this player holds (0 none), and `vwep_bare`: drawn without his own gun even with
  // none (a server recording's body on a death frame). `filtered_models` swaps the body and adds
  // the weapon (WorldModels::vweps).
  int vwep = 0;
  bool vwep_bare = false;
  // Its animation drawn between frames (model_lerp.hpp): `frame_from` the frame group it is
  // leaving (-1: none) and `frame_f` the share of the way to `frame` (1: there); `lerp` flows a
  // looping group (a torch's flame) between its own frames too. Off: each frame as the server
  // holds it.
  int frame_from = -1;
  float frame_f = 1;
  bool lerp = false;
  int effects = 0;  // the entity's effect bits (kGlow...)
  // Quake II's look (q2_look.hpp): its light pulsing (an item's glow) and a coloured shell
  // over it (quad damage, invulnerability, a power shield's hit), `shell_rgb` its flat colour.
  bool pulse = false, shell = false;
  bool bleeds = false;       // a body's piece by its bits (Quake II's gib bit), not its model file
  bool green_blood = false;  // its blood green (Quake II's green gib bit)
  bool flies = false;        // Quake II's corpse with flies round it (its flies bit)
  bool shell_only = false;   // its shell alone, no plain draw under it (a power screen's plate)
  // A Quake II entity's animation bits (its effects' ANIM01, ANIM23, ANIM_ALL, ANIM_ALLFAST;
  // q2_look.hpp's q2_texture_frame): a brush model's texture frame, a model's or a sprite's own
  // frame, cycled on the picture's clock; -1 elsewhere, its textures on the map's clock.
  int q2_anim = -1;
  float shell_rgb[3] = {0, 0, 0};
  bool turns = false;  // it turns about its yaw on the picture's clock (the rules' world says so)
  bool spinning = false;  // Quake II's spinning lights: upside down, 500 degrees a second
  bool bobs = false;      // Quake II's bob bit: rising and sinking 5 units
  // Quake II's render bits: lit full whatever the room, or never darker than a tenth.
  bool full_bright = false, min_light = false;
  bool no_shadow = false;  // Quake II's render bit: it casts no ground shadow (model_shadow.hpp)
  // Seen through infra-red goggles: its skin times pure red, no light of the room, no glow.
  bool infra_red = false;
  float alpha = 1;  // how much of it shows, 0..1 (a mod's or the map's `alpha`; 1 solid)
  float scale = 1;  // its size about the origin (a mod's `scale`)
  float colormod[3] = {1, 1, 1};  // its light tinted, glowing texels kept (a mod's `colormod`)
  // An item coming back as a hologram (ce_respawns, classicenhance/hologram.hpp): the share of its
  // light that is the hologram's blue, and the blue shell over it (0: none).
  float holo_blue = 0, holo_shell = 0;
  sh::Vec3f origin, angles;
  uint32_t top = models::kShirtRow, bottom = models::kPantsRow;  // palette rows for the tints
  // A player's `skin` (its user info's: the picture `skins/<name>.pcx` its figure wears, chosen
  // by `player_skin_morph`); empty: none named.
  std::string skin_name;
  // A player's `model` (its user info's: the figure in `players/<model>/` it is drawn as, over the
  // game's own); empty: none named.
  std::string model_name;
  // Its side as the viewer sees it (teamskin, enemycolor: `models::side_of`); none: not a player.
  models::Side side = models::Side::none;
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
// `loc_draw`'s marker is a head: Quake's the player's, Quake II's the ancient head (each game's
// pack has only its own, so the other draws nothing).
enum class EffectModel : uint8_t { explosion, bolt, bolt2, bolt3, place_q1, place_q2 };
inline constexpr const char* kEffectModels[] = {
    "progs/s_explod.spr", "progs/bolt.mdl",     "progs/bolt2.mdl",
    "progs/bolt3.mdl",    "progs/h_player.mdl", "models/items/c_head/tris.md2"};
inline int effect_model(const Listen& l, EffectModel m) {
  return (l.server ? static_cast<int>(l.server->models.size()) : 0) + static_cast<int>(m);
}

// The effect bits a power held lights its holder with (the mod's EF_DIMLIGHT, EF_BLUE, EF_RED).
inline constexpr int kGlow = 8, kGlowBlue = 64, kGlowRed = 128;

// A carried model's entity number: its holder's plus this, so its last pose is its own.
inline constexpr int kCarriedEntity = 1 << 20;
inline constexpr int kPlateEntity = 1 << 21;  // a power screen's plate: its bearer + this

// Every entity to draw this frame (not the local player); a carried model right after its holder.
void place_models(const Listen& l, std::vector<Placed>& out);

// An item taken and coming back (the rules' world's `back_in`): where it will lie, as it will show,
// and the seconds until it does on the server's clock.
struct Returning {
  Placed item;
  double in_s = 0;
};
// Every item coming back within `within_s` (none in a game whose rules cannot say).
void place_returning(const Listen& l, double within_s, std::vector<Returning>& out);
// `item` drawn as the hologram `h` (its see-through share, blue light and shell).
Placed hologram_of(const Placed& item, const ge::classicenhance::HologramLook& h);

struct ModelLoad {
  std::span<const std::string> names;  // the server's model list
  // The enhanced cache (`ce_models`: classicenhance/cached.hpp's `find_cache`), or empty: a
  // model's skins drawn from its enhanced copies when they are all there and the atlas holds them.
  std::filesystem::path cache;
  // The visible weapons' list as the server named it (`//vwep`, classic/vweps.hpp), entry 0 the
  // body; empty: none. Loaded after the effects' models, a file the pack lacks made from it.
  std::span<const std::string> vweps;
};

// THE VISIBLE WEAPONS LOADED (classic/vweps.hpp): entry `i` of the server's list is model kind
// `first + i`; `on` when the body loaded (or the list draws none, `-`), else the feature is off
// for the level. `draw` is r_drawvweps this frame (Quake II's cl_vwep: 0 draws each figure's
// own plain weapon in place of the one carried).
struct Vweps {
  uint32_t first = 0, count = 0;
  uint32_t dashes = 0;  // bit i: entry i is `-` (a weapon of none: the body alone)
  bool on = false, bodiless = false;
  bool draw = true;
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
  // Quake II's player figure (`players/<folder>/tris.md2`) or a weapon carried on it
  // (`#<file>`, its file in the figure's folder, `carried`): drawn in each player's own look.
  bool figure = false;
  std::vector<bool> resting;  // a player's figure: each frame a body lies still on (a death's last)
  // A second-game figure's folder's plain `weapon.md2` (an index into the figure kinds): what it
  // carries under cl_vwep 0; kNone: none.
  uint32_t plain = kNone;
  std::string carried;
  // Drawn for Quake's players (a `model` naming its folder): the frame of its own drawn
  // for each frame of the game's figure, by the frames' names (app/figure_frames.hpp); empty: the
  // same frame.
  std::vector<int> frame_map;
  uint32_t worn = ~0u;  // a weapon of a figure's folder: the skin-only model its picture is in
  // A Quake player's figure from another game: its folder's `weapon.md2` (an index into the
  // figure kinds, in its own picture), drawn in its hand on the same frame; kNone: none.
  uint32_t held = kNone;
};

// THE PLAYERS' SKINS: the figure every player is drawn as, and the most pictures under `skins/`
// read with a map (each a skin-only morph model, kept while the atlas holds them).
inline constexpr std::string_view kPlayerModel = "progs/player.mdl";
// Quake II's figure's plain weapon (in its folder): what it carries under cl_vwep 0.
inline constexpr std::string_view kPlainWeapon = "weapon.md2";
inline constexpr size_t kMaxPlayerSkins = 32;
// Quake II's figures read with a map (every folder under `players/` with a figure), and the
// skins in them.
inline constexpr size_t kMaxFigures = 8;
// The skins worn at once (render/morph.hpp's slots): one read the first time it is worn, kept
// until a slot is wanted for another and it was worn longest ago.
inline constexpr size_t kSkinSlots = 32;

// A skin picture listed with the map: its key, read whole (Quake II's) or cut as Quake's
// game's, and its files in the order tried (.tga, .png, .pcx).
struct SkinFile {
  std::string key;
  bool whole = false;
  std::vector<std::string> paths;
};

// What a draw wears: a model's skin (render's kOwnSkin: its own) or a listed skin (`file`).
struct Worn {
  static constexpr uint32_t kNoFile = ~0u;
  uint32_t model = ge::render::kOwnSkin;
  uint32_t file = kNoFile;
};
using models::SkinChoice;

struct WorldModels {
  std::vector<ModelKind> kinds;    // by modelindex
  std::vector<uint32_t> brush_of;  // by modelindex: the map's own brush model ("*N": N), or 0
  uint32_t brushes = 0;            // the map's brush models, the world's [0] among them
  ge::render::Morph morph;
  ge::render::Sprites sprites;
  bool has_morph = false, has_sprites = false;
  // The glow disc's made frame (gl_flashblend), ModelKind::kNone when there are no sprites, and the
  // screen's orange tint this frame from an eye inside one of the moment's lights (0..1).
  uint32_t glow_disc = ModelKind::kNone;
  double flash_tint = 0;
  models::LightProbe probe;
  struct Lit {
    sh::Vec3f at;
    float rgb[3] = {1, 1, 1};
    bool floor_found = false;  // the floor under it (its height and slope) for its shadow
    models::FloorUnder floor;
  };
  std::unordered_map<int, Lit> lit;                          // by entity
  std::unordered_map<int, ge::render::MorphPose> last_pose;  // by entity
  // Each morph model's frames and triangles kept on the CPU (no skin): where a warp's ash lands.
  std::vector<ge::render::MorphModel> shapes;
  std::unordered_map<uint32_t, float> radius;  // by morph model: its frame 0's radius, m (shadows)
  std::vector<ge::render::MorphDraw> morph_draws;
  std::vector<ge::render::SpriteDraw> sprite_draws;
  std::vector<std::pair<double, size_t>> near;  // scratch
  // The skins listed (skin_key -> its `skin_files` row) and the player's choice this frame.
  std::unordered_map<std::string, uint32_t> player_skins;
  SkinChoice skin_choice;
  Vweps vweps;
  // r_fullbrightskins this frame: how far a player (Quake's figure, a figure from a
  // folder) is lifted toward full light, unshaded at 1 (0..1).
  double fullbright = 0;
  // Quake II's figures (folder -> its kind in `figure_kinds`) and the weapons carried on
  // each ("<folder>/<file>" -> its kind there).
  std::unordered_map<std::string, uint32_t> figures, figure_carried;
  // Each figure folder's first picture listed (folder -> its `skin_files` row): Quake's
  // player whose skin the folder lacks wears it.
  std::unordered_map<std::string, uint32_t> figure_first_skins;
  std::vector<ModelKind> figure_kinds;
  // The skins listed (`player_skins` keys them) and where they are read from, and the slots.
  std::vector<SkinFile> skin_files;
  const SearchPath* files = nullptr;
  std::vector<uint8_t> palette;
  struct Slots {
    std::vector<uint32_t> of_file, file_in;  // a file's slot; a slot's file
    std::vector<uint64_t> used;              // the frame each slot was last worn
    std::vector<bool> failed;                // a file that read in no form
    uint64_t frame = 0;
  } slots;
};

// The kind `p` is drawn as, and what it wears: a player in their own skin, a second-game player as
// their own figure, the weapon they carry from its folder.
const ModelKind& drawn_kind(const WorldModels& m, const Placed& p, Worn* worn);
// The slot listed skin `file` is in, read into the slot worn longest ago the first time; kNoSlot
// when it does not read or every slot is worn this frame.
uint32_t worn_slot(ge::gpu::Device& d, WorldModels& m, uint32_t file);

// The models read and their skins decoded, on the CPU (any thread: it reads the files and the
// cache, no GPU), for `create_models` to upload.
struct ModelsRead {
  std::vector<ModelKind> kinds;
  std::vector<uint32_t> brush_of;
  uint32_t brushes = 0;
  std::vector<ge::render::MorphModel> morphs;
  std::vector<ge::render::SpriteFrame> frames;
  uint32_t glow_disc = ModelKind::kNone;  // the glow disc's frame among them (gl_flashblend)
  ge::render::TintRamps ramps;
  models::LightProbe probe;
  std::unordered_map<std::string, uint32_t> player_skins;  // skin key -> its `skin_files` row
  std::unordered_map<std::string, uint32_t> figures, figure_carried;
  // Each figure folder's first picture listed (folder -> its `skin_files` row): Quake's
  // player whose skin the folder lacks wears it.
  std::unordered_map<std::string, uint32_t> figure_first_skins;
  std::vector<ModelKind> figure_kinds;
  std::vector<std::string> player_frames;  // the game's figure's frames' names, in order
  Vweps vweps;
  std::vector<SkinFile> skin_files;
  const SearchPath* files = nullptr;  // where the skins are read from (it outlives the models)
  std::vector<uint8_t> palette;
};
// Reads `load.names` through `game`'s adapter and `palette`; a file missing or refused is
// reported once and drawn as nothing.
ModelsRead read_models(const SearchPath& files, std::span<const uint8_t> palette,
                       const ge::brush::Map& map, const ModelLoad& load,
                       const GameAdapter& game = qw_adapter());
// The models read uploaded; the passes join `pipelines` (finished by the caller).
void create_models(ge::gpu::Device& device, ge::gpu::PipelineSet& pipelines, WorldModels& m,
                   const wgpu::BindGroupLayout& frame, ModelsRead read);
// Where the map's own brush models stand this frame (doors, plats, buttons): each one an entity
// of `placed` holds offset by that entity's origin; one no entity holds (removed by the game) is
// hidden. Empty when the models were loaded with no server's list.
void place_brushes(const WorldModels& m, std::span<const Placed> placed, double t,
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
  // r_powershells and r_itempulse (q2_look.hpp's power_look): a power's coloured shell over
  // its carrier and an item's pulsing light, in either game.
  bool power_shells = false, item_pulse = false;
  // cl_nobob: an item the game marks to float (Quake II's bob bit) drawn still.
  bool items_still = false;
  // Quake II's model light (classic/q2_fx_switches.hpp; the look's `model_light`): every model's
  // light times `light`; `flat` (gl_dotshading 0) lit evenly, no side brighter; `roll_hack`
  // (cl_rollhack) every other player's figure (not `self`, the player's own) leaning the other way.
  double light = 1;
  bool flat = false, roll_hack = false;
  int self = 0;
  // The models' ground shadows (model_shadow.hpp's mode: 0 none, 1 under each, 2 fading).
  int shadows = 0;
  // gl_flashblend: each of the moment's lights drawn as a glow disc (classic/flash_blend.hpp).
  bool flash_blend = false;
  // gl_celshading: an ink line this many pixels wide round each solid model (thinner and fainter
  // with distance, classic/q2_fx_switches.hpp's q2_outline_px), on a view `view_px` high; 0 none.
  double cel_shading = 0, view_px = 0;
};

// `dynamic` is the frame's dynamic lights (a muzzle's flash, a rocket's glow, an explosion, a
// power's glow: what bsp_draw adds to the walls' lightmaps), at `dynamic_strength`: each adds to
// every model in its reach as it adds to the floor under it (effects.hpp's `dynamic_light_at`),
// from its side.
void prepare_models(ge::gpu::Device& device, WorldModels& m, const ge::brush::Map& map,
                    std::span<const Placed> placed, const ge::render::FrameView& view,
                    double brightness, double minlight, bool smooth, const InPlace& in_place,
                    std::span<const ge::brush::app::DynamicLight> dynamic = {},
                    double dynamic_strength = 1, const Burning* burning = nullptr,
                    std::span<const std::pair<int, ge::render::MorphBlood>> bloodied = {});

// The triangles of `p`'s model as `prepare_models` draws it at `time_s` (its frame and skin
// group): nine floats a triangle, its model's own frame (m). False for a sprite, a brush or none.
bool body_triangles(const WorldModels& m, const Placed& p, double time_s, std::vector<float>& out);
// The point `a`, `b` (barycentric from its first corner) of triangle `tri` of `p`'s model as it
// stands in its REST frame (frame 0), times its scale (m): where render/morph.hpp's MorphWound
// sits.
bool body_rest(const WorldModels& m, const Placed& p, double time_s, uint32_t tri, float a, float b,
               float out[3]);
// The tip of what `p` holds out at `time_s`: of its model's points as drawn, the one furthest along
// `forward` (world, unit), into `tip` (world, m). A monster's gun barrel, as its frame moves it.
// False for a sprite, a brush or none.
bool held_tip(const WorldModels& m, const Placed& p, double time_s, const double forward[3],
              double tip[3]);

// The models a frame draws: `placed` itself when `f` hides nothing and no player carries a
// visible weapon (no copy), else the ones it does not hide (model_filters.hpp: gibs, still bodies)
// copied into `kept` (reused), each player with a visible weapon (`Placed::vwep`, while
// `m.vweps` is on and draws) drawn as the list's body with the weapon right after him.
std::span<const Placed> filtered_models(const WorldModels& m, const ModelFilters& f,
                                        std::span<const Placed> placed, std::vector<Placed>& kept);

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
