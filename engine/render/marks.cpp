// The marks drawn: the atlas, a blast's scorch pressed, the decals as the beams pass's burns.
//
// ge:summary The decals as burns: the atlas, the scorch pressed, the burn vertices
// ge:keywords render marks decals burns scorch atlas mark_burns press_scorch
// ge:skill render detail
// ge:test tests/render_marks_test.cpp
#include "render/marks.hpp"

#include <algorithm>
#include <cmath>

namespace ge::render {

const std::vector<uint8_t>& mark_atlas() {
  static const std::vector<uint8_t> kAtlas = detail::bake_atlas(kMarkAtlasPx, kMarkVariants);
  return kAtlas;
}

BeamSettings mark_beam_settings(uint32_t marks) {
  BeamSettings s;
  s.max_burn_verts = std::max(marks, 1u) * 240;  // a decal's slot (detail::make_pool's)
  s.atlas = &mark_atlas();
  s.atlas_width = kMarkAtlasPx * kMarkVariants;
  s.atlas_height = kMarkAtlasPx * detail::kDecalKinds;
  return s;
}

int press_scorch(detail::DecalPool& pool, const detail::Receivers& receivers, const Blast& blast) {
  Scorch sc;
  if (!scorch_of(blast, sc)) return -1;
  detail::DecalAt d;
  d.at = {sc.at[0], sc.at[1], sc.at[2]};
  d.normal = {sc.normal[0], sc.normal[1], sc.normal[2]};
  d.size_m = sc.size_m;
  d.spin_rad = sc.spin_rad;
  d.now_s = sc.born_s;
  d.variant = sc.variant % kMarkVariants;
  d.kind = detail::DecalKind::scorch;
  return detail::place_decal(pool, receivers, d);
}

void mark_burns(const detail::DecalPool& pool, double now_s,
                std::span<const std::array<float, 3>> light, std::vector<BurnVertex>& out) {
  out.clear();
  for (uint32_t i = 0; i < pool.capacity; ++i) {
    const detail::Decal& d = pool.decals[i];
    if (!d.live) continue;
    const detail::DecalLook look = detail::look_at(d.kind, now_s - d.born_s, d.ember_s);
    const float soot = static_cast<float>(look.opacity * (1 - (look.r + look.g + look.b) / 3));
    // A fresh scorch's embers at their hottest (look_at's 2.6) are kHotEmberCd.
    const float ember = static_cast<float>(look.er / 2.6) * kHotEmberCd;
    // A beam's line and a melt glow white-hot, past a scorch's dull cap.
    const bool hot = d.kind == detail::DecalKind::burn_line || d.kind == detail::DecalKind::melt;
    const std::span<const detail::DecalVertex> verts = detail::decal_verts(pool, i);
    const std::array<float, 3> lit = i < light.size() ? light[i] : std::array<float, 3>{1, 1, 1};
    for (size_t k = 0; k + 2 < verts.size(); k += 3) {
      // The world's up across this triangle, in the atlas's uv: the way y climbs fastest.
      const detail::DecalVertex& a = verts[k];
      const double du1 = verts[k + 1].u - a.u, dv1 = verts[k + 1].v - a.v;
      const double du2 = verts[k + 2].u - a.u, dv2 = verts[k + 2].v - a.v;
      const double dy1 = verts[k + 1].pos.y - a.pos.y, dy2 = verts[k + 2].pos.y - a.pos.y;
      const double det = du1 * dv2 - du2 * dv1;
      double gu = 0, gv = 1;
      if (std::abs(det) > 1e-12) {
        gu = (dy1 * dv2 - dy2 * dv1) / det;
        gv = (du1 * dy2 - du2 * dy1) / det;
      }
      // Into the atlas's cell (its uv is the mark's over the variants and the kinds).
      gu /= kMarkVariants;
      gv /= detail::kDecalKinds;
      for (size_t j = k; j < k + 3; ++j) {
        const detail::DecalVertex& v = verts[j];
        BurnVertex b;
        b.at[0] = v.pos.x, b.at[1] = v.pos.y, b.at[2] = v.pos.z;
        b.u = static_cast<float>((d.variant + v.u) / kMarkVariants);
        b.v = static_cast<float>((static_cast<int>(d.kind) + v.v) / detail::kDecalKinds);
        b.fade = static_cast<float>(v.fade);
        b.soot = soot;
        b.ember = ember;
        b.relief = d.kind == detail::DecalKind::bullet_hole ? 1.0f : 0.0f;
        b.glow_keys = hot ? kHotGlowKeys : 0.0f;
        b.up[0] = static_cast<float>(gu);
        b.up[1] = static_cast<float>(gv);
        for (int c = 0; c < 3; ++c) b.light[c] = lit[static_cast<size_t>(c)];
        out.push_back(b);
      }
    }
  }
}

int press_glow(detail::DecalPool& pool, const detail::Receivers& receivers, const double at[3],
               const double normal[3], const GlowMark& mark, double now_s, uint32_t variant) {
  detail::DecalAt d;
  d.at = {at[0], at[1], at[2]};
  d.normal = {normal[0], normal[1], normal[2]};
  d.size_m = mark.size_m;
  d.spin_rad = static_cast<double>(variant % 1024) * (6.283185307179586 / 1024);
  d.now_s = now_s;
  d.variant = variant % kMarkVariants;
  d.kind = detail::DecalKind::melt;
  d.ember_s = mark.glow_s;
  return detail::place_decal(pool, receivers, d);
}

std::array<float, 3> ember_rgb(float heat) {
  const float red[3] = {0.55f, 0.04f, 0.0f}, orange[3] = {1.0f, 0.32f, 0.03f},
              white[3] = {1.0f, 0.78f, 0.45f};
  std::array<float, 3> c{};
  const float h = std::clamp(heat, 0.0f, 1.0f);
  for (int k = 0; k < 3; ++k) {
    c[static_cast<size_t>(k)] = h < 0.55f
                                    ? red[k] + (orange[k] - red[k]) * (h / 0.55f)
                                    : orange[k] + (white[k] - orange[k]) * ((h - 0.55f) / 0.45f);
  }
  return c;
}

int mark_glows(const detail::DecalPool& pool, double now_s, MarkGlow* out, int most) {
  if (most <= 0) return 0;
  int n = 0;
  float level[64];
  const int cap = std::min(most, 64);
  // Each glow joins the nearest kept one within kGlowMergeM, else takes a place of its own, else
  // replaces the dimmest when it is brighter.
  for (uint32_t i = 0; i < pool.capacity; ++i) {
    const detail::Decal& d = pool.decals[i];
    if (!d.live) continue;
    const detail::DecalLook look = detail::look_at(d.kind, now_s - d.born_s, d.ember_s);
    const float heat = static_cast<float>(look.er / 2.6);
    if (heat <= 0.01f) continue;
    const double area = d.size_m * (d.length_m > 0 ? d.length_m : d.size_m) * 0.75;
    const float l = heat * static_cast<float>(std::min(area / kGlowAreaM2, 3.0));
    const std::array<float, 3> c = ember_rgb(heat);
    const double at[3] = {d.at.x + d.normal.x * 0.15, d.at.y + d.normal.y * 0.15,
                          d.at.z + d.normal.z * 0.15};
    int near = -1;
    double best = kGlowMergeM * kGlowMergeM;
    for (int k = 0; k < n; ++k) {
      double d2 = 0;
      for (int j = 0; j < 3; ++j) d2 += (out[k].at[j] - at[j]) * (out[k].at[j] - at[j]);
      if (d2 < best) {
        best = d2;
        near = k;
      }
    }
    if (near < 0 && n < cap) {
      near = n++;
      out[near] = MarkGlow{};
      level[near] = 0;
    } else if (near < 0) {
      int dim = 0;
      for (int k = 1; k < n; ++k)
        if (level[k] < level[dim]) dim = k;
      if (level[dim] >= l) continue;
      near = dim;
      out[near] = MarkGlow{};
      level[near] = 0;
    }
    // Joined: the place weighted by level, the colours summed.
    MarkGlow& g = out[near];
    const float sum = level[near] + l;
    for (int j = 0; j < 3; ++j) {
      g.at[j] = level[near] > 0 ? (g.at[j] * level[near] + at[j] * l) / sum : at[j];
      g.rgb[j] += c[static_cast<size_t>(j)] * l;
    }
    level[near] = sum;
    g.reach_m = static_cast<float>(
        std::max(static_cast<double>(g.reach_m), 2.0 + 4.0 * std::sqrt(std::min(sum, 4.0f))));
  }
  // Brightest first.
  for (int a = 1; a < n; ++a)
    for (int b = a; b > 0 && level[b] > level[b - 1]; --b) {
      std::swap(level[b], level[b - 1]);
      std::swap(out[b], out[b - 1]);
    }
  return n;
}

}  // namespace ge::render
