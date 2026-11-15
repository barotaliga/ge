// A MAP'S SOURCE: what the map compiler reads, whatever it was written in (a `.map` brush file, a
// plan, an SVG floor plan). Entities in order, the first the world (`worldspawn`); an entity has
// its keys and its brushes; a brush is convex, a list of sides, each a plane facing out of it with
// the texture on it and the texture's projection.
//
// A brush's contents follow from its sides' textures: a name starting `*` is a liquid (lava when
// it holds "lava", slime when "slime", water otherwise), one starting `sky` is sky, `clip` is a
// clip brush (it stops bodies and is never drawn), `trigger` a trigger's volume (never drawn); a
// brush takes the first such side's kind, solid when it has none.
//
// A side's projection is `s = vecs[0]·p + vecs[0][3]`, `t = vecs[1]·p + vecs[1][3]` in texels, as
// the families' texinfo stores it. `axial_vecs` is the projection most sources use: the face's
// nearest axis picks the texture plane (a floor: s along +x, t along −y; a wall facing ±x: s along
// +y, t down; facing ±y: s along +x, t down), then shift, turn (degrees) and scale.
//
// ge:summary A map's source for the compiler: entities with keys and convex brushes of sides
// ge:keywords map-compiler source entity brush side texture projection axial contents liquid
// ge:keywords sky clip trigger worldspawn texinfo vecs
// ge:skill brush
// ge:test tests/brush_compile_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "brush/compile/geom.hpp"

namespace ge::brush::mapc {

// A brush's kind, as bits; a leaf takes the strongest of its brushes'.
enum Contents : uint32_t {
  kEmpty = 0,
  kWater = 1,
  kSlime = 2,
  kLava = 4,
  kSky = 8,
  kSolid = 16,
  kClip = 32,     // stops bodies (the box hulls), never drawn, not in the point tree
  kTrigger = 64,  // a trigger's volume: in no tree, never drawn
};
inline bool opaque(uint32_t c) { return (c & (kSolid | kSky)) != 0; }
inline bool liquid(uint32_t c) { return (c & (kWater | kSlime | kLava)) != 0; }

struct Side {
  PlaneD plane;  // facing out of the brush
  std::string texture;
  double vecs[2][4] = {{1, 0, 0, 0}, {0, -1, 0, 0}};
  int32_t flags = 0, value = 0;  // the 38 family's surface flags and value
};

struct Brush {
  std::vector<Side> sides;
  uint32_t contents = kSolid;
  int line = 0;  // where the source wrote it (0: made by the compiler)
};

struct Entity {
  std::vector<std::pair<std::string, std::string>> keys;
  std::vector<Brush> brushes;
  int line = 0;

  const std::string* get(const std::string& key) const;
  void set(const std::string& key, const std::string& value);
};

struct Source {
  std::vector<Entity> entities;  // [0] the world
};

// The contents a texture name gives (kEmpty when it gives none).
uint32_t contents_of_texture(const std::string& name);
// Every brush's contents from its sides' textures.
void set_contents(Source* src);
// The axial projection of a face facing `n`: shift (texels), turn (degrees), scale (units a texel).
void axial_vecs(const V3& n, double shift_s, double shift_t, double turn, double scale_s,
                double scale_t, double out[2][4]);
// A box brush over [lo, hi], every side `texture`, axial projection.
Brush box_brush(const V3& lo, const V3& hi, const std::string& texture);

}  // namespace ge::brush::mapc
