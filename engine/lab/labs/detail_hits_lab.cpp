// LAB `detail-hits`: the marks a fight's events leave (engine/detail/hits.hpp) in the bevelled
// test hall, so a change to where a shot's hole goes or how much a wound bleeds shows as
// pictures: a shotgun's pellets and a burst from a rifle with their rays drawn from the gun;
// one victim hit four times as hard (a twentieth of the health, a quarter, a half, all of it),
// each in a hall of its own, with the blood each left on the wall behind and the floor; and a
// fresh scorch's embers cooling through their first seconds. With how many marks each event
// pressed and what an event costs.
//
// ge:summary Lab: shots' holes and wounds' blood from game events, and a scorch's embers cooling
// ge:keywords lab detail hit shot bullet-hole shotgun pellet wound bleed blood damage health ember
// ge:skill detail labs
// ge:test tests/detail_hits_test.cpp
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/prng.hpp"
#include "detail/decals.hpp"
#include "detail/hits.hpp"
#include "detail/test_hall.hpp"
#include "detail_view.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"

namespace {

namespace gl = ge::lab;
namespace gd = ge::detail;
namespace dv = ge::lab::detail_view;

constexpr gl::Rgba kSky{38, 42, 52};
constexpr gl::Rgba kRay{255, 214, 90, 150};
constexpr gl::Rgba kVictim{120, 220, 255, 220};

// Every live decal in the pool drawn at its age at `now_s`.
void draw_pool(dv::Canvas& c, const gd::DecalPool& pool, double now_s) {
  for (uint32_t i = 0; i < pool.capacity; ++i) {
    const gd::Decal& d = pool.decals[i];
    if (!d.live) continue;
    dv::draw_decal(c, gd::decal_verts(pool, i), d.kind, d.variant, now_s - d.born_s);
  }
}

struct Ray {
  gd::Vec3 from, to;
};

// A gun at `from` aimed at `aim`: `pellets` rounds in a cone of `spread_rad`, seeds from `seed`.
void fire(gd::DecalPool& pool, const gd::Receivers& rec, const gd::Vec3& from, const gd::Vec3& aim,
          int pellets, double spread_rad, uint32_t seed, std::vector<Ray>& rays, int& hits) {
  ge::core::Rng rng = ge::core::make_rng(seed);
  const gd::Vec3 dir = gd::normalized(gd::sub(aim, from));
  const gd::Vec3 u = gd::any_perpendicular(dir), v = gd::cross(dir, u);
  for (int i = 0; i < pellets; ++i) {
    const double a = ge::core::range(rng, 0, ge::core::kTau);
    const double off = spread_rad * std::sqrt(ge::core::next(rng));
    const double so = ge::core::libm::sin(off);
    const gd::Vec3 d = gd::normalized(gd::add(
        gd::mul(dir, ge::core::libm::cos(off)),
        gd::add(gd::mul(u, ge::core::libm::cos(a) * so), gd::mul(v, ge::core::libm::sin(a) * so))));
    gd::ShotAt s;
    s.from = from;
    s.dir = d;
    s.seed = seed * 31 + static_cast<uint32_t>(i);
    const gd::ShotResult r = gd::shot_hit(pool, rec, s);
    if (r.slot >= 0) ++hits;
    rays.push_back({from, r.hit ? r.at : gd::mad(from, d, 12)});
  }
}

const dv::Cam kShotCam{{-0.6, 2.2, 3.2}, {-1.2, 1.2, 9.0}, 62, 5};

// The victim of the wound panels: where he stands, where he is struck, the gun behind the shot.
constexpr gd::Vec3 kVictimFeet{2.2, 0, 8.7};
constexpr gd::Vec3 kWound{2.2, 1.3, 8.7};
constexpr gd::Vec3 kGun{1.3, 1.5, 4.2};
const dv::Cam kWoundCam{{-0.4, 1.9, 6.0}, {2.4, 0.8, 9.4}, 58, 5};

void run(gl::Lab& lab) {
  const int w = static_cast<int>(lab.arg_num("width"));
  const gd::TestHall hall = gd::test_hall();
  gd::BevelSpec spec;
  spec.radius_m = 0.05;
  const gd::TriMesh mesh = dv::mesh_of(dv::bevelled(hall.solids, spec));
  const gd::Receivers rec = gd::bin_receivers(mesh);

  {
    // Two shotgun blasts at the back wall, a rifle's burst walked across the left pillar and the
    // floor before it.
    gd::DecalPool pool = gd::make_pool(64);
    std::vector<Ray> rays;
    int hits = 0;
    const auto t0 = std::chrono::steady_clock::now();
    fire(pool, rec, {-0.8, 1.5, 4.0}, {-1.9, 1.6, 10}, 12, 0.07, 1, rays, hits);
    fire(pool, rec, {-0.8, 1.5, 4.0}, {-4.0, 1.1, 10}, 12, 0.07, 2, rays, hits);
    for (int i = 0; i < 8; ++i) {
      const double t = i / 7.0;
      fire(pool, rec, {-0.8, 1.5, 4.0}, {-3.4 + 0.5 * t, 1.6 - 1.6 * t, 6.0 + 0.6 * t}, 1, 0.01,
           10 + static_cast<uint32_t>(i), rays, hits);
    }
    const auto t1 = std::chrono::steady_clock::now();
    lab.metric("shots-fired", static_cast<double>(rays.size()), "count");
    lab.metric("holes-pressed", hits, "count");
    lab.metric("shot-us-each",
               std::chrono::duration<double, std::micro>(t1 - t0).count() /
                   static_cast<double>(rays.size()),
               "us", gl::Det::measured);
    const int pw = w / 2;
    gl::Picture pic = gl::create_picture(pw * 2, pw * 9 / 16, kSky);
    for (int k = 0; k < 2; ++k) {
      dv::Canvas c = dv::make_canvas(pw, pw * 9 / 16, kShotCam);
      dv::draw_mesh(c, mesh, dv::hall_paint);
      draw_pool(c, pool, 0);
      if (k == 1) {
        for (const Ray& r : rays) dv::draw_line3(c, r.from, r.to, kRay, 1);
      }
      gl::Picture p = dv::finish(c, kSky);
      dv::label(p, 6, 6, k == 0 ? "THE HOLES" : "AND EVERY ROUND'S RAY");
      gl::blit(pic, p, pw * k, 0);
    }
    lab.shot({"shots",
              "SHOTS LANDING: SHOTGUN BLASTS AND A RIFLE'S BURST",
              "Two shotgun blasts of twelve pellets at the back wall and an eight-round burst "
              "walked down the left pillar onto the floor, each round cast from the gun and its "
              "hole pressed where it struck (left), with every round's ray drawn from the gun "
              "(right).",
              "every hole at the end of a ray; the pellets in a loose disc on the wall; the "
              "burst's holes on the pillar's facets and then the floor, none floating",
              {}},
             pic);
  }
  {
    // One victim, four hits of rising damage, each into a clean hall.
    const double lost[] = {5, 25, 50, 100};
    gl::LabTable t{"The blood one wound leaves, by the health it took (of 100)",
                   {"health lost", "share", "spatters", "drops", "decals pressed"},
                   {}};
    const int pw = w / 2, ph = pw * 9 / 16;
    gl::Picture pic = gl::create_picture(pw * 2, ph * 2, kSky);
    double us = 0;
    for (int k = 0; k < 4; ++k) {
      gd::DecalPool pool = gd::make_pool(32);
      gd::WoundAt wnd;
      wnd.at = kWound;
      wnd.shot_dir = gd::sub(kWound, kGun);
      wnd.health_lost = lost[k];
      wnd.max_health = 100;
      wnd.seed = 7;
      const auto t0 = std::chrono::steady_clock::now();
      const gd::BleedReport r = gd::bleed(pool, rec, wnd);
      us +=
          std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
      char share[16];
      std::snprintf(share, sizeof share, "%.2f", r.share);
      t.rows.push_back({std::to_string(static_cast<int>(lost[k])), share,
                        std::to_string(r.spatters), std::to_string(r.drops),
                        std::to_string(r.pressed)});
      lab.metric("blood-pressed-" + std::to_string(static_cast<int>(lost[k])), r.pressed, "count");
      dv::Canvas c = dv::make_canvas(pw, ph, kWoundCam);
      dv::draw_mesh(c, mesh, dv::hall_paint);
      draw_pool(c, pool, 0);
      // Where the victim stood and the shot's line through him.
      dv::draw_line3(c, kVictimFeet, {kVictimFeet.x, 1.8, kVictimFeet.z}, kVictim, 2);
      dv::draw_line3(c, kGun, kWound, kRay, 1);
      gl::Picture p = dv::finish(c, kSky);
      char b[48];
      std::snprintf(b, sizeof b, "%d OF 100 HEALTH LOST", static_cast<int>(lost[k]));
      dv::label(p, 6, 6, b);
      gl::blit(pic, p, pw * (k % 2), ph * (k / 2));
    }
    lab.table(t);
    lab.metric("bleed-us-each", us / 4, "us", gl::Det::measured);
    lab.shot({"wounds",
              "BLOOD BY THE HEALTH A HIT TOOK",
              "One victim (the blue post) shot from the same gun (the yellow line) four times as "
              "hard, each in a clean hall: a twentieth of his health, a quarter, a half and all "
              "of it. The spatter carries on along the shot onto the wall behind him, the pool "
              "gathers on the floor under the wound and drops fall round his feet, all growing "
              "with the share of health lost.",
              "a spot or two for the graze; the wall behind painted and a pool a metre across "
              "for the full hit; every mark on a surface, on the far side of the victim from "
              "the gun",
              {}},
             pic);
  }
  {
    // A fresh scorch's embers cooling.
    const double ages[] = {0, 0.25, 0.5, 0.8, 1.2, 5};
    const int pw = w / 3, ph = pw * 3 / 4;
    gl::Picture pic = gl::create_picture(pw * 3, ph * 2, kSky);
    gd::DecalPool pool = gd::make_pool(1);
    gd::DecalAt d;
    d.kind = gd::DecalKind::scorch;
    d.at = {1.2, 0, 2.2};
    d.size_m = 1.4;
    d.spin_rad = 0.6;
    d.variant = 2;
    gd::place_decal(pool, rec, d);
    const dv::Cam cam{{0.2, 1.4, 0.6}, {1.2, 0, 2.2}, 52, 1};
    dv::Light dim;
    dim.key = 0.3;
    dim.lamp_power = 0.8;
    dim.ambient = 0.1;
    const gd::DecalKindSpec& ss = gd::decal_spec(gd::DecalKind::scorch);
    for (int k = 0; k < 6; ++k) {
      dv::Canvas c = dv::make_canvas(pw, ph, cam, dim);
      dv::draw_mesh(c, mesh, dv::hall_paint);
      draw_pool(c, pool, ages[k]);
      gl::Picture p = dv::finish(c, kSky);
      char b[32];
      std::snprintf(b, sizeof b, "%.2f S", ages[k]);
      dv::label(p, 6, 6, b);
      gl::blit(pic, p, pw * (k % 3), ph * (k / 3));
    }
    char cap[256];
    std::snprintf(cap, sizeof cap,
                  "A blast's scorch on the flagstones in dim light at six ages: its embers "
                  "white-orange at the blast, cooling through red, out by %.1f s, leaving soot.",
                  ss.ember_s);
    lab.shot({"embers",
              "A FRESH SCORCH'S EMBERS COOLING",
              cap,
              "the glow brightest in the mark's core and gone by the last panel; the soot the "
              "same in every panel",
              {}},
             pic);
  }
}

const gl::LabSpec kSpec{
    "detail-hits",
    gl::LabKind::picture,
    "engine",
    "engine/detail/hits.hpp, engine/detail/decals.hpp",
    "detail",
    "Does a shot leave its hole where its ray lands, does a wound bleed in proportion to the "
    "health it took onto the surfaces behind and under the victim, and do a scorch's embers cool "
    "on time?",
    {{"width", "960", "a picture's width, px"}},
    "--width=240",
    run};
GE_LAB(kSpec);

}  // namespace
