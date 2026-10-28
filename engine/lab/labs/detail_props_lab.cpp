// LAB `detail-props`: the seeded props (engine/detail/props.hpp, prop_shapes.hpp) scattered over
// the bevelled test hall by the default rules, so a change to a rule, the scatter or a shape shows
// as pictures and counts: every shape's variants; the hall bare and dressed; the hall from above
// with every prop; close-ups of rubble heaped at a wall's foot, moss round the basin, debris on
// the open floor and moss low on a damp wall; and the hall with its bevels, its props and a fire
// fight's decals together. With the props placed, tried and dropped for spacing, by kind.
//
// ge:summary Lab: props by rules in the hall: shapes, bare and dressed, plan, close-ups, counts
// ge:keywords lab detail props rubble debris moss scatter rules plan dressed seed counts
// ge:skill detail labs
// ge:test tests/detail_props_test.cpp
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "detail/decals.hpp"
#include "detail/prop_shapes.hpp"
#include "detail/props.hpp"
#include "detail/test_hall.hpp"
#include "detail_view.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"

namespace {

namespace gl = ge::lab;
namespace gd = ge::detail;
namespace dv = ge::lab::detail_view;

constexpr gl::Rgba kSky{38, 42, 52};

dv::Material material_of(gd::PropKind kind, uint32_t variant) {
  switch (kind) {
    case gd::PropKind::rubble:
      return {0.33, 0.31, 0.28, 0.12};
    case gd::PropKind::debris:
      return variant % 2 == 0 ? dv::Material{0.47, 0.44, 0.39, 0.1}
                              : dv::Material{0.31, 0.20, 0.11, 0.08};
    case gd::PropKind::moss:
      return {0.17, 0.30, 0.06, 0.04};
  }
  return {1, 0, 1, 0};
}

// Every prop's shape, made once a kind and variant.
struct Shapes {
  std::vector<gd::PropMesh> mesh[gd::kPropKinds];
  explicit Shapes(int variants) {
    for (int k = 0; k < gd::kPropKinds; ++k) {
      for (int v = 0; v < variants; ++v) {
        mesh[k].push_back(gd::prop_mesh(static_cast<gd::PropKind>(k), static_cast<uint32_t>(v)));
      }
    }
  }
};

void draw_props(dv::Canvas& c, const Shapes& shapes, const std::vector<gd::Prop>& props) {
  for (const gd::Prop& p : props) {
    const int k = static_cast<int>(p.kind);
    const auto& list = shapes.mesh[k];
    dv::draw_prop(c, list[p.variant % list.size()], gd::prop_frame(p),
                  material_of(p.kind, p.variant));
  }
}

void gallery(gl::Lab& lab, const Shapes& shapes, int cell) {
  constexpr int kVariants = 4;
  gl::Picture pic = gl::create_picture(cell * kVariants, cell * gd::kPropKinds, kSky);
  gd::TriMesh floor;
  floor.pos = {{-0.7, 0, -0.7}, {-0.7, 0, 0.7}, {0.7, 0, 0.7}, {0.7, 0, -0.7}};
  floor.idx = {0, 1, 2, 0, 2, 3};
  floor.tag = {0, 0};
  const dv::Cam cam{{0.85, 0.75, -0.95}, {0, 0.12, 0}, 40, 1};
  for (int k = 0; k < gd::kPropKinds; ++k) {
    for (int v = 0; v < kVariants; ++v) {
      dv::Light light;
      light.lamp = {-0.6, 1.2, -0.8};
      light.lamp_power = 1.2;
      dv::Canvas c = dv::make_canvas(cell, cell, cam, light);
      dv::draw_mesh(c, floor, dv::hall_paint);
      gd::Prop p;
      p.kind = static_cast<gd::PropKind>(k);
      p.variant = static_cast<uint8_t>(v);
      p.size_m = 0.8;
      p.yaw_rad = 0.5;
      dv::draw_prop(c, shapes.mesh[k][static_cast<size_t>(v)], gd::prop_frame(p),
                    material_of(p.kind, static_cast<uint32_t>(v)));
      gl::Picture panel = dv::finish(c, kSky);
      gl::blit(pic, panel, v * cell, k * cell);
    }
    dv::label(pic, 4, k * cell + 4, std::string(gd::prop_name(static_cast<gd::PropKind>(k))));
  }
  lab.shot({"shapes",
            "EVERY PROP'S SHAPE, FOUR VARIANTS EACH",
            "The props' faceted shapes built in code, 0.8 m across on a flagstone: rubble lumps "
            "(top), debris as slab shards and timber splinters, and moss cushions (bottom).",
            "every lump different and closed, sitting flat on its base; shards and splinters "
            "with broken outlines; moss low and lumpy",
            {}},
           pic);
}

const dv::Cam kHallCam{{0.4, 2.3, -2.6}, {0.6, 1.0, 6.0}, 58, 5};

struct Close {
  const char* name;
  dv::Cam cam;
};
const Close kCloses[] = {
    {"RUBBLE AT THE BACK WALL", {{-2.4, 1.3, 7.6}, {-3.2, 0.0, 9.9}, 50, 1}},
    {"MOSS ROUND THE BASIN", {{-1.0, 1.4, 0.0}, {-3.5, 0.0, 2.6}, 52, 1}},
    {"DEBRIS ON THE FLOOR", {{2.4, 1.3, 0.8}, {3.0, 0.0, 3.4}, 52, 1}},
    {"MOSS LOW ON A DAMP WALL", {{-3.6, 1.1, 2.4}, {-6.0, 0.4, 2.2}, 50, 1}},
};

void run(gl::Lab& lab) {
  const int w = static_cast<int>(lab.arg_num("width"));
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));
  const gd::TestHall hall = gd::test_hall();
  gd::BevelSpec spec;
  spec.radius_m = 0.05;
  const gd::TriMesh mesh = dv::mesh_of(dv::bevelled(hall.solids, spec));
  const gd::Receivers rec = gd::bin_receivers(mesh);
  const auto rules = gd::default_rules();
  gd::ScatterReport rep;
  const auto t0 = std::chrono::steady_clock::now();
  const std::vector<gd::Prop> props =
      gd::scatter_props(mesh, rec, rules, seed, gd::hall_damp, &hall, &rep);
  const auto t1 = std::chrono::steady_clock::now();
  gl::LabTable t{"Props by kind: sampled, placed and dropped for spacing",
                 {"kind", "tried", "placed", "too close", "buried"},
                 {}};
  for (int k = 0; k < gd::kPropKinds; ++k) {
    const std::string name(gd::prop_name(static_cast<gd::PropKind>(k)));
    t.rows.push_back({name, std::to_string(rep.tried[k]), std::to_string(rep.placed[k]),
                      std::to_string(rep.too_close[k]), std::to_string(rep.buried[k])});
    lab.metric("placed-" + name, rep.placed[k], "count");
  }
  lab.table(t);
  lab.metric("scatter-ms", std::chrono::duration<double, std::milli>(t1 - t0).count(), "ms",
             gl::Det::measured);
  const Shapes shapes(4);
  size_t tris = 0;
  for (const gd::Prop& p : props) {
    tris += shapes.mesh[static_cast<int>(p.kind)][p.variant % 4].pos.size() / 3;
  }
  lab.metric("prop-triangles", static_cast<double>(tris), "count");

  gallery(lab, shapes, w / 4);

  const int h = w * 9 / 16;
  {
    gl::Picture pic = gl::create_picture(w * 2, h, kSky);
    dv::Canvas bare = dv::make_canvas(w, h, kHallCam);
    dv::draw_mesh(bare, mesh, dv::hall_paint);
    gl::blit(pic, dv::finish(bare, kSky), 0, 0);
    dv::Canvas dressed = dv::make_canvas(w, h, kHallCam);
    dv::draw_mesh(dressed, mesh, dv::hall_paint);
    draw_props(dressed, shapes, props);
    gl::blit(pic, dv::finish(dressed, kSky), w, 0);
    dv::label(pic, 8, 8, "BARE");
    dv::label(pic, w + 8, 8, "DRESSED BY THE DEFAULT RULES, SEED " + lab.arg("seed"));
    lab.shot({"hall",
              "THE HALL BARE AND DRESSED",
              "The bevelled test hall as built (left) and with the props the default rules "
              "scatter over it (right): rubble heaped at the walls' feet, debris thin over the "
              "floor, moss in the damp round the basin.",
              "rubble gathered in the corners and along the walls, rarely in the open; moss only "
              "near the basin; nothing floating or buried",
              {{"seed", lab.arg("seed")}}},
             pic);
  }
  {
    dv::Cam top{{0.0, 30.0, 4.99}, {0.0, 0.0, 5.0}, 0, 5.8};
    dv::Canvas c = dv::make_canvas(w * 3 / 4, w * 3 / 4 * 10 / 13, top);
    dv::draw_mesh(c, mesh, dv::hall_paint);
    draw_props(c, shapes, props);
    lab.shot({"plan",
              "THE HALL FROM ABOVE, EVERY PROP",
              "The test hall looked straight down (the back wall at the top, the open front at "
              "the bottom, the basin at the lower left) with every prop the rules placed.",
              "bands of rubble along every wall's foot and round the pillars' plinths and the "
              "steps; a green halo of moss round the basin; the floor's middle nearly clear",
              {{"seed", lab.arg("seed")}}},
             dv::finish(c, kSky));
  }
  {
    const int pw = w / 2, ph = pw * 5 / 8;
    gl::Picture pic = gl::create_picture(pw * 2, ph * 2, kSky);
    for (int k = 0; k < 4; ++k) {
      dv::Canvas c = dv::make_canvas(pw, ph, kCloses[k].cam);
      dv::draw_mesh(c, mesh, dv::hall_paint);
      draw_props(c, shapes, props);
      gl::Picture p = dv::finish(c, kSky);
      dv::label(p, 6, 6, kCloses[k].name);
      gl::blit(pic, p, pw * (k % 2), ph * (k / 2));
    }
    lab.shot({"close-ups",
              "PROPS CLOSE UP",
              "Four places close up: rubble heaped at the back wall's foot, moss round the basin's "
              "kerb, debris on the open floor, and moss climbing a damp wall from the floor.",
              "stones sitting into the floor, larger ones fewer; moss on the wall only near its "
              "foot; no two props inside each other",
              {{"seed", lab.arg("seed")}}},
             pic);
  }
  {
    // Everything at once: bevels, props, and a few marks.
    gd::DecalPool pool = gd::make_pool(16);
    struct M {
      gd::Vec3 at, n;
      double size, age;
      uint32_t var;
      gd::DecalKind kind;
    };
    const M marks[] = {
        {{6, 0, 3}, gd::normalized({-1, 1, 0}), 1.6, 0, 0, gd::DecalKind::scorch},
        {{1.2, 0, 2.2}, {0, 1, 0}, 1.1, 0, 1, gd::DecalKind::scorch},
        {{3.62, 0.2, 8.4}, gd::normalized({-1, 1.2, 0}), 1, 5, 2, gd::DecalKind::blood},
        {{-0.4, 1.6, 9.6}, {0, 0, -1}, 0.16, 0, 1, gd::DecalKind::bullet_hole},
        {{0.3, 1.9, 9.6}, {0, 0, -1}, 0.16, 0, 2, gd::DecalKind::bullet_hole}};
    dv::Canvas c = dv::make_canvas(w, h, kHallCam);
    dv::draw_mesh(c, mesh, dv::hall_paint);
    for (const M& m : marks) {
      gd::DecalAt d;
      d.kind = m.kind;
      d.at = m.at;
      d.normal = m.n;
      d.size_m = m.size;
      d.variant = m.var;
      const int slot = gd::place_decal(pool, rec, d);
      if (slot >= 0)
        dv::draw_decal(c, gd::decal_verts(pool, static_cast<uint32_t>(slot)), m.kind, m.var, m.age);
    }
    draw_props(c, shapes, props);
    lab.shot({"together",
              "BEVELS, PROPS AND DECALS TOGETHER",
              "The test hall with every detail pass at once: its exposed edges rounded, its "
              "props scattered by the default rules, and a few marks of a fight pressed on.",
              "the hall reads as built of worn stone and timber rather than of boxes",
              {{"seed", lab.arg("seed")}}},
             dv::finish(c, kSky));
  }
}

const gl::LabSpec kSpec{
    "detail-props",
    gl::LabKind::picture,
    "engine",
    "engine/detail/props.hpp, engine/detail/prop_shapes.hpp",
    "detail",
    "Do the default rules heap rubble at walls' feet, spread debris thin and keep moss to the "
    "damp, with every prop standing on its surface and none inside another?",
    {{"seed", "38", "the scatter's seed"}, {"width", "960", "a hall picture's width, px"}},
    "--width=240",
    run};
GE_LAB(kSpec);

}  // namespace
