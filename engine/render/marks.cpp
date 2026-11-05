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
    const detail::DecalLook look = detail::look_at(d.kind, now_s - d.born_s);
    const float soot = static_cast<float>(look.opacity * (1 - (look.r + look.g + look.b) / 3));
    // A fresh scorch's embers at their hottest (look_at's 2.6) are kHotEmberCd.
    const float ember = static_cast<float>(look.er / 2.6) * kHotEmberCd;
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
        b.up[0] = static_cast<float>(gu);
        b.up[1] = static_cast<float>(gv);
        for (int c = 0; c < 3; ++c) b.light[c] = lit[static_cast<size_t>(c)];
        out.push_back(b);
      }
    }
  }
}

}  // namespace ge::render
