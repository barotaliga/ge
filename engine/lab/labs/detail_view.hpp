// THE DETAIL LABS' VIEW: a level's triangles drawn on the CPU in perspective, lit PER PIXEL (a key
// light, a warm lamp, a sky fill and a glint, so a bevel's round catches the light), with decals
// blended into the surface's own colour and gloss before the light is applied, and lines drawn
// over it depth-tested (an edge's verdict, a decal's triangles). No GPU: the detail passes are
// judged in the container, and every pixel comes from double arithmetic and the engine's libm,
// so a picture is the same on every row and part of the lab's digest.
//
// The canvas keeps a pixel's albedo, gloss, diffuse light and glint apart until `finish`, which
// composes them, tone-maps, applies the display's gamma and averages the supersampling down.
//
// ge:summary Detail labs' CPU view: per-pixel lit triangles, decals blended, depth-tested lines
// ge:keywords lab detail view raster perspective per-pixel lighting decal blend near-plane
// ge:keywords canvas supersample tone-map lines
// ge:skill detail labs
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "detail/bevel.hpp"
#include "detail/decals.hpp"
#include "detail/mesh.hpp"
#include "detail/prop_shapes.hpp"
#include "detail/props.hpp"
#include "lab/raster.hpp"

namespace ge::lab::detail_view {

namespace gd = ge::detail;

struct Cam {
  gd::Vec3 eye{0, 1.6, -4};
  gd::Vec3 target{0, 1, 5};
  double fov_deg = 60;  // vertical; 0: orthographic
  double half_m = 5;    // orthographic: half the picture's height
};

// A surface's colour (linear 0..1) and gloss at a point.
struct Material {
  double r, g, b;
  double gloss;
};
using Paint = Material (*)(uint16_t tag, const gd::Vec3& p, const gd::Vec3& n);

struct Light {
  gd::Vec3 key_dir = gd::normalized({-0.45, 0.8, -0.4});  // toward the key light
  double key = 1.6;
  gd::Vec3 lamp{1.5, 2.2, 3};  // a warm point light
  double lamp_power = 6;       // its strength at 1 m
  double ambient = 0.18;
};

struct Canvas {
  int w = 0, h = 0, ss = 2;
  Cam cam;
  Light light;
  gd::Vec3 right, up, fwd;  // the camera's basis
  double focal = 1;
  std::vector<double> depth;  // view z, m; +inf empty
  std::vector<float> albedo;  // rgb
  std::vector<float> gloss;
  std::vector<float> diffuse;  // rgb light on a white surface
  std::vector<float> glint;    // the specular term at full gloss
  std::vector<float> over;     // rgba drawn over the lit picture (lines)
  std::vector<float> emit;     // rgb light given off (a scorch's embers), added after lighting
};

// A canvas of `w` x `h` final pixels drawn `ss` times larger, under `cam` and `light`.
Canvas make_canvas(int w, int h, const Cam& cam, const Light& light = {}, int ss = 2);

// The mesh's triangles, each painted by `paint` and lit by its own face's normal.
void draw_mesh(Canvas& c, const gd::TriMesh& mesh, Paint paint);
// A prop's faceted shape at its frame in one material.
void draw_prop(Canvas& c, const gd::PropMesh& shape, const gd::PropFrame& frame, Material m);
// A decal's triangles, its mask and its look at `age_s` blended into the surface under it.
void draw_decal(Canvas& c, std::span<const gd::DecalVertex> verts, gd::DecalKind kind,
                uint32_t variant, double age_s);
// A line from `a` to `b`, `px` final pixels wide, hidden where a surface is in front of it.
void draw_line3(Canvas& c, const gd::Vec3& a, const gd::Vec3& b, Rgba colour, double px = 1.5);

// The lit, tone-mapped picture at the final size; `sky` where nothing was drawn.
Picture finish(const Canvas& c, Rgba sky);

// Every solid's faces as one mesh.
gd::TriMesh mesh_of(const std::vector<gd::Solid>& solids);
// The solids bevelled against each other, as solids (and the full results when asked).
std::vector<gd::Solid> bevelled(const std::vector<gd::Solid>& solids, const gd::BevelSpec& spec,
                                std::vector<gd::BevelResult>* out = nullptr);
// White words on a dark plate at (x, y), twice the font's size on a picture 600 px wide or more.
void label(Picture& p, int x, int y, const std::string& text);

// The test hall's materials: flagstones, ashlar walls, timber, a dark wet basin.
Material hall_paint(uint16_t tag, const gd::Vec3& p, const gd::Vec3& n);
// The test yard's (test_hall.hpp `test_yard`): lawn, a dirt path, green ground, stone flags,
// brick, a metal deck, a wood floor and a roof, by tag.
Material yard_paint(uint16_t tag, const gd::Vec3& p, const gd::Vec3& n);
// A prop's material by its kind and variant.
Material prop_material(gd::PropKind kind, uint32_t variant);

}  // namespace ge::lab::detail_view
