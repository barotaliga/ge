// The model sheets: turntable, axes with a ruler, topology, details, LODs and strips.
//
// ge:summary The model sheets: turntable, axes, topology, details, LODs, strips
// ge:keywords lab model-sheets turntable axes topology detail lod strip
// ge:skill labs
// ge:test tests/lab_model_view_test.cpp
#include "lab/model_sheets.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

#include "core/libm/libm.hpp"

namespace ge::lab {

namespace {

namespace a = assets;

constexpr Rgba kSheet{24, 26, 32};
constexpr Rgba kText{226, 230, 238};
constexpr Rgba kDim{150, 156, 170};
constexpr Rgba kRuler{255, 210, 60};
constexpr int kBand = 14;  // a panel's caption above it

std::string fixed(double v, int places) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", places, v);
  return b;
}

const char* side_name(double az) {
  static const char* const kNames[8] = {"FRONT", "FRONT RIGHT", "RIGHT", "BACK RIGHT",
                                        "BACK",  "BACK LEFT",   "LEFT",  "FRONT LEFT"};
  const double turn = std::fmod(std::fmod(az, 360.0) + 360.0 + 22.5, 360.0);
  return kNames[static_cast<int>(turn / 45.0) % 8];
}

}  // namespace

Picture turntable_sheet(const a::Model& m, const Shape& s, double floor_y, int panel, int count,
                        double el, double fov) {
  count = std::clamp(count, 1, 24);
  const int cols = std::min(count, 4), rows = (count + cols - 1) / cols;
  Picture pic = create_picture(cols * panel, rows * (panel + kBand), kSheet);
  const a::Box box = shape_box(s);
  ViewStyle style;
  for (int k = 0; k < count; ++k) {
    const double az = 360.0 * k / count;
    const int x = (k % cols) * panel, y = (k / cols) * (panel + kBand);
    draw_shape(pic, x, y + kBand, panel, panel, orbit_cam(box, az, el, fov), m, s, style, floor_y);
    draw_text(pic, x + 4, y + 3, "AZ " + fixed(az, 0) + "  " + side_name(az), kText);
  }
  return pic;
}

Picture axes_sheet(const a::Model& m, const Shape& s, double floor_y, int panel) {
  static const char* const kSides[4] = {"front", "right", "back", "top"};
  Picture pic = create_picture(4 * panel, panel + kBand, kSheet);
  const a::Box box = shape_box(s);
  ViewStyle style;
  style.floor = false;
  for (int k = 0; k < 4; ++k) {
    const ViewCam cam = axis_cam(box, kSides[k]);
    const int x0 = k * panel;
    draw_shape(pic, x0, kBand, panel, panel, cam, m, s, style, floor_y);
    std::string title = kSides[k];
    for (char& c : title) c = static_cast<char>(c - 32);
    draw_text(pic, x0 + 4, 3, title, kText);
    // The ruler: up the panel's left from the box's floor (above: from its back), a tick every
    // 10 cm, longer every 50 cm and the metres numbered.
    const double px_m = (panel / 2.0) / cam.half_m;
    const bool top = k == 3;
    const double lo = top ? box.min[2] : box.min[1], hi = top ? box.max[2] : box.max[1];
    const double mid = top ? cam.target[2] : cam.target[1];
    const double y_lo = kBand + panel / 2.0 + (mid - lo) * px_m;
    const double y_hi = kBand + panel / 2.0 + (mid - hi) * px_m;
    draw_line(pic, x0 + 8, y_lo, x0 + 8, y_hi, kRuler);
    for (int t = 0; t * 0.1 <= hi - lo + 1e-9; ++t) {
      const double y = y_lo - t * 0.1 * px_m;
      const int len = t % 10 == 0 ? 10 : t % 5 == 0 ? 7 : 4;
      draw_line(pic, x0 + 8, y, x0 + 8 + len, y, kRuler);
      if (t % 10 == 0 && t > 0)
        draw_text(pic, x0 + 20, y - 3, std::to_string(t / 10) + " M", kRuler);
    }
    const std::string size = (top ? "DEPTH " : "HEIGHT ") + fixed(hi - lo, 2) + " M";
    draw_text(pic, x0 + panel - text_width(size) - 4, 3, size, kDim);
  }
  return pic;
}

Picture topology_sheet(const a::Model& m, const Shape& s, double floor_y, int panel) {
  Picture pic = create_picture(2 * panel, panel + kBand, kSheet);
  const a::Box box = shape_box(s);
  ViewStyle wire;
  wire.wire = true;
  wire.edge = {255, 255, 255, 110};
  draw_shape(pic, 0, kBand, panel, panel, orbit_cam(box, 35, 12), m, s, wire, floor_y);
  draw_text(pic, 4, 3, "SHADED, EVERY EDGE", kText);
  ViewStyle bones;
  bones.shade = true;
  bones.wire = true;
  bones.edge = {255, 255, 255, 30};
  bones.bones = true;
  bones.labels = true;
  draw_shape(pic, panel, kBand, panel, panel, orbit_cam(box, 215, 12), m, s, bones, floor_y);
  draw_text(pic, panel + 4, 3, "SKELETON, FROM BEHIND LEFT", kText);
  return pic;
}

std::vector<int> detail_joints(const a::Model& m, const std::string& names, int most,
                               std::string* missing) {
  std::vector<int> out;
  if (!names.empty()) {
    size_t at = 0;
    while (at <= names.size()) {
      const size_t comma = names.find(',', at);
      const std::string n =
          names.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
      if (!n.empty()) {
        const int j = a::find_joint(m, n);
        if (j < 0) {
          *missing += (missing->empty() ? "" : ", ") + n;
        } else {
          out.push_back(j);
        }
      }
      if (comma == std::string::npos) break;
      at = comma + 1;
    }
    return out;
  }
  // The skeleton's ends first, then every joint whose own skin is small (3 to 35 cm round): one
  // side of a mirrored pair (a `_r` whose `_l` is taken is left out), at most `most`.
  const Shape rest = shape_at(m, -1, 0);
  std::vector<int> children(m.joints.size(), 0);
  for (const a::Joint& j : m.joints) {
    if (j.parent >= 0) ++children[static_cast<size_t>(j.parent)];
  }
  auto mirrored = [&](int j) {
    const std::string& n = m.joints[static_cast<size_t>(j)].name;
    if (n.size() < 2 || n.compare(n.size() - 2, 2, "_r") != 0) return false;
    const int l = a::find_joint(m, n.substr(0, n.size() - 2) + "_l");
    return l >= 0 && std::find(out.begin(), out.end(), l) != out.end();
  };
  for (int pass = 0; pass < 2; ++pass) {
    for (size_t j = 0; j < m.joints.size() && static_cast<int>(out.size()) < most; ++j) {
      const int c = static_cast<int>(j);
      if (std::find(out.begin(), out.end(), c) != out.end() || mirrored(c)) continue;
      if (m.joints[j].parent < 0 || (pass == 0 && children[j] != 0)) continue;
      if (pass == 1) {
        float centre[3], radius = 0;
        if (!region_of(m, rest, c, 0, centre, &radius) || radius < 0.03f || radius > 0.35f) {
          continue;
        }
      }
      out.push_back(c);
    }
  }
  return out;
}

bool detail_region(const a::Model& m, const Shape& s, int joint, float centre[3], float* radius) {
  if (joint < 0 || static_cast<size_t>(joint) >= s.bones.size()) return false;
  if (region_of(m, s, joint, 0, centre, radius) && *radius >= 0.03f && *radius <= 0.35f) {
    *radius *= 1.1f;
    return true;
  }
  const float* g = s.bones[static_cast<size_t>(joint)].m;
  centre[0] = g[3];
  centre[1] = g[7];
  centre[2] = g[11];
  *radius = 0.15f;
  return true;
}

Picture detail_sheet(const a::Model& m, const Shape& s, std::span<const int> joints, int panel) {
  const int n = static_cast<int>(joints.size());
  const int cols = 4, rows = std::max(1, (2 * n + cols - 1) / cols);
  Picture pic = create_picture(cols * panel, rows * (panel + kBand), kSheet);
  const a::Box box = shape_box(s);
  const double mid[3] = {(box.min[0] + box.max[0]) / 2.0, (box.min[1] + box.max[1]) / 2.0,
                         (box.min[2] + box.max[2]) / 2.0};
  ViewStyle style;
  style.wire = true;
  style.edge = {255, 255, 255, 90};
  style.floor = false;
  int k = 0;
  for (int j : joints) {
    float centre[3];
    float radius = 0;
    if (!detail_region(m, s, j, centre, &radius)) continue;
    // From outside: the side of the model the region lies on, then the opposite side.
    const double dx = centre[0] - mid[0], dz = centre[2] - mid[2];
    const double out =
        std::fabs(dx) + std::fabs(dz) < 1e-6 ? 0.0 : core::libm::atan2(dx, dz) * 57.29577951308232;
    const double azs[2] = {out, out + 180};
    for (double az : azs) {
      const int x = (k % cols) * panel, y = (k / cols) * (panel + kBand);
      draw_shape(pic, x, y + kBand, panel, panel, close_cam(centre, radius, az, 15), m, s, style,
                 box.min[1]);
      draw_text(pic, x + 4, y + 3,
                m.joints[static_cast<size_t>(j)].name + "  " + side_name(az) + "  R " +
                    fixed(radius * 100, 0) + " CM",
                kText);
      ++k;
    }
  }
  return pic;
}

Picture lod_sheet(const a::Model& m, const std::function<Shape(int)>& shape_at_lod, int levels,
                  double floor_y, int panel, std::vector<size_t>* triangles) {
  levels = std::max(levels, 1);
  // The heights a figure is drawn at on screen, px: each level's band, two a level.
  static const int kHeights[4] = {96, 48, 24, 12};
  const int small = panel / 2;
  Picture pic = create_picture(std::max(levels * panel, 4 * small + 8),
                               panel + kBand + levels * (small + kBand), kSheet);
  std::vector<Shape> shapes;
  shapes.reserve(static_cast<size_t>(levels));
  for (int n = 0; n < levels; ++n) shapes.push_back(shape_at_lod(n));
  a::Box box = shape_box(shapes[0]);
  ViewStyle style;
  style.wire = true;
  style.edge = {255, 255, 255, 60};
  triangles->clear();
  for (int n = 0; n < levels; ++n) {
    const size_t tris = shapes[static_cast<size_t>(n)].idx.size() / 3;
    triangles->push_back(tris);
    draw_shape(pic, n * panel, kBand, panel, panel, orbit_cam(box, 35, 12), m,
               shapes[static_cast<size_t>(n)], style, floor_y);
    draw_text(pic, n * panel + 4, 3,
              "LOD " + std::to_string(n) + "  " + std::to_string(tris) + " TRIANGLES", kText);
  }
  // Each level as small as it is seen, scaled up nearest so its pixels show.
  ViewStyle plain;
  plain.floor = false;
  plain.supersample = 4;
  for (int n = 0; n < levels; ++n) {
    const int y0 = panel + kBand + n * (small + kBand);
    draw_text(pic, 4, y0 + 3, "LOD " + std::to_string(n) + " AT 96, 48, 24 AND 12 PX TALL", kDim);
    for (int h = 0; h < 4; ++h) {
      const int px = kHeights[h];
      Picture tiny = create_picture(px, px, kSheet);
      draw_shape(tiny, 0, 0, px, px, orbit_cam(box, 35, 8, 30, 0.95), m,
                 shapes[static_cast<size_t>(n)], plain, floor_y);
      blit(pic, resized(tiny, small, small), h * (small + 2), y0 + kBand);
    }
  }
  return pic;
}

Picture strip_sheet(const a::Model& m, std::span<const Shape> frames,
                    std::span<const std::string> labels, std::span<const double> azimuths,
                    double floor_y, int panel, double el) {
  const int cols = static_cast<int>(frames.size());
  const int rows = static_cast<int>(azimuths.size());
  Picture pic =
      create_picture(std::max(cols, 1) * panel, std::max(rows, 1) * (panel + kBand), kSheet);
  if (frames.empty()) return pic;
  a::Box box = shape_box(frames[0]);
  for (const Shape& f : frames) {
    const a::Box b = shape_box(f);
    for (int c = 0; c < 3; ++c) {
      box.min[c] = std::min(box.min[c], b.min[c]);
      box.max[c] = std::max(box.max[c], b.max[c]);
    }
  }
  ViewStyle style;
  for (int r = 0; r < rows; ++r) {
    const ViewCam cam = orbit_cam(box, azimuths[static_cast<size_t>(r)], el);
    for (int c = 0; c < cols; ++c) {
      const int x = c * panel, y = r * (panel + kBand);
      draw_shape(pic, x, y + kBand, panel, panel, cam, m, frames[static_cast<size_t>(c)], style,
                 floor_y);
      std::string label =
          static_cast<size_t>(c) < labels.size() ? labels[static_cast<size_t>(c)] : std::string();
      if (c == 0) {
        std::string az = "AZ ";
        az += fixed(azimuths[static_cast<size_t>(r)], 0);
        az += "  ";
        az += label;
        label = std::move(az);
      }
      draw_text(pic, x + 4, y + 3, label, kText);
    }
  }
  return pic;
}

}  // namespace ge::lab
