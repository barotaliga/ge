// LAB `morph`: what the morph plugin makes of a low-polygon vertex-animated model. A lumpy body
// made here (a ten-by-seven globe pushed in and out by a smooth field, with a seam down its back
// and a second frame where it breathes) is written as a classic model file by the fixture writer,
// read back, and drawn three ways in each frame: flat, a normal a face (as the classic renderer
// lights it); smooth, the frame's own vertex normals (what the cache stage writes by default);
// and rounded, every triangle cut into nine on its point-normal patch (an option, off by
// default), with the triangles' edges over each.
//
// The numbers: the file's bytes, the vertices before and after the seam split, the triangles,
// how far the rounding moves a surface point at most (in units of the body's radius), and that
// the copies of a position on the seam come out with one normal. Everything is made here, so the
// pictures and the digest are the same everywhere; the player's own pack's models are drawn
// only by the classicenhance tool, into their cache.
//
// ge:summary Lab: a low-poly morph model drawn flat, smooth-shaded and rounded, two frames
// ge:keywords lab morph model smooth normals rounding pn-triangles seam frame flat wire
// ge:skill morph labs
// ge:test tests/morph_smooth_test.cpp
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "lab/lab.hpp"
#include "lab/model_view.hpp"
#include "lab/raster.hpp"
#include "morph/read.hpp"
#include "morph/smooth.hpp"
#include "morph/write.hpp"

namespace mo = ge::morph;
namespace lab = ge::lab;
namespace libm = ge::core::libm;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kLon = 10, kLat = 7;  // the globe's segments round and its rings top to bottom
constexpr float kRadius = 16;

// The body's radius in a direction, for frame `f`: a few smooth lumps, and a breath on frame 1.
float body_radius(double lon, double lat, int f) {
  const double lumps = 0.18 * libm::sin(3 * lon) * libm::cos(2 * lat) + 0.12 * libm::cos(5 * lat);
  return kRadius * float(1.0 + lumps + (f == 1 ? 0.08 * libm::cos(lat) : 0.0));
}

// The globe as a morph model: rows of vertices between the poles, the seam at longitude 0 (the
// rings' first vertex has a twin half the skin over, the one place the format lets a position read
// two texels), two frames.
mo::Model lumpy_body() {
  mo::Model m;
  m.skins.push_back({32, 16, std::vector<uint8_t>(512, 7), 0.0f, 0});
  // The file's own vertices: the two poles and (kLat - 1) rings of kLon.
  std::vector<double> lon_of, lat_of;
  auto add = [&](double lon, double lat) {
    lon_of.push_back(lon);
    lat_of.push_back(lat);
    return int(lon_of.size() - 1);
  };
  const int north = add(0, kPi / 2);
  std::vector<std::vector<int>> ring(size_t(kLat - 1));
  for (int r = 1; r < kLat; r++) {
    for (int i = 0; i < kLon; i++)
      ring[size_t(r - 1)].push_back(add(2 * kPi * i / kLon, kPi / 2 - kPi * r / kLat));
  }
  const int south = add(0, -kPi / 2);
  // A view vertex for (file vertex, its texel column); the seam's twin is column 16.
  auto vertex = [&](int source, int column) {
    const float u = (float(column) + 0.5f) / 32.0f;
    for (uint32_t v = 0; v < m.vertex_count(); v++) {
      if (m.source_vertex[v] == source && m.st[size_t{v} * 2] == u) return v;
    }
    m.source_vertex.push_back(source);
    m.st.push_back(u);
    m.st.push_back(float(0.5 - lat_of[size_t(source)] / kPi));
    return m.vertex_count() - 1;
  };
  auto u_of = [&](int i) { return i < kLon ? i : 16; };
  auto at = [&](int r, int i) { return ring[size_t(r)][size_t(i % kLon)]; };
  auto tri = [&](uint32_t a, uint32_t b, uint32_t c) {
    m.indices.insert(m.indices.end(), {a, b, c});
  };
  for (int i = 0; i < kLon; i++) {
    tri(vertex(north, 5), vertex(at(0, i), u_of(i)), vertex(at(0, i + 1), u_of(i + 1)));
    for (int r = 0; r + 1 < kLat - 1; r++) {
      const uint32_t a = vertex(at(r, i), u_of(i)), b = vertex(at(r, i + 1), u_of(i + 1));
      const uint32_t c = vertex(at(r + 1, i), u_of(i)), d = vertex(at(r + 1, i + 1), u_of(i + 1));
      tri(a, c, d);
      tri(a, d, b);
    }
    tri(vertex(south, 5), vertex(at(kLat - 2, i + 1), u_of(i + 1)),
        vertex(at(kLat - 2, i), u_of(i)));
  }
  for (int f = 0; f < 2; f++) {
    mo::Frame fr;
    fr.name = f == 0 ? "rest" : "breathe";
    fr.group = f;
    for (uint32_t v = 0; v < m.vertex_count(); v++) {
      const double lon = lon_of[size_t(m.source_vertex[v])],
                   lat = lat_of[size_t(m.source_vertex[v])];
      const float r = body_radius(lon, lat, f);
      fr.positions.push_back(r * float(libm::cos(lat) * libm::cos(lon)));
      fr.positions.push_back(r * float(libm::cos(lat) * libm::sin(lon)));
      fr.positions.push_back(r * float(libm::sin(lat)));
    }
    m.frames.push_back(std::move(fr));
  }
  m.radius = kRadius * 1.3f;
  return m;
}

// A frame as the labs' shape: z up becomes y up, a unit a centimetre.
lab::Shape shape_of(const mo::Model& m, size_t frame, bool flat) {
  lab::Shape s;
  const std::vector<float>& p = m.frames[frame].positions;
  const std::vector<float> n = mo::frame_normals(m, m.frames[frame]);
  auto put = [&](const float* q, std::vector<float>& out) {
    out.insert(out.end(), {q[0] * 0.01f, q[2] * 0.01f, -q[1] * 0.01f});
  };
  auto put_n = [&](const float* q, std::vector<float>& out) {
    out.insert(out.end(), {q[0], q[2], -q[1]});
  };
  for (size_t t = 0; t + 2 < m.indices.size(); t += 3) {
    float face[3] = {0, 0, 0};
    if (flat) {
      const float* a = &p[size_t(m.indices[t]) * 3];
      const float* b = &p[size_t(m.indices[t + 1]) * 3];
      const float* c = &p[size_t(m.indices[t + 2]) * 3];
      const float e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
      const float e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
      face[0] = e1[1] * e2[2] - e1[2] * e2[1];
      face[1] = e1[2] * e2[0] - e1[0] * e2[2];
      face[2] = e1[0] * e2[1] - e1[1] * e2[0];
      const float len = std::sqrt(face[0] * face[0] + face[1] * face[1] + face[2] * face[2]);
      for (float& v : face) v /= len > 0 ? len : 1.0f;
    }
    for (size_t k = 0; k < 3; k++) {
      const size_t v = m.indices[t + k];
      put(&p[v * 3], s.pos);
      put_n(flat ? face : &n[v * 3], s.nrm);
      s.part.push_back(0);
      s.top.push_back(0);
      s.idx.push_back(uint32_t(s.idx.size()));
    }
  }
  return s;
}

void run(lab::Lab& lab) {
  const mo::Model made = lumpy_body();
  const std::vector<uint8_t> bytes = mo::write_model(made);
  mo::Model m;
  std::string error;
  if (!mo::read_model(bytes, "lumpy.mdl", &m, &error)) {
    lab.fail(error, "next: the writer and the reader disagree; run tests/morph_read_test.cpp");
    return;
  }
  mo::SmoothOptions round;
  round.level = 3;
  round.crease_deg = 75;
  const mo::Model r = mo::smooth_model(m, round);
  int sources = 0;
  for (const int s : m.source_vertex) sources = std::max(sources, s + 1);
  lab.metric("file-bytes", double(bytes.size()), "bytes");
  lab.metric("file-vertices", sources, "count");
  lab.metric("vertices", m.vertex_count(), "count");
  lab.metric("triangles", static_cast<double>(m.indices.size()) / 3.0, "count");
  lab.metric("rounded-triangles", static_cast<double>(r.indices.size()) / 3.0, "count");
  // How far rounding pushes the surface out: the widest radius of the rounded frame over the
  // widest of the original's, in units of the body's radius.
  double most_flat = 0, most_round = 0;
  for (size_t v = 0; v < m.vertex_count(); v++) {
    const float* q = &m.frames[0].positions[v * 3];
    most_flat = std::max(most_flat, double(std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2])));
  }
  for (size_t v = 0; v < r.vertex_count(); v++) {
    const float* q = &r.frames[0].positions[v * 3];
    most_round = std::max(most_round, double(std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2])));
  }
  lab.metric("rounding-reach", (most_round - most_flat) / kRadius, "radius");
  // The seam's copies share one normal.
  const std::vector<float> n = mo::frame_normals(m, m.frames[0]);
  int split = 0;
  for (size_t a = 0; a < m.vertex_count(); a++) {
    for (size_t b = a + 1; b < m.vertex_count(); b++) {
      if (m.source_vertex[a] != m.source_vertex[b]) continue;
      split++;
      if (n[a * 3] != n[b * 3] || n[a * 3 + 1] != n[b * 3 + 1] || n[a * 3 + 2] != n[b * 3 + 2]) {
        lab.fail("the seam's copies of vertex " + std::to_string(m.source_vertex[a]) +
                     " came out with different normals",
                 "next: frame_normals must sum over source_vertex (plugins/morph/smooth.cpp)");
      }
    }
  }
  lab.metric("seam-copies", split, "count");

  ge::assets::Model look;
  look.materials.push_back({"body", {0.55f, 0.42f, 0.33f}});
  constexpr int kW = 300, kH = 300;
  for (size_t f = 0; f < m.frames.size(); f++) {
    lab::Picture pic = lab::create_picture(kW * 3, kH, {24, 26, 32});
    const lab::Shape shapes[3] = {shape_of(m, f, true), shape_of(m, f, false),
                                  shape_of(r, f, false)};
    const lab::ViewCam cam = lab::orbit_cam(lab::shape_box(shapes[2]), 35, 20);
    for (int i = 0; i < 3; i++) {
      lab::ViewStyle style;
      style.wire = true;
      style.floor = false;
      lab::draw_shape(pic, i * kW, 0, kW, kH, cam, look, shapes[i], style);
    }
    const std::string name = m.frames[f].name;
    lab.shot({"body-" + name,
              "A LUMPY LOW-POLY BODY, FRAME '" + name + "': FLAT, SMOOTH, ROUNDED",
              "Left: a normal a face, as the classic renderer lights it. Middle: the frame's own "
              "vertex normals, the cache's default. Right: every triangle cut into nine on its "
              "point-normal patch (an option, off by default).",
              "the middle hides the facets without moving a vertex; no seam line down the back; "
              "the right's silhouette rounds but keeps the lumps where they were",
              {{"frame", name}, {"level", "3"}, {"crease", "75"}}},
             pic);
  }
}

const lab::LabSpec kSpec{
    "morph",
    lab::LabKind::picture,
    "plugin:morph",
    "plugins/morph",
    "morph",
    "What does a low-polygon morph model look like flat, with smooth normals and rounded, frame "
    "by frame, and does its seam vanish?",
    {},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
