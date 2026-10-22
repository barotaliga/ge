// LAB `forest`: the trees as built (mountain/app/trees.hpp), every form's every variant at each
// level of detail on one sheet, and every kind in its own colours on another, each tree standing
// as an instance stretches it (its height 4.5 times its crown's radius), seen from a little above
// and lit by a sun behind the left shoulder. The table counts each mesh's triangles against the
// LODs' budgets and the share of its faces that carry snow. CPU pictures (an orthographic raster
// drawn back to front), kept out of the digest since the meshes are built with the platform's
// trigonometry; the triangle counts are in it. Named gpu_ because the trees live in the mountain's
// app half, which only a GPU build makes.
//
// ge:summary Lab: every tree form, variant and LOD on a sheet, every kind's colours, the triangles
// ge:keywords lab forest trees tree-mesh variants lod triangles budget snow-share kinds sheet
// ge:skill mountain labs
// ge:test tests/mountain_trees_test.cpp
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "lab/lab.hpp"
#include "mountain/app/trees.hpp"

namespace {

namespace app = ge::mountain::app;
using ge::lab::Picture;
using ge::lab::Rgba;
using ge::mountain::mapgen::TreeKind;

constexpr Rgba kBack{58, 66, 84};
constexpr Rgba kGround{170, 180, 196};
constexpr Rgba kText{226, 230, 238};
constexpr float kAspect = 4.5f;  // a standing tree's height over its crown's radius
// The LODs' triangle budgets: full, mid, far.
constexpr size_t kBudget[app::kTreeLods] = {450, 160, 60};
const char* const kLodNames[app::kTreeLods] = {"FULL", "MID", "FAR"};
const char* const kFormNames[app::kTreeForms] = {"CONIFER", "PINE", "LARCH", "BROADLEAF", "SNAG"};
// The kind each form is shown in on the first sheet.
constexpr TreeKind kShownAs[app::kTreeForms] = {TreeKind::spruce, TreeKind::pine, TreeKind::larch,
                                                TreeKind::birch, TreeKind::snag};
const char* const kKindNames[app::kTreeKinds] = {
    "SPRUCE",    "FIR",     "PINE",    "LARCH",      "BLACK SPRUCE", "STONE PINE", "WHITE PINE",
    "LODGEPOLE", "HEMLOCK", "JUNIPER", "DWARF PINE", "SNAG",         "BIRCH",      "ASPEN",
    "ROWAN",     "ALDER",   "WILLOW",  "BEECH",      "MAPLE",        "ASH"};

// The view: turned 30 degrees about the vertical, looking 15 degrees down (the constants are
// those angles' cosines and sines).
constexpr float kTurnC = 0.8660254f, kTurnS = 0.5f, kTiltC = 0.9659258f, kTiltS = 0.2588190f;

struct Eye {
  float x, up, depth;  // across the screen, up it, toward the viewer
};
Eye view(float x, float y, float z) {
  const float vx = x * kTurnC - z * kTurnS;
  const float vz = x * kTurnS + z * kTurnC;
  return {vx, y * kTiltC + vz * kTiltS, vz * kTiltC - y * kTiltS};
}

// One triangle filled flat (its pixel centres inside it), clipped to the picture.
void fill(Picture& pic, const double* x, const double* y, Rgba c) {
  const int lx = std::max(0, static_cast<int>(std::floor(std::min({x[0], x[1], x[2]}))));
  const int hx = std::min(pic.width - 1, static_cast<int>(std::ceil(std::max({x[0], x[1], x[2]}))));
  const int ly = std::max(0, static_cast<int>(std::floor(std::min({y[0], y[1], y[2]}))));
  const int hy =
      std::min(pic.height - 1, static_cast<int>(std::ceil(std::max({y[0], y[1], y[2]}))));
  for (int py = ly; py <= hy; ++py) {
    for (int px = lx; px <= hx; ++px) {
      const double cx = px + 0.5, cy = py + 0.5;
      const double w0 = (x[1] - cx) * (y[2] - cy) - (x[2] - cx) * (y[1] - cy);
      const double w1 = (x[2] - cx) * (y[0] - cy) - (x[0] - cx) * (y[2] - cy);
      const double w2 = (x[0] - cx) * (y[1] - cy) - (x[1] - cx) * (y[0] - cy);
      if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0)) {
        ge::lab::set_pixel(pic, px, py, c);
      }
    }
  }
}

// The mesh standing at (ox, base) on the picture, `scale` px a metre of a tree whose crown is
// 1 m across in radius, painted in `kind`'s parts.
void draw_tree(Picture& pic, const ge::render::MeshData& m, TreeKind kind,
               const std::vector<float>& materials, double ox, double base, double scale) {
  struct Tri {
    float depth;
    uint32_t i;
  };
  std::vector<Tri> tris;
  const auto& p = m.positions;
  for (uint32_t t = 0; t + 2 < m.indices.size(); t += 3) {
    float d = 0;
    for (int k = 0; k < 3; ++k) {
      const size_t v = m.indices[t + static_cast<uint32_t>(k)];
      d += view(p[v * 3], p[v * 3 + 1] * kAspect, p[v * 3 + 2]).depth;
    }
    tris.push_back({d, t});
  }
  std::stable_sort(tris.begin(), tris.end(),
                   [](const Tri& a, const Tri& b) { return a.depth < b.depth; });
  // The sun behind the left shoulder and above, as the world lab's mornings light it.
  const float sun[3] = {-0.55f, 0.7f, 0.45f};
  const uint32_t first = app::tree_material(kind);
  for (const Tri& tri : tris) {
    double x[3], y[3];
    const size_t v0 = m.indices[tri.i];
    // The standing normal: the height's stretch undone.
    float n[3] = {m.normals[v0 * 3], m.normals[v0 * 3 + 1] / kAspect, m.normals[v0 * 3 + 2]};
    const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    for (float& c : n) c /= len;
    if (view(n[0], n[1], n[2]).depth < 0) continue;  // turned away
    for (int k = 0; k < 3; ++k) {
      const size_t v = m.indices[tri.i + static_cast<uint32_t>(k)];
      const Eye e = view(p[v * 3], p[v * 3 + 1] * kAspect, p[v * 3 + 2]);
      x[k] = ox + e.x * scale;
      y[k] = base - e.up * scale;
    }
    const float lit = 0.3f + 0.7f * std::max(0.0f, n[0] * sun[0] + n[1] * sun[1] + n[2] * sun[2]);
    const size_t mat = static_cast<size_t>(first + m.parts[v0]) * 3;
    auto ch = [&](int c) {
      return static_cast<uint8_t>(std::clamp(
          std::sqrt(materials[mat + static_cast<size_t>(c)] * lit) * 255.0f, 0.0f, 255.0f));
    };
    fill(pic, x, y, {ch(0), ch(1), ch(2)});
  }
}

void ground(Picture& pic, int x0, int y0, int w, int h) {
  ge::lab::fill_rect(pic, x0, y0, w, h, kBack);
  ge::lab::fill_rect(pic, x0, y0 + h - 22, w, 22, kGround);
}

void run(ge::lab::Lab& lab) {
  const std::vector<float> materials = app::tree_materials();
  constexpr int kCellW = 92, kCellH = 190;
  const double scale = (kCellH - 44) / (kAspect + 0.2);

  // THE FORMS: a row a form, its variants side by side at each LOD.
  Picture forms = ge::lab::create_picture(kCellW * app::kTreeVariants * app::kTreeLods,
                                          kCellH * app::kTreeForms, kBack);
  ge::lab::LabTable counts{"Triangles a mesh (full / mid / far) and its faces' snow share at full",
                           {"form", "variant", "full", "mid", "far", "snow"},
                           {}};
  int over = 0;
  for (int f = 0; f < app::kTreeForms; ++f) {
    for (int v = 0; v < app::kTreeVariants; ++v) {
      std::vector<std::string> row{kFormNames[f], std::to_string(v)};
      double snow = 0;
      for (int l = 0; l < app::kTreeLods; ++l) {
        const auto m = app::tree_mesh(static_cast<app::TreeForm>(f), v, l);
        const size_t tris = m.indices.size() / 3;
        row.push_back(std::to_string(tris));
        if (tris > kBudget[l]) {
          ++over;
          lab.fail(std::string(kFormNames[f]) + " variant " + std::to_string(v) + " " +
                       kLodNames[l] + " has " + std::to_string(tris) +
                       " triangles, over its budget of " + std::to_string(kBudget[l]),
                   "Cut its tiers, pads or limbs at that LOD in plugins/mountain/app/trees.cpp. "
                   "Skill: mountain.");
        }
        if (l == 0) {
          size_t white = 0;
          for (size_t t = 0; t < m.indices.size(); t += 3)
            white += m.parts[m.indices[t]] == app::kSnow;
          snow = tris ? static_cast<double>(white) / static_cast<double>(tris) : 0;
        }
        const int cx = (v * app::kTreeLods + l) * kCellW, cy = f * kCellH;
        ground(forms, cx, cy, kCellW - 2, kCellH - 2);
        draw_tree(forms, m, kShownAs[f], materials, cx + kCellW / 2.0, cy + kCellH - 30, scale);
        ge::lab::draw_text(forms, cx + 4, cy + 4,
                           std::string(kFormNames[f]) + " " + std::to_string(v), kText);
        ge::lab::draw_text(forms, cx + 4, cy + 14,
                           std::string(kLodNames[l]) + " " + std::to_string(tris), kText);
      }
      lab.metric(std::string("snow.") + kFormNames[f] + "." + std::to_string(v), snow, "share",
                 ge::lab::Det::measured);
      row.push_back(std::to_string(std::lround(snow * 100)) + "%");
      counts.rows.push_back(std::move(row));
    }
  }
  lab.table(counts);
  lab.metric("over-budget", over, "count");
  lab.shot({"forms",
            "EVERY TREE FORM, VARIANT AND LOD",
            "A row a form (conifer, pine, larch, broadleaf, snag), its four variants side by side, "
            "each at full, mid and far, standing 4.5 times as tall as its crown is wide, lit from "
            "behind the left shoulder. The numbers are the triangles.",
            "A loaded conifer is banded, white on each tier's upper face and dark under its lip. A "
            "pine's pads are flat and capped. Each LOD keeps the full one's outline. No two "
            "variants of a form stand the same.",
            {}},
           forms, ge::lab::Det::measured);

  // THE KINDS: every kind at full, its first variant, in its own colours.
  constexpr int kPerRow = 10;
  Picture kinds = ge::lab::create_picture(kCellW * kPerRow, kCellH * 2, kBack);
  for (int k = 0; k < app::kTreeKinds; ++k) {
    const auto kind = static_cast<TreeKind>(k);
    const auto m = app::tree_mesh(app::tree_form(kind), 0, 0);
    const int cx = (k % kPerRow) * kCellW, cy = (k / kPerRow) * kCellH;
    ground(kinds, cx, cy, kCellW - 2, kCellH - 2);
    draw_tree(kinds, m, kind, materials, cx + kCellW / 2.0, cy + kCellH - 30, scale);
    ge::lab::draw_text(kinds, cx + 4, cy + 4, kKindNames[k], kText);
  }
  lab.shot({"kinds",
            "EVERY KIND OF TREE IN ITS COLOURS",
            "Every kind the generator plants, drawn as its form's first variant at full detail in "
            "its own needles, bark and twigs.",
            "A birch's bark is pale, a larch's twigs orange-brown, a black spruce the darkest "
            "green; every kind's snow is the same white.",
            {}},
           kinds, ge::lab::Det::measured);
}

const ge::lab::LabSpec kSpec{
    "forest",
    ge::lab::LabKind::picture,
    "plugin:mountain",
    "plugins/mountain/app/trees.hpp",
    "mountain",
    "What does every tree form, variant and level of detail look like, and what does each cost?",
    {},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
