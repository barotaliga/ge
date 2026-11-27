// Decals: the kinds' table, their masks and looks, the atlas, the clip onto receivers and the pool.
//
// ge:summary Decals' kinds, masks, ageing looks, atlas bake, box clip and pool
// ge:keywords detail decal mask scorch bullet-hole blood clip atlas pool expire burn-line melt
// ge:skill detail
// ge:test tests/detail_decals_test.cpp
#include "detail/decals.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/noise.hpp"
#include "core/prng.hpp"

namespace ge::detail {
namespace {

constexpr DecalKindSpec kSpecs[kDecalKinds] = {
    {"scorch", 0, 0, 0, 0.55, 80, 0.6},      {"bullet_hole", 0, 0, 0, 0.5, 60, 0},
    {"blood", 120, 60, 40, 0.4, 85, 0},      {"burn_line", 0, 0, 0, 0.55, 80, 3.0},
    {"melt", 0, 0, 0, 0.4, 80, 3.0},         {"blood_pool", 120, 60, 75, 0.15, 30, 0},
    {"blood_drip", 120, 60, 40, 0.4, 50, 0}, {"blood_print", 120, 60, 30, 0.25, 30, 0},
};

// How long a pool takes to spread out and a drip to run its length, s.
constexpr double kPoolSpreadS = 2.5, kDripRunS = 6;

// The lift off the surface that keeps a decal in front of it, m.
constexpr double kLift = 0.001;
// How far a buried face's solid is looked through, m.
constexpr double kBuriedReach = 8;

double fbm(double x, double y, double scale, double seed) {
  double sum = 0, amp = 0.5, norm = 0;
  for (int o = 0; o < 3; ++o) {
    sum += amp * core::value_noise(x, y, scale, seed + o * 17);
    norm += amp;
    amp *= 0.5;
    scale *= 0.5;
  }
  return sum / norm;
}

// Noise round a circle at angle `a`, so the ragged edge closes on itself.
double round_noise(double a, double scale, double seed) {
  return fbm(core::libm::cos(a) * 4, core::libm::sin(a) * 4, scale, seed);
}

MaskSample scorch(uint32_t var, double x, double y, double r, double a) {
  const double seed = 101 + var * 7;
  // The burnt core, ragged; tongues thrown out by the blast; and a faint soot halo round both.
  const double edge = 0.3 + 0.1 * (round_noise(a, 1.6, seed) - 0.5);
  const double streak = ramp(0.5, 0.8, round_noise(a, 0.9, seed + 3));
  const double reach = edge + 0.12 * streak;
  const double core_cover = ramp(reach, reach - 0.26, r);
  const double halo = 0.8 * ramp(0.5, 0.16, r) * (0.65 + 0.7 * fbm(x * 5, y * 5, 1, seed + 4));
  double cover = std::max(core_cover, halo);
  cover *= 0.82 + 0.18 * fbm(x * 8, y * 8, 1, seed + 9);
  const double shade = -0.35 * ramp(0.3, 0, r) + 0.15 * (fbm(x * 20, y * 20, 1, seed) - 0.5);
  // The embers: patches of the burnt core and the blast's tongues, not the soot halo.
  const double ember = std::max(core_cover * ramp(0.42, 0.7, fbm(x * 14, y * 14, 1, seed + 21)),
                                0.8 * core_cover * streak * ramp(0.12, 0.3, r));
  return {core::clamp(cover, 0, 1), shade, core::clamp(ember, 0, 1)};
}

MaskSample bullet(uint32_t var, double x, double y, double r, double a) {
  const double seed = 211 + var * 11;
  const double hole = 0.09 + 0.02 * round_noise(a, 1, seed);
  if (r < hole) return {1, -1};
  // The crater: its walls in shadow, dark falling to a broken rim.
  const double rim = 0.2 + 0.05 * round_noise(a, 0.8, seed + 1);
  if (r < rim) {
    const double k = (r - hole) / (rim - hole);
    return {1, -0.85 + 0.9 * k};
  }
  // Cracks: a few rays, each a thin wedge of angle that thins out with distance.
  core::Rng rng = core::make_rng(var * 2654435761u + 7);
  const int cracks = core::int_in(rng, 3, 6);
  double crack = 0;
  for (int i = 0; i < cracks; ++i) {
    const double ca = core::range(rng, 0, core::kTau);
    const double len = core::range(rng, 0.3, 0.48);
    const double d = std::fabs(
        core::angle_diff(a, ca + 0.25 * (r - rim) * (fbm(r * 10, ca, 1, seed + i) - 0.5) * 4));
    const double width = 0.07 * (1 - (r - rim) / (len - rim));
    if (r < len && width > 0 && d < width) crack = std::max(crack, 1 - d / width);
  }
  // The soot and grime the round drove into the face round the hole: a dark smudge, densest at
  // the rim and frayed at its edge, so the hole reads from across a room and not only up close.
  const double soot = 0.7 * ramp(0.44, rim, r) * (0.65 + 0.35 * fbm(x * 9, y * 9, 1, seed + 5));
  const double cover = std::max(crack * 0.9, soot);
  return {cover, crack > soot ? -0.6 : -0.3};
}

MaskSample blood(uint32_t var, double x, double y, double r, double a) {
  const double seed = 307 + var * 13;
  const double edge = 0.24 + 0.1 * (round_noise(a, 1.2, seed) - 0.5) * 2;
  // A run in one direction, the way a splash leaves a wound.
  core::Rng rng = core::make_rng(var * 2246822519u + 3);
  const double run_a = core::range(rng, 0, core::kTau);
  const double run = std::max(0.0, core::libm::cos(core::angle_diff(a, run_a)));
  const double reach = edge + 0.12 * run * run * run * run;
  double cover = ramp(reach, reach - 0.04, r);
  double rim = cover > 0 ? ramp(reach - 0.09, reach - 0.02, r) : 0;
  // The droplets round it.
  const int drops = core::int_in(rng, 6, 11);
  for (int i = 0; i < drops; ++i) {
    const double da = run_a + core::range(rng, -1.3, 1.3);
    const double dr = core::range(rng, 0.3, 0.47);
    const double size = core::range(rng, 0.012, 0.035) * (1.3 - dr);
    const double dx = x - core::libm::cos(da) * dr, dy = y - core::libm::sin(da) * dr;
    const double d = std::sqrt(dx * dx + dy * dy);
    const double c = ramp(size, size * 0.6, d);
    if (c > cover) {
      cover = c;
      rim = 0.5;
    }
  }
  return {cover, -0.15 + 0.6 * rim};
}

// Blood gathered on a floor: a puddle with a lobed edge (a few lobes and a finer waver), a few
// drops fallen beside it, and a rim a little raised (darker as it dries). It spreads from the
// middle out: `arrive` its distance over the edge's.
MaskSample blood_pool(uint32_t var, double x, double y, double r, double a) {
  const double seed = 601 + var * 17;
  const double edge = 0.36 + 0.09 * (round_noise(a, 1.0, seed) - 0.5) * 2 +
                      0.03 * (round_noise(a, 0.4, seed + 1) - 0.5) * 2;
  double cover = ramp(edge, edge - 0.02, r);
  double rim = cover > 0 ? ramp(edge - 0.07, edge - 0.01, r) : 0;
  double arrive = core::clamp(r / edge, 0, 1) * 0.95;
  core::Rng rng = core::make_rng(var * 2654435761u + 11);
  const int drops = core::int_in(rng, 2, 5);
  for (int i = 0; i < drops; ++i) {
    const double da = core::range(rng, 0, core::kTau);
    const double dr = core::range(rng, 0.4, 0.46);
    const double size = core::range(rng, 0.012, 0.03);
    const double dx = x - core::libm::cos(da) * dr, dy = y - core::libm::sin(da) * dr;
    const double c = ramp(size, size * 0.6, std::sqrt(dx * dx + dy * dy));
    if (c > cover) {
      cover = c;
      rim = 0.6;
      arrive = 0.5;
    }
  }
  return {cover, -0.1 + 0.7 * rim, 0, arrive};
}

// Blood running down a wall from a splash: u down the box (kDripAspect times its width), v across.
// A blot where it struck and two to four runs from its lower edge, each wandering a little and
// ending in a bead; the runs reach as far as they get by `arrive` (their length over the longest
// a run may be), the blot at once.
MaskSample blood_drip(uint32_t var, double u, double v) {
  const double seed = 701 + var * 19;
  const double x = v - 0.5, y = u * kDripAspect;  // in widths: x across, y down from the top
  const double by = kDripBlotU * kDripAspect;     // the blot's middle
  const double bx = x, bdy = y - by;
  const double br = std::sqrt(bx * bx + bdy * bdy);
  const double ba = core::libm::atan2(bdy, bx);
  const double edge = 0.2 + 0.04 * (round_noise(ba, 1.2, seed) - 0.5) * 2;
  double cover = ramp(edge, edge - 0.03, br);
  double shade = cover > 0 ? -0.15 + 0.6 * ramp(edge - 0.07, edge - 0.02, br) : 0;
  double arrive = 0;
  constexpr double kLongest = 2.4;  // the longest run, widths
  core::Rng rng = core::make_rng(var * 2246822519u + 29);
  const int runs = core::int_in(rng, 2, 4);
  for (int i = 0; i < runs; ++i) {
    const double x0 = core::range(rng, -0.14, 0.14);
    const double y0 = by + std::sqrt(std::max(0.0, edge * edge - x0 * x0)) * 0.7;
    const double len = core::range(rng, 0.5, kLongest);
    const double y1 = std::min(y0 + len, kDripAspect - 0.15);
    const double w = core::range(rng, 0.022, 0.04);
    if (y < y0 - 0.05 || y > y1 + w * 2.2) continue;
    const double along = std::max(0.0, y - y0);
    const auto x_at = [&](double yy) {
      return x0 + 0.06 * (fbm(yy * 1.5, i * 3.1, 1, seed + i) - 0.5) * 2 * (yy - y0);
    };
    // The run, thinning a little as it goes; the bead at its end, fatter.
    const double yy = std::min(y, y1);
    const double width = w * (1 - 0.25 * along / std::max(len, 1e-3));
    double c = y <= y1 ? ramp(width, width * 0.45, std::fabs(x - x_at(yy))) : 0;
    const double bead_r = w * 1.7;
    const double ex = x - x_at(y1), ey = y - (y1 - bead_r * 0.3);
    const double bead = ramp(bead_r, bead_r * 0.6, std::sqrt(ex * ex + ey * ey));
    c = std::max(c, bead);
    if (c > cover) {
      cover = c;
      shade = bead > 0.5 ? 0.35 : 0.05;
      arrive = 0.05 + 0.95 * core::clamp(along / kLongest, 0, 1);
    }
  }
  return {core::clamp(cover, 0, 1), shade, 0, arrive};
}

// A boot's print in blood: x across, y along (the toe toward -y), both -0.5..0.5 of the box. The
// sole's ball and its heel, a gap at the arch, lugs across both and blood taken patchily as a sole
// lifting off a floor leaves it; odd variants the right foot (mirrored).
MaskSample blood_print(uint32_t var, double x, double y) {
  const double seed = 811 + var * 23;
  const double mx = (var & 1u) != 0 ? -x : x;
  // The ball: an egg wider toward the toe, its inner side straighter (toward +x, the left foot's).
  const double bx = (mx + 0.015) / (0.16 + 0.025 * ramp(0.1, -0.3, y)), by = (y + 0.13) / 0.25;
  const double ball = std::sqrt(bx * bx + by * by);
  const double hx = mx / 0.13, hy = (y - 0.29) / 0.14;
  const double heel = std::sqrt(hx * hx + hy * hy);
  double cover = std::max(ramp(1.0, 0.85, ball), ramp(1.0, 0.85, heel));
  if (cover <= 0) return {0, 0};
  // The lugs: bars across the sole, a little blood between them.
  const double lug = 0.5 + 0.5 * core::libm::sin(y * 120 + 1.3 * core::libm::sin(mx * 30));
  cover *= 0.8 + 0.2 * ramp(0.3, 0.7, lug);
  // Patchy: blood taken where the sole pressed, more at its edge.
  const double patch = fbm(x * 6, y * 6, 1, seed);
  cover *= ramp(0.2, 0.45, patch + 0.15 * std::max(ball < 1 ? ball : heel, 0.0));
  return {core::clamp(cover, 0, 1), 0.1 * (lug - 0.5)};
}

// A beam's burnt line: u along it, v across. A ragged black core that wanders a little off the
// middle (never at the ends, so pieces laid end to end meet), a thin soot halo either side, both
// thinned over each end's last stretch so the overlap with the next piece does not show.
MaskSample burn_line(uint32_t var, double u, double v) {
  const double seed = 401 + var * 5;
  const double y = v - 0.5;
  const double taper = ramp(0, 0.14, u) * ramp(1, 0.86, u);
  if (taper <= 0) return {0, 0};
  const double bell = core::libm::sin(core::kPi * u);
  const double wander = 0.07 * bell * (fbm(u * 6, 0.5, 1, seed) - 0.5) * 2;
  const double d = std::fabs(y - wander);
  const double hw = 0.15 + 0.06 * (fbm(u * 9, 3.5, 1, seed + 1) - 0.5) * 2;
  const double core_cover = ramp(hw, hw - 0.07, d);
  const double halo = 0.55 * ramp(0.48, hw, d) * (0.6 + 0.8 * fbm(u * 12, y * 12, 1, seed + 4));
  const double grain = 0.85 + 0.15 * fbm(u * 20, y * 20, 1, seed + 9);
  const double cover = std::max(core_cover, halo * grain) * taper;
  const double shade = -0.3 * core_cover + 0.15 * (fbm(u * 24, y * 24, 1, seed + 2) - 0.5);
  return {core::clamp(cover, 0, 1), shade, core::clamp(core_cover * taper, 0, 1)};
}

// The pool a held beam melts into a wall: a ragged molten middle, a ring of raised slag round it
// and a soot halo outside.
MaskSample melt(uint32_t var, double x, double y, double r, double a) {
  const double seed = 503 + var * 3;
  const double edge = 0.36 + 0.08 * (round_noise(a, 1.4, seed) - 0.5) * 2;
  const double pool = ramp(edge, edge - 0.1, r);
  const double ring = ramp(edge - 0.14, edge - 0.04, r) * pool;
  const double halo =
      0.6 * ramp(0.5, edge - 0.02, r) * (0.6 + 0.8 * fbm(x * 6, y * 6, 1, seed + 4));
  const double cover = std::max(pool, halo) * (0.88 + 0.12 * fbm(x * 10, y * 10, 1, seed + 9));
  // The melt's middle mottled (its glow brighter where the shade is lower): runs of hotter metal
  // and darker crust, so it never reads as a flat disc.
  const double mottle = fbm(x * 9, y * 9, 1, seed + 6);
  const double shade = pool * (1 - ring) * (-0.1 - 0.8 * mottle) + 0.35 * ring +
                       0.1 * (fbm(x * 24, y * 24, 1, seed) - 0.5);
  return {core::clamp(cover, 0, 1), shade, core::clamp(pool * (1 - 0.6 * ring), 0, 1)};
}

// Sutherland-Hodgman against dot(n, p) <= d.
void clip_loop(const std::vector<Vec3>& in, const Vec3& n, double d, std::vector<Vec3>& out) {
  out.clear();
  const size_t k = in.size();
  for (size_t i = 0; i < k; ++i) {
    const Vec3& a = in[i];
    const Vec3& b = in[(i + 1) % k];
    const double da = dot(n, a) - d, db = dot(n, b) - d;
    if (da <= 0) out.push_back(a);
    if ((da < 0 && db > 0) || (da > 0 && db < 0)) out.push_back(mad(a, sub(b, a), da / (da - db)));
  }
}

// A clipped piece wholly inside a solid: its middle and each corner (drawn a centimetre in toward
// the middle, off the edges a neighbour may stand on) all buried. A piece half under a block
// stays; the block's own faces hide that half.
bool piece_buried(const Receivers& rec, const std::vector<Vec3>& poly, const Vec3& n,
                  ReceiverScratch& s) {
  Vec3 mid{0, 0, 0};
  for (const Vec3& v : poly) mid = add(mid, v);
  mid = mul(mid, 1.0 / static_cast<double>(poly.size()));
  if (!buried_at(rec, mid, n, kBuriedReach, s)) return false;
  for (const Vec3& v : poly) {
    const Vec3 in = sub(mid, v);
    const double d = length(in);
    const Vec3 p = d > 0.02 ? mad(v, in, 0.01 / d) : mid;
    if (!buried_at(rec, p, n, kBuriedReach, s)) return false;
  }
  return true;
}

}  // namespace

const DecalKindSpec& decal_spec(DecalKind kind) { return kSpecs[static_cast<int>(kind)]; }

DecalLook look_at(DecalKind kind, double age_s, double ember_s, double life_s, double spread_from) {
  DecalKindSpec s = decal_spec(kind);
  if (ember_s > 0) s.ember_s = ember_s;
  switch (kind) {
    case DecalKind::scorch: {
      // Embers: white-orange at the blast, cooling through red to nothing over `ember_s`; the
      // cube of what is left so the glow falls as fast as the fireball dies (a mark that glowed on
      // under the smoke read as a second, brown cloud) and lingers only as a dull red.
      const double hot = 1 - ramp(0, s.ember_s, age_s);
      const double k = hot * hot * hot;
      return {0.035, 0.03, 0.025, 0.92, 0, 2.6 * k, 0.9 * k * hot, 0.2 * k * hot * hot};
    }
    case DecalKind::bullet_hole:
      return {0.03, 0.028, 0.026, 1, 0};  // soot-black: it must read against any wall
    case DecalKind::burn_line:
    case DecalKind::melt: {
      // White-hot where the beam has just been, through orange and red to black soot over
      // `ember_s`; the square of what is left, so it holds its glow longer than a blast's scorch.
      const double hot = 1 - ramp(0, s.ember_s, age_s);
      const double k = hot * hot;
      return {0.03, 0.026, 0.022, 0.95, 0, 2.6 * k, 0.9 * k * hot, 0.2 * k * hot * hot};
    }
    case DecalKind::blood:
    case DecalKind::blood_pool:
    case DecalKind::blood_drip:
    case DecalKind::blood_print: {
      // A life of its own: the drying and the fading shortened to fit it; < 0 never fades.
      double life = s.life_s, dry_s = s.dry_s, fade_s = s.fade_s;
      if (life_s > 0) {
        life = life_s;
        dry_s = std::min(dry_s, 0.6 * life);
        fade_s = std::min(fade_s, 0.5 * life);
      }
      // Darkening fast at first as it thickens, then slowly; the gloss going sooner.
      const double u = core::clamp(age_s / dry_s, 0, 1);
      const double dry = 1 - (1 - u) * (1 - u);
      const double g = core::clamp(age_s / (0.7 * dry_s), 0, 1);
      const double dull = 1 - (1 - g) * (1 - g);
      const double fade = life_s < 0 ? 1 : core::clamp((life - age_s) / fade_s, 0, 1);
      // A pool is thicker: darker, glossier and slower to dry than a splash's film.
      const bool pool = kind == DecalKind::blood_pool;
      const double wet[3] = {pool ? 0.11 : 0.17, pool ? 0.0035 : 0.006, pool ? 0.0035 : 0.0055};
      const double dried[3] = {pool ? 0.026 : 0.038, pool ? 0.0045 : 0.007, pool ? 0.0035 : 0.005};
      const double gloss = pool ? core::lerp(0.95, 0.14, dull)
                                : core::lerp(kind == DecalKind::blood_drip ? 0.7 : 0.6, 0.05, dull);
      DecalLook l{core::lerp(wet[0], dried[0], dry), core::lerp(wet[1], dried[1], dry),
                  core::lerp(wet[2], dried[2], dry), (pool ? 0.97 : 0.95) * fade, gloss};
      // A pool spreads out and a drip runs on from where it was pressed, slowing as it goes.
      const double run = kind == DecalKind::blood_pool   ? kPoolSpreadS
                         : kind == DecalKind::blood_drip ? kDripRunS
                                                         : 0;
      if (run > 0) {
        const double k = core::clamp(age_s / run, 0, 1);
        l.spread = spread_from + (1 - spread_from) * (1 - (1 - k) * (1 - k));
      }
      return l;
    }
  }
  return {0, 0, 0, 0, 0};
}

MaskSample mask_at(DecalKind kind, uint32_t variant, double u, double v) {
  if (u < 0 || u > 1 || v < 0 || v > 1) return {0, 0};
  if (kind == DecalKind::burn_line) return burn_line(variant, u, v);
  if (kind == DecalKind::blood_drip) return blood_drip(variant, u, v);
  if (kind == DecalKind::blood_print) return blood_print(variant, u - 0.5, v - 0.5);
  const double x = u - 0.5, y = v - 0.5;
  const double r = std::sqrt(x * x + y * y);
  if (r >= 0.5) return {0, 0};
  const double a = core::libm::atan2(y, x);
  switch (kind) {
    case DecalKind::scorch:
      return scorch(variant, x, y, r, a);
    case DecalKind::bullet_hole:
      return bullet(variant, x, y, r, a);
    case DecalKind::blood:
      return blood(variant, x, y, r, a);
    case DecalKind::melt:
      return melt(variant, x, y, r, a);
    case DecalKind::blood_pool:
      return blood_pool(variant, x, y, r, a);
    case DecalKind::burn_line:
    case DecalKind::blood_drip:
    case DecalKind::blood_print:
      break;
  }
  return {0, 0};
}

std::vector<uint8_t> bake_atlas(int size, int variants) {
  const int w = size * variants, h = size * kDecalKinds;
  std::vector<uint8_t> px(static_cast<size_t>(w) * h * 4, 0);
  for (int k = 0; k < kDecalKinds; ++k) {
    for (int var = 0; var < variants; ++var) {
      for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
          const MaskSample m = mask_at(static_cast<DecalKind>(k), static_cast<uint32_t>(var),
                                       (x + 0.5) / size, (y + 0.5) / size);
          const size_t o =
              ((static_cast<size_t>(k) * size + y) * w + static_cast<size_t>(var) * size + x) * 4;
          const auto c = static_cast<uint8_t>(std::lround(m.cover * 255));
          px[o] = c;
          px[o + 1] = static_cast<uint8_t>(std::lround((m.shade * 0.5 + 0.5) * 255));
          const double b = is_blood(static_cast<DecalKind>(k)) ? m.arrive : m.ember;
          px[o + 2] = static_cast<uint8_t>(std::lround(b * 255));
          px[o + 3] = c;
        }
      }
    }
  }
  return px;
}

DecalPool make_pool(uint32_t capacity, uint32_t verts_per_decal) {
  DecalPool p;
  p.capacity = capacity;
  p.verts_per_decal = verts_per_decal - verts_per_decal % 3;
  p.decals.resize(capacity);
  p.verts.resize(static_cast<size_t>(capacity) * p.verts_per_decal);
  p.poly.reserve(16);
  p.next.reserve(16);
  p.staged.reserve(size_t{p.verts_per_decal} * 4);
  p.order.reserve(size_t{p.verts_per_decal} * 4 / 3);
  p.kept.reserve(p.verts_per_decal);
  return p;
}

int place_decal(DecalPool& pool, const Receivers& rec, const DecalAt& d) {
  if (pool.capacity == 0 || rec.mesh == nullptr) return -1;
  const DecalKindSpec& spec = decal_spec(d.kind);
  const Vec3 n = normalized(d.normal);
  const Vec3 t0 = any_perpendicular(n);
  const Vec3 b0 = cross(n, t0);
  const double cs = core::libm::cos(d.spin_rad), sn = core::libm::sin(d.spin_rad);
  Vec3 t = add(mul(t0, cs), mul(b0, sn));
  // A line's own direction, laid flat on the surface.
  const Vec3 flat = sub(d.along, mul(n, dot(d.along, n)));
  if (length(flat) > 1e-9) t = normalized(flat);
  const Vec3 b = cross(n, t);
  const double long_m = d.length_m > 0 ? d.length_m : d.size_m;
  const double half_t = long_m * 0.5, half = d.size_m * 0.5;
  const double depth = std::max(0.02, d.size_m * spec.depth_share);
  const double reach = std::sqrt(half_t * half_t + half * half) + depth;
  triangles_in(rec, {d.at.x - reach, d.at.y - reach, d.at.z - reach},
               {d.at.x + reach, d.at.y + reach, d.at.z + reach}, pool.scratch);
  const double min_cos = core::libm::cos(spec.max_angle_deg * core::kPi / 180);
  const Vec3 planes[6] = {t, mul(t, -1), b, mul(b, -1), n, mul(n, -1)};
  const double dists[6] = {dot(t, d.at) + half_t, -dot(t, d.at) + half_t, dot(b, d.at) + half,
                           -dot(b, d.at) + half,  dot(n, d.at) + depth,   -dot(n, d.at) + depth};
  pool.staged.clear();
  for (uint32_t tri : pool.scratch.found) {
    const Vec3 tn = normal_of(*rec.mesh, tri);
    const double facing = dot(tn, n);
    if (facing < min_cos) continue;
    pool.poly = {corner(*rec.mesh, tri, 0), corner(*rec.mesh, tri, 1), corner(*rec.mesh, tri, 2)};
    for (int k = 0; k < 6 && pool.poly.size() >= 3; ++k) {
      clip_loop(pool.poly, planes[k], dists[k], pool.next);
      pool.poly.swap(pool.next);
    }
    if (pool.poly.size() < 3) continue;
    if (piece_buried(rec, pool.poly, tn, pool.scratch_up)) continue;
    const double fade = ramp(min_cos, min_cos + (1 - min_cos) * 0.5, facing);
    auto vert = [&](const Vec3& p) {
      const Vec3 rel = sub(p, d.at);
      return DecalVertex{mad(p, tn, kLift), dot(rel, t) / long_m + 0.5,
                         dot(rel, b) / d.size_m + 0.5, fade};
    };
    for (size_t k = 1; k + 1 < pool.poly.size(); ++k) {
      pool.staged.push_back(vert(pool.poly[0]));
      pool.staged.push_back(vert(pool.poly[k]));
      pool.staged.push_back(vert(pool.poly[k + 1]));
    }
  }
  if (pool.staged.empty()) return -1;
  if (pool.staged.size() > pool.verts_per_decal) {
    // Too many for a slot: keep the triangles nearest the centre (ties by their order).
    const size_t tris = pool.staged.size() / 3;
    auto& order = pool.order;
    order.clear();
    for (size_t i = 0; i < tris; ++i) {
      const Vec3 c = mul(
          add(add(pool.staged[i * 3].pos, pool.staged[i * 3 + 1].pos), pool.staged[i * 3 + 2].pos),
          1.0 / 3);
      order.push_back({length(sub(c, d.at)), i});
    }
    std::sort(order.begin(), order.end());
    auto& kept = pool.kept;
    kept.clear();
    for (size_t k = 0; k < order.size() && kept.size() + 3 <= pool.verts_per_decal; ++k) {
      for (int j = 0; j < 3; ++j) kept.push_back(pool.staged[order[k].second * 3 + j]);
    }
    pool.staged.swap(kept);
  }
  // Its own slot when pressed again in place; else a free slot, else the oldest placed.
  uint32_t slot = 0;
  uint32_t oldest = UINT32_MAX;
  const bool again = d.slot >= 0 && static_cast<uint32_t>(d.slot) < pool.capacity &&
                     pool.decals[static_cast<size_t>(d.slot)].live &&
                     pool.decals[static_cast<size_t>(d.slot)].serial == d.serial;
  if (again) slot = static_cast<uint32_t>(d.slot);
  for (uint32_t i = 0; !again && i < pool.capacity; ++i) {
    if (!pool.decals[i].live) {
      slot = i;
      break;
    }
    if (pool.decals[i].serial < oldest) {
      oldest = pool.decals[i].serial;
      slot = i;
    }
  }
  Decal& dc = pool.decals[slot];
  dc = {true,       d.kind,
        d.variant,  pool.next_serial++,
        d.now_s,    d.at,
        n,          d.size_m,
        d.length_m, d.ember_s,
        d.life_s,   d.spread_from,
        d.spin_rad, static_cast<uint32_t>(pool.staged.size()),
        d.strength};
  std::copy(pool.staged.begin(), pool.staged.end(),
            pool.verts.begin() + static_cast<std::ptrdiff_t>(slot) * pool.verts_per_decal);
  return static_cast<int>(slot);
}

void expire(DecalPool& pool, double now_s) {
  for (Decal& d : pool.decals) {
    const double life = d.life_s != 0 ? d.life_s : decal_spec(d.kind).life_s;
    if (d.live && life > 0 && now_s - d.born_s >= life) d.live = false;
  }
}

DecalLook look_of(const Decal& d, double now_s) {
  DecalLook l = look_at(d.kind, now_s - d.born_s, d.ember_s, d.life_s, d.spread_from);
  l.opacity *= d.strength;
  return l;
}

uint32_t live_count(const DecalPool& pool) {
  uint32_t n = 0;
  for (const Decal& d : pool.decals) n += d.live ? 1 : 0;
  return n;
}

std::span<const DecalVertex> decal_verts(const DecalPool& pool, uint32_t i) {
  return {pool.verts.data() + static_cast<size_t>(i) * pool.verts_per_decal, pool.decals[i].count};
}

}  // namespace ge::detail
