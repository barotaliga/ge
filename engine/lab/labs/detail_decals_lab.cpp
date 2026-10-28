// LAB `detail-decals`: the decals (engine/detail/decals.hpp) pressed into the bevelled test hall,
// so a change to a mask, a look or the clip shows as pictures: every kind's masks; the hall
// marked by a fire fight (scorches at a wall's foot and on the floor, bullet holes in a wall and
// round a pillar, blood over a step's nose, on the floor and on a wall); close-ups of the marks
// that wrap a corner; one splash of blood drying and fading through its life; and the decals'
// own triangles over the surfaces they were clipped to. With the count of vertices each decal
// took and what placing one costs.
//
// ge:summary Lab: decals in the hall: masks, a fire fight's marks, wrapping, blood ageing, cost
// ge:keywords lab detail decal scorch bullet-hole blood mask atlas wrap age fade clip cost
// ge:skill detail labs
// ge:test tests/detail_decals_test.cpp
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "detail/decals.hpp"
#include "detail/test_hall.hpp"
#include "detail_view.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"

namespace {

namespace gl = ge::lab;
namespace gd = ge::detail;
namespace dv = ge::lab::detail_view;

constexpr gl::Rgba kSky{38, 42, 52};

struct Mark {
  gd::DecalAt at;
  double age_s;
};

std::vector<Mark> fire_fight() {
  std::vector<Mark> m;
  auto add = [&](gd::DecalKind k, gd::Vec3 p, gd::Vec3 n, double size, double spin, uint32_t var,
                 double age) {
    gd::DecalAt d;
    d.kind = k;
    d.at = p;
    d.normal = gd::normalized(n);
    d.size_m = size;
    d.spin_rad = spin;
    d.variant = var;
    m.push_back({d, age});
  };
  using K = gd::DecalKind;
  add(K::scorch, {6, 0, 4.6}, {-1, 1, 0}, 1.6, 0.4, 0, 0);
  add(K::scorch, {1.2, 0, 2.2}, {0, 1, 0}, 1.1, 2.0, 1, 0);
  for (int i = 0; i < 9; ++i) {
    const int row = i / 3;
    const double x = -4.2 + (i % 3) * 0.55 + 0.13 * row, y = 1.1 + row * 0.42;
    add(K::bullet_hole, {x, y, 10}, {0, 0, -1}, 0.16, i * 1.3, static_cast<uint32_t>(i % 4), 0);
  }
  for (int i = 0; i < 5; ++i) {
    const double a = (215 + i * 22) * ge::core::kPi / 180;
    const gd::Vec3 n{ge::core::libm::cos(a), 0, ge::core::libm::sin(a)};
    add(K::bullet_hole, {-3 + 0.39 * n.x, 1.3 + 0.3 * (i % 2), 6 + 0.39 * n.z}, n, 0.16, i, i % 4,
        0);
  }
  add(K::blood, {3.62, 0.2, 8.4}, {-1, 1.2, 0}, 1.0, 0.7, 2, 0);
  add(K::blood, {-4.2, 0, 8.2}, {0, 1, 0}, 0.9, 2.4, 1, 20);
  add(K::blood, {0.9, 1.3, 10}, {0, 0, -1}, 0.8, -1.4, 3, 70);
  return m;
}

void place_all(gd::DecalPool& pool, const gd::Receivers& rec, const std::vector<Mark>& marks,
               std::vector<int>& slots) {
  slots.clear();
  for (const Mark& m : marks) slots.push_back(gd::place_decal(pool, rec, m.at));
}

void draw_scene(dv::Canvas& c, const gd::TriMesh& mesh, const gd::DecalPool& pool,
                const std::vector<Mark>& marks, const std::vector<int>& slots, double extra_age) {
  dv::draw_mesh(c, mesh, dv::hall_paint);
  for (size_t i = 0; i < marks.size(); ++i) {
    if (slots[i] < 0) continue;
    const auto slot = static_cast<uint32_t>(slots[i]);
    dv::draw_decal(c, gd::decal_verts(pool, slot), marks[i].at.kind, marks[i].at.variant,
                   marks[i].age_s + extra_age);
  }
}

void masks(gl::Lab& lab, int cell) {
  constexpr int kVariants = 4;
  gl::Picture pic = gl::create_picture(cell * kVariants, cell * gd::kDecalKinds, kSky);
  for (int k = 0; k < gd::kDecalKinds; ++k) {
    const auto kind = static_cast<gd::DecalKind>(k);
    const gd::DecalLook look = gd::look_at(kind, 0);
    for (int var = 0; var < kVariants; ++var) {
      for (int y = 0; y < cell; ++y) {
        for (int x = 0; x < cell; ++x) {
          const gd::MaskSample m =
              gd::mask_at(kind, static_cast<uint32_t>(var), (x + 0.5) / cell, (y + 0.5) / cell);
          const double bg = 0.62, a = m.cover * look.opacity;
          const double tone = ge::core::clamp(1 + 0.7 * m.shade, 0, 1.6);
          const double rgb[3] = {bg * (1 - a) + look.r * tone * 2.2 * a,
                                 bg * 0.96 * (1 - a) + look.g * tone * 2.2 * a,
                                 bg * 0.9 * (1 - a) + look.b * tone * 2.2 * a};
          gl::Rgba c{0, 0, 0, 255};
          c.r = static_cast<uint8_t>(ge::core::clamp(rgb[0], 0, 1) * 255);
          c.g = static_cast<uint8_t>(ge::core::clamp(rgb[1], 0, 1) * 255);
          c.b = static_cast<uint8_t>(ge::core::clamp(rgb[2], 0, 1) * 255);
          gl::set_pixel(pic, var * cell + x, k * cell + y, c);
        }
      }
    }
    dv::label(pic, 4, k * cell + 4, std::string(gd::decal_spec(kind).name));
  }
  lab.shot({"masks",
            "EVERY KIND'S MASKS, FOUR VARIANTS EACH",
            "The decal masks as a renderer bakes them, laid on plain stone in each kind's fresh "
            "look: scorch marks (top), bullet holes with their cracks and dust, and blood with "
            "its run and droplets (bottom).",
            "ragged, streaked scorch edges; a dark hole, pale rim and three to six cracks; blood "
            "with a rim and satellite drops; no mark cut off at its square",
            {}},
           pic);
}

const dv::Cam kHallCam{{0.9, 2.4, -1.4}, {0.9, 0.5, 6.0}, 64, 5};

struct Close {
  const char* name;
  dv::Cam cam;
};
const Close kCloses[] = {
    {"SCORCH AT A WALL'S FOOT", {{3.9, 1.5, 3.3}, {6.0, 0.25, 4.8}, 55, 1}},
    {"SHOTS ROUND A PILLAR", {{-2.6, 1.6, 4.3}, {-3.0, 1.4, 6.0}, 50, 1}},
    {"BLOOD OVER A STEP", {{2.6, 1.2, 7.4}, {3.7, 0.15, 8.4}, 52, 1}},
    {"SHOTS IN A WALL", {{-3.4, 1.7, 8.0}, {-3.5, 1.5, 10.0}, 50, 1}},
};

void run(gl::Lab& lab) {
  const int w = static_cast<int>(lab.arg_num("width"));
  const gd::TestHall hall = gd::test_hall();
  gd::BevelSpec spec;
  spec.radius_m = 0.05;
  const gd::TriMesh mesh = dv::mesh_of(dv::bevelled(hall.solids, spec));
  const gd::Receivers rec = gd::bin_receivers(mesh);
  const std::vector<Mark> marks = fire_fight();
  gd::DecalPool pool = gd::make_pool(64);
  std::vector<int> slots;
  const auto t0 = std::chrono::steady_clock::now();
  place_all(pool, rec, marks, slots);
  const auto t1 = std::chrono::steady_clock::now();

  gl::LabTable t{"Every decal placed: its kind, size, the triangles it was clipped to",
                 {"#", "kind", "size m", "age s", "vertices", "triangles"},
                 {}};
  uint32_t verts = 0;
  int placed = 0;
  for (size_t i = 0; i < marks.size(); ++i) {
    const uint32_t n = slots[i] < 0 ? 0 : pool.decals[static_cast<size_t>(slots[i])].count;
    verts += n;
    placed += slots[i] < 0 ? 0 : 1;
    char size[16], age[16];
    std::snprintf(size, sizeof size, "%.2f", marks[i].at.size_m);
    std::snprintf(age, sizeof age, "%.0f", marks[i].age_s);
    t.rows.push_back({std::to_string(i), std::string(gd::decal_spec(marks[i].at.kind).name), size,
                      age, std::to_string(n), std::to_string(n / 3)});
  }
  lab.table(t);
  lab.metric("decals-placed", placed, "count");
  lab.metric("decal-vertices", verts, "count");
  lab.metric("place-us-each",
             std::chrono::duration<double, std::micro>(t1 - t0).count() /
                 static_cast<double>(marks.size()),
             "us", gl::Det::measured);

  masks(lab, w / 6);

  const int h = w * 9 / 16;
  {
    dv::Canvas c = dv::make_canvas(w, h, kHallCam);
    draw_scene(c, mesh, pool, marks, slots, 0);
    lab.shot({"hall",
              "THE HALL AFTER A FIRE FIGHT",
              "The bevelled test hall with the marks a fight leaves: two scorches (one at the "
              "right wall's foot, one on the floor), nine bullet holes in the back wall and five "
              "round the left pillar, and blood over the steps' first nose, by the corner block "
              "(20 s old) and on the back wall (70 s old, fading).",
              "the wall-foot scorch runs up the wall and over the floor unbroken; the pillar's "
              "holes sit on its facets; the older blood darker and fainter than the fresh",
              {}},
             dv::finish(c, kSky));
  }
  {
    const int pw = w / 2, ph = pw * 5 / 8;
    gl::Picture pic = gl::create_picture(pw * 2, ph * 2, kSky);
    for (int k = 0; k < 4; ++k) {
      dv::Canvas c = dv::make_canvas(pw, ph, kCloses[k].cam);
      draw_scene(c, mesh, pool, marks, slots, 0);
      gl::Picture p = dv::finish(c, kSky);
      dv::label(p, 6, 6, kCloses[k].name);
      gl::blit(pic, p, pw * (k % 2), ph * (k / 2));
    }
    lab.shot({"close-ups",
              "DECALS WRAPPING THE HALL'S CORNERS",
              "Four marks close up: a scorch across a wall's foot, bullet holes across a "
              "pillar's facets, blood run over a step's rounded nose, holes in an ashlar wall.",
              "no mark floats in the air past an edge or stops short at one; the masks keep "
              "their shape across a fold",
              {}},
             pic);
  }
  {
    // One splash aged through its life, close up.
    const double ages[] = {0, 20, 45, 70, 95, 115};
    const int pw = w / 3, ph = pw * 3 / 4;
    gl::Picture pic = gl::create_picture(pw * 3, ph * 2, kSky);
    const dv::Cam cam{{-4.0, 1.0, 7.2}, {-4.2, 0, 8.2}, 50, 1};
    const gd::DecalKindSpec& bs = gd::decal_spec(gd::DecalKind::blood);
    for (int k = 0; k < 6; ++k) {
      dv::Canvas c = dv::make_canvas(pw, ph, cam);
      dv::draw_mesh(c, mesh, dv::hall_paint);
      dv::draw_decal(c, gd::decal_verts(pool, static_cast<uint32_t>(slots[marks.size() - 2])),
                     gd::DecalKind::blood, 1, ages[k]);
      gl::Picture p = dv::finish(c, kSky);
      char b[32];
      std::snprintf(b, sizeof b, "%.0f S", ages[k]);
      dv::label(p, 6, 6, b);
      gl::blit(pic, p, pw * (k % 3), ph * (k / 3));
    }
    char cap[256];
    std::snprintf(cap, sizeof cap,
                  "One blood decal on the flagstones at six ages: wet and red when fresh, dried "
                  "brown and matte by %.0f s, fading over the last %.0f s of its %.0f s life.",
                  bs.dry_s, bs.fade_s, bs.life_s);
    lab.shot({"blood-ages",
              "BLOOD DRYING AND FADING",
              cap,
              "the gloss gone and the red turned brown by the third panel; the last panel "
              "nearly clear; the stone's own pattern unchanged under it",
              {}},
             pic);
  }
  {
    const int pw = w / 2, ph = pw * 5 / 8;
    gl::Picture pic = gl::create_picture(pw * 2, ph, kSky);
    for (int k = 0; k < 2; ++k) {
      const Close& cl = kCloses[k == 0 ? 0 : 2];
      dv::Canvas c = dv::make_canvas(pw, ph, cl.cam);
      draw_scene(c, mesh, pool, marks, slots, 0);
      for (size_t i = 0; i < marks.size(); ++i) {
        if (slots[i] < 0) continue;
        const auto v = gd::decal_verts(pool, static_cast<uint32_t>(slots[i]));
        for (size_t j = 0; j + 2 < v.size(); j += 3) {
          for (int e = 0; e < 3; ++e) {
            dv::draw_line3(c, v[j + e].pos, v[j + (e + 1) % 3].pos, {120, 220, 255, 200}, 1);
          }
        }
      }
      gl::Picture p = dv::finish(c, kSky);
      dv::label(p, 6, 6, cl.name);
      gl::blit(pic, p, pw * k, 0);
    }
    lab.shot({"clipped",
              "EACH DECAL'S OWN TRIANGLES",
              "The decals' meshes drawn over the surfaces they were clipped to: every receiver "
              "triangle in a decal's box cut to the box, so the mark follows the floor, the wall, "
              "the bevel's facets and the step's nose.",
              "the triangles end at the box's square and on every fold of the surface; none "
              "crosses empty space",
              {}},
             pic);
  }
  // The pool after the longest life: blood is freed, the rest stays.
  gd::expire(pool, gd::decal_spec(gd::DecalKind::blood).life_s);
  lab.metric("live-after-blood-life", gd::live_count(pool), "count");
}

const gl::LabSpec kSpec{
    "detail-decals",
    gl::LabKind::picture,
    "engine",
    "engine/detail/decals.hpp, engine/detail/receivers.hpp",
    "detail",
    "Do scorch marks, bullet holes and blood land on a brush-built hall's surfaces, wrap its "
    "corners and bevels without floating or breaking, and does blood dry and fade on time?",
    {{"width", "960", "a hall picture's width, px"}},
    "--width=240",
    run};
GE_LAB(kSpec);

}  // namespace
