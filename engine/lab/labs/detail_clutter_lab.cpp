// LAB `detail-clutter`: the floor clutter (engine/detail/props.hpp `clutter_rules`) chosen by what
// each floor is made of (surfaces.hpp: its texture's name and mean colour) and by whether it lies
// under the open sky, scattered over the test yard (test_hall.hpp `test_yard`): a lawn named
// grass, a dirt path, ground known as grass only by its green, a flagged court with a garden wall
// and a boulder, and a roofed room with a metal deck and a timber floor. Shows every new shape's
// variants; the yard bare and dressed; the yard from above; six places close up; and a table of
// each texture's class by name and by colour with the props it took.
//
// ge:summary Lab: floor clutter by surface class and open sky over the test yard
// ge:keywords lab detail clutter props grass pebble rock dirt twig branch shell bone
// ge:keywords surface-class texture-name colour outdoor indoor sky yard clutter_rules
// ge:skill detail labs
// ge:test tests/detail_clutter_test.cpp
#include <array>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "detail/prop_shapes.hpp"
#include "detail/props.hpp"
#include "detail/surfaces.hpp"
#include "detail/test_hall.hpp"
#include "detail_view.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"

namespace {

namespace gl = ge::lab;
namespace gd = ge::detail;
namespace dv = ge::lab::detail_view;

constexpr gl::Rgba kSky{118, 146, 182};
constexpr int kFirstNew = static_cast<int>(gd::PropKind::pebble);

dv::Light yard_light() {
  dv::Light l;
  l.key_dir = gd::normalized({-0.5, 0.85, -0.35});
  l.key = 1.9;
  l.lamp_power = 0;
  l.ambient = 0.26;
  return l;
}

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
    const auto& list = shapes.mesh[static_cast<int>(p.kind)];
    dv::draw_prop(c, list[p.variant % list.size()], gd::prop_frame(p),
                  dv::prop_material(p.kind, p.variant));
  }
}

void gallery(gl::Lab& lab, const Shapes& shapes, int cell) {
  constexpr int kVariants = 4;
  const int kinds = gd::kPropKinds - kFirstNew;
  gl::Picture pic = gl::create_picture(cell * kVariants, cell * kinds, kSky);
  gd::TriMesh floor;
  floor.pos = {{-0.7, 0, -0.7}, {-0.7, 0, 0.7}, {0.7, 0, 0.7}, {0.7, 0, -0.7}};
  floor.idx = {0, 1, 2, 0, 2, 3};
  floor.tag = {1, 1};
  const dv::Cam cam{{0.85, 0.75, -0.95}, {0, 0.12, 0}, 40, 1};
  for (int row = 0; row < kinds; ++row) {
    const int k = kFirstNew + row;
    for (int v = 0; v < kVariants; ++v) {
      dv::Canvas c = dv::make_canvas(cell, cell, cam, yard_light());
      dv::draw_mesh(c, floor, dv::yard_paint);
      gd::Prop p;
      p.kind = static_cast<gd::PropKind>(k);
      p.variant = static_cast<uint8_t>(v);
      p.size_m = 0.8;
      p.yaw_rad = 0.5;
      dv::draw_prop(c, shapes.mesh[k][static_cast<size_t>(v)], gd::prop_frame(p),
                    dv::prop_material(p.kind, static_cast<uint32_t>(v)));
      gl::blit(pic, dv::finish(c, kSky), v * cell, row * cell);
    }
    dv::label(pic, 4, row * cell + 4, std::string(gd::prop_name(static_cast<gd::PropKind>(k))));
  }
  lab.shot({"shapes",
            "THE CLUTTER'S SHAPES, FOUR VARIANTS EACH",
            "Every new prop shape built in code, drawn 0.8 m across on bare earth: pebbles, "
            "rocks, clods of dirt, grass tufts, twigs, branches, spent shells and bones.",
            "pebbles round and smooth, rocks angular; clods low; tufts of thin blades leaning "
            "out; twigs thin, branches forked; shells small brass cylinders; bones with knobbed "
            "ends",
            {}},
           pic);
}

const dv::Cam kYardCam{{0.0, 6.5, -6.0}, {0.0, 0.0, 8.0}, 52, 5};

struct Close {
  const char* name;
  dv::Cam cam;
};
const Close kCloses[] = {
    {"LAWN (NAMED GRASS) BY THE BOULDER", {{-4.2, 1.3, 4.0}, {-6.0, 0.0, 7.0}, 52, 1}},
    {"DIRT PATH", {{-0.5, 1.4, 2.0}, {-0.5, 0.0, 5.2}, 52, 1}},
    {"GROUND, GRASS BY ITS GREEN", {{5.0, 1.3, 0.8}, {5.6, 0.0, 4.0}, 52, 1}},
    {"COURT AT THE DOOR, IN THE DAMP", {{4.4, 1.5, 5.4}, {5.8, 0.2, 8.6}, 52, 1}},
    {"METAL DECK, UNDER THE ROOF", {{4.5, 1.7, 9.3}, {4.4, 0.0, 12.6}, 60, 1}},
    {"WOOD FLOOR, UNDER THE ROOF", {{7.5, 1.7, 9.3}, {7.6, 0.0, 12.6}, 60, 1}},
};

void run(gl::Lab& lab) {
  const int w = static_cast<int>(lab.arg_num("width"));
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));
  const gd::TestYard yard = gd::test_yard();
  const gd::TriMesh mesh = dv::mesh_of(yard.solids);
  const gd::Receivers rec = gd::bin_receivers(mesh);

  // Each tag's class, as a level's loader would hand it over: by the texture's name and colour.
  std::vector<gd::SurfaceClass> classes;
  gl::LabTable tt{"Each texture's class, and the clutter it took",
                  {"tag", "texture", "by name", "with colour", "grass", "twigs", "stones", "dirt",
                   "moss", "shells", "bones"},
                  {}};
  classes.reserve(yard.textures.size());
  for (const gd::YardTexture& t : yard.textures) {
    classes.push_back(gd::classify_surface(t.name, t.rgb));
  }
  gd::ScatterEnv env;
  env.damp = gd::yard_damp;
  env.ctx = &yard;
  env.class_of_tag = classes;
  const auto rules = gd::clutter_rules();
  gd::ScatterReport rep;
  const auto t0 = std::chrono::steady_clock::now();
  const std::vector<gd::Prop> props = gd::scatter_props(mesh, rec, rules, seed, env, &rep);
  const auto t1 = std::chrono::steady_clock::now();

  // Props by the tag they stand on.
  constexpr int kCols = 7;
  std::vector<std::array<int, kCols>> by_tag(yard.textures.size(), std::array<int, kCols>{});
  for (const gd::Prop& p : props) {
    const uint16_t tag = mesh.tag[p.tri];
    if (tag >= by_tag.size()) continue;
    int col = -1;
    switch (p.kind) {
      case gd::PropKind::grass:
        col = 0;
        break;
      case gd::PropKind::twig:
      case gd::PropKind::branch:
        col = 1;
        break;
      case gd::PropKind::pebble:
      case gd::PropKind::rock:
        col = 2;
        break;
      case gd::PropKind::dirt:
        col = 3;
        break;
      case gd::PropKind::moss:
        col = 4;
        break;
      case gd::PropKind::shell:
        col = 5;
        break;
      case gd::PropKind::bone:
        col = 6;
        break;
      default:
        break;
    }
    if (col >= 0) ++by_tag[tag][static_cast<size_t>(col)];
  }
  for (size_t t = 0; t < yard.textures.size(); ++t) {
    const gd::YardTexture& tx = yard.textures[t];
    std::vector<std::string> row{std::to_string(t), tx.name,
                                 std::string(gd::surface_name(gd::classify_name(tx.name))),
                                 std::string(gd::surface_name(classes[t]))};
    for (int c = 0; c < kCols; ++c)
      row.push_back(std::to_string(by_tag[t][static_cast<size_t>(c)]));
    tt.rows.push_back(row);
  }
  lab.table(tt);
  // The ones that say the rules hold: grass on the lawns and none on the metal deck.
  lab.metric("grass-on-lawn", by_tag[0][0], "count");
  lab.metric("grass-on-green-ground", by_tag[2][0], "count");
  lab.metric("grass-on-metal", by_tag[5][0], "count");
  lab.metric("grass-on-wood", by_tag[6][0], "count");
  lab.metric("stones-on-metal", by_tag[5][2], "count");
  lab.metric("shells-on-metal", by_tag[5][5], "count");

  gl::LabTable t{"Props by kind: sampled, placed and dropped for spacing",
                 {"kind", "tried", "placed", "too close", "buried"},
                 {}};
  for (int k = 0; k < gd::kPropKinds; ++k) {
    if (rep.tried[k] == 0) continue;
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
    dv::Canvas bare = dv::make_canvas(w, h, kYardCam, yard_light());
    dv::draw_mesh(bare, mesh, dv::yard_paint);
    gl::blit(pic, dv::finish(bare, kSky), 0, 0);
    dv::Canvas dressed = dv::make_canvas(w, h, kYardCam, yard_light());
    dv::draw_mesh(dressed, mesh, dv::yard_paint);
    draw_props(dressed, shapes, props);
    gl::blit(pic, dv::finish(dressed, kSky), w, 0);
    dv::label(pic, 8, 8, "BARE");
    dv::label(pic, w + 8, 8, "CLUTTER BY SURFACE, SEED " + lab.arg("seed"));
    lab.shot({"yard",
              "THE YARD BARE AND CLUTTERED",
              "The test yard as built (left) and with the clutter its floors' classes call for "
              "(right). From the left: a lawn named grass with a boulder and a garden wall, a dirt "
              "path, green ground whose name says only 'ground', and the flagged court with the "
              "roofed room on it.",
              "grass thick on both lawns and fringing the path; stones along the wall's foot and "
              "the boulder; the court's flags with pebbles at the room's walls and no grass",
              {{"seed", lab.arg("seed")}}},
             pic);
  }
  {
    // From above with the roof left off, so the room's floors show.
    gd::TriMesh open = mesh;
    for (size_t i = 0; i < open.tag.size(); ++i) {
      if (open.tag[i] != 7) continue;
      for (int k = 0; k < 3; ++k) open.pos[open.idx[i * 3 + static_cast<size_t>(k)]].y = -50;
    }
    const dv::Cam top{{0.0, 30.0, 7.99}, {0.0, 0.0, 8.0}, 0, 8.4};
    dv::Canvas c = dv::make_canvas(w, w * 168 / 200, top, yard_light());
    dv::draw_mesh(c, open, dv::yard_paint);
    draw_props(c, shapes, props);
    lab.shot(
        {"plan",
         "THE YARD FROM ABOVE, ROOF OFF",
         "The yard looked straight down with the room's roof lifted off: the lawn at the left, "
         "the path, the green ground at the lower right and the court at the upper right with "
         "the room's metal deck and timber floor inside its brick shell.",
         "the two lawns green with tufts and twigs, a branch or two; the path with clods and "
         "pebbles and grass at its edges; inside the room shells and a bone but no grass; "
         "moss round the doorway's damp",
         {{"seed", lab.arg("seed")}}},
        dv::finish(c, kSky));
  }
  {
    const int pw = w / 2, ph = pw * 5 / 8;
    constexpr int kCount = static_cast<int>(sizeof(kCloses) / sizeof(kCloses[0]));
    gl::Picture pic = gl::create_picture(pw * 2, ph * (kCount / 2), kSky);
    for (int k = 0; k < kCount; ++k) {
      dv::Canvas c = dv::make_canvas(pw, ph, kCloses[k].cam, yard_light());
      dv::draw_mesh(c, mesh, dv::yard_paint);
      draw_props(c, shapes, props);
      gl::Picture p = dv::finish(c, kSky);
      dv::label(p, 6, 6, kCloses[k].name);
      gl::blit(pic, p, pw * (k % 2), ph * (k / 2));
    }
    lab.shot({"close-ups",
              "CLUTTER CLOSE UP",
              "Six places at standing height: the lawn by the boulder, the dirt path, the green "
              "ground, the court at the room's door in the damp, and the metal deck and the "
              "timber floor under the roof.",
              "grass only out in the open on the grassy floors; rocks and pebbles at the "
              "boulder's foot; clods on the path; moss in the damp by the door; spent shells and "
              "the odd bone indoors, with no grass or twigs",
              {{"seed", lab.arg("seed")}}},
             pic);
  }
}

const gl::LabSpec kSpec{
    "detail-clutter",
    gl::LabKind::picture,
    "engine",
    "engine/detail/props.hpp, engine/detail/surfaces.hpp, engine/detail/prop_shapes.hpp",
    "detail",
    "Does each floor take the clutter its texture calls for (grass on grass, stones on stone, "
    "clods on earth, shells and bones anywhere) and does the open sky decide where grass and "
    "twigs grow?",
    {{"seed", "38", "the scatter's seed"}, {"width", "960", "a yard picture's width, px"}},
    "--width=240",
    run};
GE_LAB(kSpec);

}  // namespace
