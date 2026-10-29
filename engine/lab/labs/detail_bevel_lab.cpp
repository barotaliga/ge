// LAB `detail-bevel`: the bevel pass (engine/detail/bevel.hpp) on the test hall, so a change to
// which edges are cut, and how, shows as pictures and counts: the hall sharp and bevelled side by
// side; close-ups of a pillar on its plinth, a flight of steps, a doorway's jambs and lintel, a
// block in a corner and a basin's kerb, sharp against chamfered against rounded; every edge drawn
// in its verdict's colour (rounded, buried against a neighbour, thin, flat); and the counts and
// the cost of the pass.
//
// ge:summary Lab: the test hall's edges bevelled: before and after, close-ups, verdicts, cost
// ge:keywords lab detail bevel chamfer round edge verdict buried close-up hall cost
// ge:skill detail labs
// ge:test tests/detail_bevel_test.cpp
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "detail/bevel.hpp"
#include "detail/test_hall.hpp"
#include "detail_view.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"

namespace {

namespace gl = ge::lab;
namespace gd = ge::detail;
namespace dv = ge::lab::detail_view;

constexpr gl::Rgba kSky{38, 42, 52};

std::string fixed(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

using dv::bevelled;
using dv::label;
using dv::mesh_of;

gl::Picture view(const gd::TriMesh& m, const dv::Cam& cam, int w, int h) {
  dv::Canvas c = dv::make_canvas(w, h, cam);
  dv::draw_mesh(c, m, dv::hall_paint);
  return dv::finish(c, kSky);
}

struct CloseUp {
  const char* name;
  dv::Cam cam;
};

const dv::Cam kHallCam{{0.4, 2.3, -2.6}, {0.6, 1.0, 6.0}, 58, 5};

const CloseUp kCloseUps[] = {
    {"PILLAR ON ITS PLINTH", {{1.4, 1.3, 3.7}, {3.0, 0.6, 6.0}, 50, 1}},
    {"STEPS TO THE DAIS", {{5.2, 1.1, 5.3}, {4.5, 0.3, 8.4}, 50, 1}},
    {"DOORWAY AND LINTEL", {{-0.9, 1.6, 7.7}, {0.3, 2.5, 9.8}, 56, 1}},
    {"BLOCK IN THE CORNER", {{-3.4, 1.5, 6.9}, {-5.3, 0.55, 9.4}, 50, 1}},
    {"THE BASIN'S KERB", {{-0.6, 1.3, -0.4}, {-3.2, 0.0, 2.2}, 52, 1}},
};

void before_after(gl::Lab& lab, const gd::TestHall& hall, const gd::BevelSpec& spec, int w) {
  const int h = w * 9 / 16;
  gl::Picture pic = gl::create_picture(w * 2, h, kSky);
  gl::Picture a = view(mesh_of(hall.solids), kHallCam, w, h);
  gl::Picture b = view(mesh_of(bevelled(hall.solids, spec)), kHallCam, w, h);
  gl::blit(pic, a, 0, 0);
  gl::blit(pic, b, w, 0);
  label(pic, 8, 8, "SHARP");
  label(pic, w + 8, 8,
        "BEVELLED R " + fixed(spec.radius_m * 100, 1) + " CM, " + std::to_string(spec.segments) +
            " FACETS");
  lab.shot({"hall",
            "THE TEST HALL, SHARP AND BEVELLED",
            "The brush-built test hall lit by a key light and a warm lamp, its brushes as they "
            "are (left) and with every exposed edge rounded by the bevel pass (right).",
            "every arris a pillar, a step, a jamb or the kerb shows catches a highlight on the "
            "right; walls standing on the floor and blocks meeting flush show no groove",
            {{"radius", lab.arg("radius")}, {"segments", lab.arg("segments")}}},
           pic);
}

void close_ups(gl::Lab& lab, const gd::TestHall& hall, const gd::BevelSpec& spec, int w) {
  const int pw = w / 2, ph = pw * 5 / 8;
  const int rows = static_cast<int>(std::size(kCloseUps));
  gl::Picture pic = gl::create_picture(pw * 3, ph * rows, kSky);
  gd::BevelSpec chamfer = spec;
  chamfer.segments = 1;
  const gd::TriMesh meshes[3] = {mesh_of(hall.solids), mesh_of(bevelled(hall.solids, chamfer)),
                                 mesh_of(bevelled(hall.solids, spec))};
  const char* cols[3] = {"SHARP", "CHAMFER", "ROUND"};
  for (int r = 0; r < rows; ++r) {
    for (int k = 0; k < 3; ++k) {
      gl::blit(pic, view(meshes[k], kCloseUps[r].cam, pw, ph), pw * k, ph * r);
      label(pic, pw * k + 6, ph * r + 6, cols[k]);
    }
  }
  for (int r = 0; r < rows; ++r) label(pic, 6, ph * r + ph - 28, kCloseUps[r].name);
  lab.shot({"close-ups",
            "CLOSE-UPS: SHARP, CHAMFERED, ROUNDED",
            "Five corners of the hall close up, each three ways: the brushes as they are, every "
            "exposed edge chamfered (one facet), and rounded (" +
                lab.arg("segments") + " facets) at a radius of " + lab.arg("radius") + " m.",
            "a lit sliver along every outside edge in the middle and right columns; the step "
            "under a step, the jamb against the wall and the lintel on the jambs meet sharp, with "
            "no notch",
            {{"radius", lab.arg("radius")}, {"segments", lab.arg("segments")}}},
           pic);
}

gl::Rgba verdict_colour(gd::EdgeVerdict v) {
  switch (v) {
    case gd::EdgeVerdict::bevelled:
      return {90, 230, 110};
    case gd::EdgeVerdict::buried:
      return {240, 70, 60};
    case gd::EdgeVerdict::thin:
      return {250, 210, 60};
    case gd::EdgeVerdict::flat:
      return {90, 150, 255};
    case gd::EdgeVerdict::knife:
      return {230, 90, 230};
  }
  return {255, 255, 255};
}

void verdicts(gl::Lab& lab, const gd::TestHall& hall, const std::vector<gd::BevelResult>& r,
              int w) {
  const int h = w * 9 / 16;
  dv::Canvas c = dv::make_canvas(w, h, kHallCam);
  dv::draw_mesh(c, mesh_of(hall.solids), dv::hall_paint);
  for (const gd::BevelResult& b : r) {
    for (const gd::BevelEdge& e : b.edges)
      dv::draw_line3(c, e.p0, e.p1, verdict_colour(e.verdict), 2);
  }
  gl::Picture pic = dv::finish(c, kSky);
  int y = 8;
  for (gd::EdgeVerdict v : {gd::EdgeVerdict::bevelled, gd::EdgeVerdict::buried,
                            gd::EdgeVerdict::thin, gd::EdgeVerdict::flat}) {
    const int scale = w >= 600 ? 2 : 1;
    gl::fill_rect(pic, 8, y, 14 * scale, 7 * scale, verdict_colour(v));
    label(pic, 8 + 18 * scale, y - 3, std::string(gd::verdict_name(v)));
    y += 12 * scale;
  }
  lab.shot({"verdicts",
            "EVERY EDGE BY ITS VERDICT",
            "Every edge of every brush in the hall, drawn over the sharp hall in the colour of "
            "what the bevel pass decided: rounded (green), buried against a neighbour so left "
            "sharp (red), too thin for the radius (yellow), folding too little (blue).",
            "green on every outside corner; red where a wall stands on the floor, where floor "
            "pieces meet round the basin, along the lintel's back and under each step",
            {{"radius", lab.arg("radius")}}},
           pic);
}

void run(gl::Lab& lab) {
  const gd::TestHall hall = gd::test_hall();
  gd::BevelSpec spec;
  spec.radius_m = lab.arg_num("radius");
  spec.segments = static_cast<int>(lab.arg_num("segments"));
  const int w = static_cast<int>(lab.arg_num("width"));
  std::vector<gd::BevelResult> results;
  const auto t0 = std::chrono::steady_clock::now();
  const std::vector<gd::Solid> round = bevelled(hall.solids, spec, &results);
  const auto t1 = std::chrono::steady_clock::now();
  int counts[5] = {};
  size_t planes = 0;
  for (const gd::BevelResult& b : results) {
    for (const gd::BevelEdge& e : b.edges) ++counts[static_cast<int>(e.verdict)];
    planes += b.solid.planes.size();
  }
  gl::LabTable t{"The hall's edges by verdict, and what the pass made", {"what", "count"}, {}};
  for (int v = 0; v < 5; ++v) {
    t.rows.push_back({std::string(gd::verdict_name(static_cast<gd::EdgeVerdict>(v))),
                      std::to_string(counts[v])});
    lab.metric(
        std::string("edges-") + std::string(gd::verdict_name(static_cast<gd::EdgeVerdict>(v))),
        counts[v], "count");
  }
  const gd::TriMesh sharp_m = mesh_of(hall.solids), round_m = mesh_of(round);
  t.rows.push_back({"brushes", std::to_string(hall.solids.size())});
  t.rows.push_back({"planes after", std::to_string(planes)});
  t.rows.push_back({"triangles sharp", std::to_string(sharp_m.triangles())});
  t.rows.push_back({"triangles bevelled", std::to_string(round_m.triangles())});
  lab.table(t);
  lab.metric("triangles-sharp", sharp_m.triangles(), "count");
  lab.metric("triangles-bevelled", round_m.triangles(), "count");
  lab.metric("bevel-ms", std::chrono::duration<double, std::milli>(t1 - t0).count(), "ms",
             gl::Det::measured);
  before_after(lab, hall, spec, w);
  close_ups(lab, hall, spec, w);
  verdicts(lab, hall, results, w);
}

const gl::LabSpec kSpec{
    "detail-bevel",
    gl::LabKind::picture,
    "engine",
    "engine/detail/bevel.hpp, engine/detail/solid.hpp",
    "detail",
    "Does the bevel pass round every exposed sharp edge of a brush-built hall, and keep sharp "
    "the edges buried against a neighbour, too thin for the radius or folding too little?",
    {{"radius", "0.05", "the round's radius, m"},
     {"segments", "3", "the round's facets (1: a chamfer)"},
     {"width", "960", "a hall picture's width, px (the close-ups are half of it a panel)"}},
    "--width=240",
    run};
GE_LAB(kSpec);

}  // namespace
