// THE CLASSIC MAP EXPERIMENT'S DATA (lab `gpu-classic`): a version 29 map out of a pack file made
// ready to draw on the GPU. Its textures' pixels (every mip the file stores) through the pack's
// palette into one atlas; its lightmaps into another; its surfaces as vertices with each one's
// texture rectangle and kind; its liquids' top surfaces grouped into bodies by kind and level; and
// its light entities as point lights.
//
// ge:summary The classic map experiment's data: atlases, vertices, liquid bodies, point lights
// ge:keywords lab classic bsp pack palette miptex atlas lightmap liquid-body point-light flicker
// ge:keywords surface-atlas relief normal-map height-map roughness
// ge:skill brush labs
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "brush/map.hpp"
#include "brush/pictures.hpp"
#include "brush/rebake.hpp"

namespace ge::brush::app {

// A pack file: a directory of named files over one block of bytes.
struct Pack {
  struct Entry {
    std::string name;
    uint32_t offset = 0, size = 0;
  };
  std::vector<uint8_t> bytes;
  std::vector<Entry> files;
};
bool read_pack(const std::string& path, Pack* out, std::string* error);
// The file `name`'s bytes, empty when the pack has none of that name.
std::span<const uint8_t> pack_file(const Pack& pack, std::string_view name);

// What a surface is drawn as (the vertex's `kind`).
enum Kind : uint32_t {
  kSolid = 0,
  kSky = 1,
  kWater = 2,
  kSlime = 3,
  kLava = 4,
  kHidden = 5,
  kWarp = 6,
  kTele = 7  // a teleporter's face (its texture named for one)
};
inline constexpr int kKinds = 8;

// One vertex: engine metres, the texture's texels, the lightmap atlas's uv (its first style's
// layer), the normal, the texture's rectangle in the atlas (mip 0 texels: x, y, w, h), its kind,
// the map vertex it was made from, its place in the rebaked atlas (rebake.hpp; −1 when there is
// none), its face's light styles (four bytes, the first in the low byte, 255 past the last; the
// k-th style's lightmap is layer k of the atlas) and its texture's animation (`Scene::frames`:
// the sequence's first frame in the low 16 bits, its frame count in the next 8, this texture's
// own frame in the top 8; 0 a still texture).
struct DrawVertex {
  float pos[3];
  float st[2];
  float lm[2];
  float nrm[3];
  float rect[4];
  uint32_t kind;
  uint32_t source;
  float lm2[2];
  uint32_t styles;
  uint32_t anim;
};
static_assert(sizeof(DrawVertex) == 80);

// THE LIGHT STYLES: a map's faces name up to four styles each, numbers 0 .. 63 a game sets to a
// pattern of brightness letters played at ten a second ('a' dark, 'm' the light as baked, 'z' about
// twice it); a face's light is the sum of each style's lightmap times its style's level now.
inline constexpr int kLightStyles = 64;
inline constexpr double kStyleRate = 10;  // letters a second
// The level of `pattern` at time `t` s (1 for an empty pattern: the light as baked).
double style_level(std::string_view pattern, double t);
// The textures' animation: frames a second (a sequence of "+0" .. "+9" textures).
inline constexpr double kFrameRate = 5;

// A liquid's top drawn by our renderer: its kind, its level (m) and its box over the plan (x0, z0,
// x1, z1, m).
struct LiquidPatch {
  Kind kind = kWater;
  double level = 0;
  double box[4] = {0, 0, 0, 0};
  double area = 0;            // m^2 of the top surfaces in it
  double centre[2] = {0, 0};  // x, z of its largest top surface's middle, m
  double largest = 0;         // that surface's area, m^2
};

// A teleporter's face as one panel: its middle (engine metres), the way it faces (unit, level),
// its width across and height (m). Faces of one panel lie within a metre and face one way.
struct Portal {
  double centre[3] = {0, 0, 0};
  double normal[3] = {0, 0, 1};
  double width = 0, height = 0;
  double area = 0;  // m^2
};

// How a light entity's brightness moves (our own functions, picked by the entity's style number).
enum class Flicker : uint8_t { steady, candle, pulse, strobe, buzz };

// The fire a light entity's model stands for, drawn as burning gas (bsp_flames.hpp): a wall
// torch; a large flame with room over it (a brazier on the floor, which burns as a bonfire under
// ce_bonfires); a large flame with no such room (a lantern's cage round it, or a low roof), which
// burns bigger under ce_bonfires; or a small flame, drawn as a large one's. `place_fires` tells
// the large ones apart.
enum class Fire : uint8_t { none, wall_torch, large, flame, lantern };

struct PointLight {
  double pos[3] = {0, 0, 0};  // engine metres
  double value = 300;         // the entity's brightness
  double reach = 7.5;         // m: where its light ends (value / wait map units)
  double colour[3] = {1, 1, 1};
  Flicker flicker = Flicker::steady;
  uint32_t seed = 0;       // the flicker's phase
  bool flame = false;      // a torch or a flame (warmer)
  Fire fire = Fire::none;  // the fire drawn at it (bsp_flames.hpp)
  float sky = 0;           // the share of open sky over its fire, 0..1 (bsp_flames' place_fires)
  bool physical = false;   // `value` is a luminous intensity, cd (a glowing liquid's light)
};
// The light's share of its brightness at time t, s.
double flicker_at(const PointLight& light, double t);

struct Atlas {
  uint32_t width = 0, height = 0, mips = 1;
  std::vector<std::vector<uint8_t>> levels;  // RGBA8 a level; alpha 255 a fullbright texel
};

// The classic sky texture's colours, the means of its texels in linear light (0..1): its cloud
// layer (the left half) where it is not see-through, its back layer (the right half), and the
// share of the cloud layer that is cloud. `found` false: the map has no sky texture.
struct SkyColours {
  double cloud[3] = {0, 0, 0};
  double back[3] = {0, 0, 0};
  double cover = 0;
  bool found = false;
};

struct Scene {
  std::string name;
  Map map;
  Atlas textures;  // sRGB colours, every mip the file stores
  // THE RELIEF (TextureSource::surface): the textures' surfaces in the same places and mips as
  // `textures`, each texel the normal's x and y in tangent space (128 flat; +Y up the picture), the
  // height (255 the top) and the roughness, linear bytes; a texture with none is flat at the top
  // with a roughness of 0.75. Empty (no levels) when no texture has one.
  Atlas surface;
  // THE COVER: how solid each texel is, one byte a texel (255 solid, 0 see-through) in the same
  // places and mips as `textures`; a face is cut where its texel's cover is under a half. From a
  // full-colour picture's alpha (`MapPicture::rgba`) and a fence texture's (a name starting `{`)
  // index 255. Empty (no levels) when every texel is solid.
  Atlas cover;
  // The lightmaps, a luxel's border round each, in `light_layers` layers stacked down the atlas:
  // layer k holds every face's k-th style (a face's place the same in each).
  Atlas light;
  uint32_t light_layers = 1;
  // The animated textures' frames, each one's rectangle in the atlas (x, y, w, h), a sequence's
  // frames together in order (DrawVertex::anim).
  std::vector<std::array<float, 4>> frames;
  std::vector<DrawVertex> vertices;
  std::vector<uint32_t> indices;
  // Each map surface's run of `indices` (first, count; 0 0 a surface not drawn), for the cull.
  std::vector<std::array<uint32_t, 2>> face_runs;
  // Each brush model's run of `vertices` (first, end; 0 0 a model with nothing drawn), [0] the
  // world's: a moving model's faces are moved by their run (bsp_draw.hpp's `set_movers`).
  std::vector<std::array<uint32_t, 2>> model_vertices;
  std::vector<LiquidPatch> liquids;
  std::vector<Portal> portals;  // the teleporters' panels, largest first
  std::vector<PointLight> lights;
  double mins[3] = {0, 0, 0}, maxs[3] = {0, 0, 0};  // engine metres
  // Where the player starts: engine metres and a heading.
  double start[3] = {0, 0, 0};
  double start_heading = 0;
  uint32_t sky_texture = UINT32_MAX;  // the first sky texture (the classic before's)
  SkyColours sky_colours;             // that texture's colours (bsp_sky.hpp picks our sky by them)
  uint32_t counts[kKinds] = {};       // triangles of each kind
  Rebaked rebaked;                    // the map's light rebaked (attach_rebake), or empty
};

// THE TEXTURE SOURCE: where a texture's pixels come from. The map's own (its miptex through the
// palette) unless `replace` is set and fills `rgba` (sRGB; alpha 255 a texel that glows) for the
// texture `name` of the map's size w x h, from its own colours `indexed` (palette indices) — a
// procedural texture picked by its likeness, or an upscaled one. `rgba` is w x h x 4 or k times
// as wide and tall, k 2, 4 or 8 (the atlas then keeps more mips and the faces sample it finer); the
// frames of an animated texture ('+' names) share one k, or all keep their own. False keeps the
// map's own.
//
// `surface`, when set, is asked for each texture `replace` filled, at that picture's size w x h:
// it fills `rgba` (w x h x 4) with the texture's relief as `Scene::surface` holds it, or returns
// false for a flat one.
struct TextureSource {
  bool (*replace)(void* user, std::string_view name, uint32_t w, uint32_t h,
                  std::span<const uint8_t> indexed, std::span<const uint8_t> palette,
                  std::vector<uint8_t>* rgba) = nullptr;
  void* user = nullptr;
  bool (*surface)(void* user, std::string_view name, uint32_t w, uint32_t h,
                  std::vector<uint8_t>* rgba) = nullptr;
  // The map's pictures as the game read them (its own reader of its textures, one entry a texture
  // in the map's order, views valid through `load_scene`); null: what the file carries itself
  // (`read_map_pictures`, 29 and BSP2).
  const std::vector<MapPicture>* pictures = nullptr;
};
// A texel of `Scene::surface` where a texture has no relief: flat, at the top, fairly rough.
inline constexpr uint8_t kFlatSurface[4] = {128, 128, 255, 191};

// The map `path` in `pack` read and made ready, with `palette` (768 bytes) for its pixels.
bool load_scene(const Pack& pack, std::string_view path, std::span<const uint8_t> palette,
                Scene* out, std::string* error, const TextureSource& source = {});

// Each texture's mean colour in linear light (rebake.hpp's albedo), from the atlas's mip 0.
std::vector<std::array<float, 3>> texture_means(const Scene& scene);
// The scene's light entities as a bake's lights: each with the falloff the real-time path gives it
// (bsp_draw.hpp; `strength` its Options::strength), steady, its shadow soft over 0.2 m.
std::vector<bake::Light> bake_lights(const Scene& scene, double strength = 1);
// `baked` kept with the scene and every vertex given its place in it (DrawVertex::lm2).
void attach_rebake(Scene& scene, Rebaked baked);

// The classic's heading (degrees counter-clockwise from +x, from above) as the engine's.
double heading_of(double angle_deg);

}  // namespace ge::brush::app
