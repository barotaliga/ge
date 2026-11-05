// The window's effects each frame (world_fx.hpp).
//
// ge:summary The window's effects each frame: trails, explosions, their lights, the screen's blend
// ge:keywords world-fx step_fx trail explosion screen-blend face_hit fx_switches held-beam
// ge:keywords warp ghost
// ge:skill render shooter
#include "shooters_edge/client/world_fx.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "brush/map.hpp"
#include "core/math.hpp"
#include "core/prng.hpp"
#include "detail/receivers.hpp"
#include "detail/surfaces.hpp"
#include "detail/vec.hpp"
#include "shooter/hull.hpp"
#include "shooters_edge/app/weapon_fx.hpp"
#include "shooters_edge/client/view.hpp"

namespace shooters_edge::client {

namespace fx = shooters_edge::effects;

namespace {

// The classic explosion sprite's frames and their rate, and the classic bolt's segment, units.
constexpr int kSpriteFrames = 6;
constexpr double kSpriteFps = 10;
constexpr double kBoltSegment = 30;
// The placed effects' entity numbers (below every static's): the sprites', then the bolts'.
constexpr int kSpriteEntities = -(1 << 20), kBoltEntities = -(1 << 21);

// The mod's contents codes for the liquids (games/shooters_edge/qc/common/defs.qc's CONTENT_*).
constexpr float kWater = -3, kSlime = -4, kLava = -5;
constexpr float kEyesUnder = 3;  // waterlevel: the eyes under the surface
// A departing figure's entity in the models drawn: its own, moved out of every real one's way.
constexpr int kLeavingBase = -1000000;

void put(const ge::camera::Vec3& v, double out[3]) {
  out[0] = v.x;
  out[1] = v.y;
  out[2] = v.z;
}

// Whether a player's warp began near `at` (map units) a moment ago: the program's fog for it.
bool near_warp(const WarpWatch& w, const sh::Vec3f& at, double t) {
  for (const Warp& v : w.live) {
    const float dx = v.at.x - at.x, dy = v.at.y - at.y, dz = v.at.z - at.z;
    if (t - v.t0 < 0.25 && dx * dx + dy * dy + dz * dz < 96.0f * 96.0f) return true;
  }
  return false;
}

// A direction in the map's axes as a unit one in the engine's.
void engine_dir(const sh::Vec3f& d, double out[3]) {
  put(engine_point(d), out);
  const double n = ge::core::hypot3(out[0], out[1], out[2]);
  for (int k = 0; k < 3; ++k) out[k] = n > 0 ? out[k] / n : 0;
  if (n == 0) out[1] = 1;
}

// THE FACE A ROUND HIT: the map's triangle the line from `from` through `at` (map units) crosses
// within a few units of `at`, among the surfaces marked in the leaf just before it: its normal
// toward `from` (map axes, unit) and its texture's surface class. False when no triangle is near.
bool face_hit(const ge::brush::Map& m, const sh::Vec3f& from, const sh::Vec3f& at,
              uint32_t* texture, sh::Vec3f* n, ge::detail::SurfaceClass* surface) {
  const double d[3] = {at.x - from.x, at.y - from.y, at.z - from.z};
  const double len = ge::core::hypot3(d[0], d[1], d[2]);
  if (len < 1e-3 || m.leaves.empty()) return false;
  const double u[3] = {d[0] / len, d[1] / len, d[2] / len};
  const float before[3] = {static_cast<float>(at.x - u[0] * 2), static_cast<float>(at.y - u[1] * 2),
                           static_cast<float>(at.z - u[2] * 2)};
  const uint32_t leaf = ge::brush::leaf_at(m, before);
  if (leaf >= m.leaves.size()) return false;
  const ge::brush::Leaf& lf = m.leaves[leaf];
  const double o[3] = {from.x, from.y, from.z};
  constexpr double kNear = 12;  // units either side of the point the server reported
  double best = kNear;
  bool found = false;
  for (uint32_t i = 0; i < lf.marks; ++i) {
    const uint32_t si = m.marks[lf.first_mark + i];
    if (si >= m.surfaces.size()) continue;
    const ge::brush::Surface& sf = m.surfaces[si];
    for (uint32_t t = 0; t + 2 < sf.count; t += 3) {
      const float* a = m.vertices[m.indices[sf.first + t]].pos;
      const float* b = m.vertices[m.indices[sf.first + t + 1]].pos;
      const float* c = m.vertices[m.indices[sf.first + t + 2]].pos;
      const double e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
      const double e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
      const double p[3] = {u[1] * e2[2] - u[2] * e2[1], u[2] * e2[0] - u[0] * e2[2],
                           u[0] * e2[1] - u[1] * e2[0]};
      const double det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
      if (std::abs(det) < 1e-9) continue;
      const double tv[3] = {o[0] - a[0], o[1] - a[1], o[2] - a[2]};
      const double bu = (tv[0] * p[0] + tv[1] * p[1] + tv[2] * p[2]) / det;
      if (bu < -1e-4 || bu > 1 + 1e-4) continue;
      const double q[3] = {tv[1] * e1[2] - tv[2] * e1[1], tv[2] * e1[0] - tv[0] * e1[2],
                           tv[0] * e1[1] - tv[1] * e1[0]};
      const double bv = (u[0] * q[0] + u[1] * q[1] + u[2] * q[2]) / det;
      if (bv < -1e-4 || bu + bv > 1 + 1e-4) continue;
      const double dist = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) / det;
      if (std::abs(dist - len) >= best) continue;
      best = std::abs(dist - len);
      found = true;
      double nn[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                      e1[0] * e2[1] - e1[1] * e2[0]};
      const double nl = ge::core::hypot3(nn[0], nn[1], nn[2]);
      const double facing = nn[0] * u[0] + nn[1] * u[1] + nn[2] * u[2];
      const double sign = facing > 0 ? -1.0 : 1.0;  // toward `from`
      *n = {static_cast<float>(sign * nn[0] / nl), static_cast<float>(sign * nn[1] / nl),
            static_cast<float>(sign * nn[2] / nl)};
      *texture = sf.texture;
      const ge::brush::Texture* tex =
          sf.texture < m.textures.size() ? &m.textures[sf.texture] : nullptr;
      *surface = tex == nullptr                           ? ge::detail::SurfaceClass::unknown
                 : tex->sky                               ? ge::detail::SurfaceClass::sky
                 : tex->liquid != ge::brush::Liquid::none ? ge::detail::SurfaceClass::liquid
                                                          : ge::detail::classify_name(tex->name);
    }
  }
  return found;
}

// The surface nearest `at` (map units) within kNearSurfaceU along the six axes: its normal (out of
// it, engine axes, unit) and how far off it is (m); false in the open.
constexpr float kNearSurfaceU = 40;
bool surface_near(const LoadedMap& m, const sh::Vec3f& at, double normal[3], double* off_m) {
  if (m.bsp.world.models.empty()) return false;
  const sh::Hull& hull = m.bsp.world.models[0].hulls[0];
  float best = 1;
  for (int axis = 0; axis < 3; ++axis)
    for (const float sign : {-1.0f, 1.0f}) {
      sh::Vec3f to = at;
      (axis == 0 ? to.x : axis == 1 ? to.y : to.z) += sign * kNearSurfaceU;
      const sh::HullTrace tr = sh::trace_hull(hull, at, to);
      if (tr.startsolid || tr.fraction >= best) continue;
      best = tr.fraction;
      engine_dir(tr.plane.normal, normal);
    }
  *off_m = best * kNearSurfaceU * ge::brush::kUnitM;
  return best < 1;
}

// The surface a missile flying into `at` (map units) struck: the nearest missile of a frame ago
// within kStruckU, the line on along its flight from the blast for kNearSurfaceU; its normal
// (engine axes) and how far off it the blast is (m). False for none (a grenade at rest, an air
// burst).
constexpr float kStruckU = 96;
bool struck_along(const LoadedMap& m, const std::unordered_map<int, FxState::Flight>& flights,
                  const sh::Vec3f& at, double normal[3], double* off_m) {
  if (m.bsp.world.models.empty()) return false;
  const FxState::Flight* best = nullptr;
  float nearest = kStruckU * kStruckU;
  for (const auto& [e, f] : flights) {
    const float dx = f.at.x - at.x, dy = f.at.y - at.y, dz = f.at.z - at.z;
    const float d2 = dx * dx + dy * dy + dz * dz;
    if (d2 < nearest && (f.dir.x != 0 || f.dir.y != 0 || f.dir.z != 0)) {
      nearest = d2;
      best = &f;
    }
  }
  if (best == nullptr) return false;
  const sh::Hull& hull = m.bsp.world.models[0].hulls[0];
  const sh::Vec3f to = {at.x + best->dir.x * kNearSurfaceU, at.y + best->dir.y * kNearSurfaceU,
                        at.z + best->dir.z * kNearSurfaceU};
  const sh::HullTrace tr = sh::trace_hull(hull, at, to);
  if (tr.startsolid || tr.fraction >= 1) return false;
  engine_dir(tr.plane.normal, normal);
  *off_m = tr.fraction * kNearSurfaceU * ge::brush::kUnitM;
  return true;
}

// The floor under a hit at `at` (map units) by a round from `shooter`: the first solid a line
// straight down from two units back along the round's line meets, as world y (m); far below when
// none is within kFloorReachU.
constexpr float kFloorReachU = 1024;
double floor_under(const LoadedMap& m, const sh::Vec3f& at, const sh::Vec3f& shooter) {
  if (m.bsp.world.models.empty()) return -1e9;
  const sh::Hull& hull = m.bsp.world.models[0].hulls[0];
  const float bx = shooter.x - at.x, by = shooter.y - at.y, bz = shooter.z - at.z;
  const float bl = std::max(std::sqrt(bx * bx + by * by + bz * bz), 1e-3f);
  const sh::Vec3f from = {at.x + bx / bl * 2, at.y + by / bl * 2, at.z + bz / bl * 2};
  const sh::HullTrace tr = sh::trace_hull(hull, from, {from.x, from.y, from.z - kFloorReachU});
  if (tr.startsolid || tr.fraction >= 1) return -1e9;
  return engine_point(tr.endpos).y;
}

// The mean colour of texture `index` (linear), off the pack's palette; kept a map. All below 0
// without the texture's pixels or a palette.
void wall_colour(FxState& s, const ge::brush::Map& m, uint32_t index, float out[3]) {
  out[0] = out[1] = out[2] = -1;
  if (const auto c = s.wall_colours.find(index); c != s.wall_colours.end()) {
    for (int k = 0; k < 3; ++k) out[k] = c->second[k];
    return;
  }
  std::array<float, 3> mean = {-1, -1, -1};
  if (index < m.textures.size() && !m.textures[index].indexed.empty() && s.palette.size() >= 768) {
    double sum[3] = {0, 0, 0};
    const std::vector<uint8_t>& px = m.textures[index].indexed;
    for (const uint8_t i : px)
      for (int k = 0; k < 3; ++k) {
        const double c = s.palette[size_t(i) * 3 + size_t(k)] / 255.0;
        sum[k] += c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
      }
    for (int k = 0; k < 3; ++k) mean[k] = static_cast<float>(sum[k] / double(px.size()));
  }
  s.wall_colours[index] = mean;
  for (int k = 0; k < 3; ++k) out[k] = mean[k];
}

// Two unit ways square to `n` and to each other.
void tangents(const double n[3], double a[3], double b[3]) {
  const double up[3] = {std::abs(n[1]) < 0.9 ? 0.0 : 1.0, std::abs(n[1]) < 0.9 ? 1.0 : 0.0, 0};
  a[0] = up[1] * n[2] - up[2] * n[1];
  a[1] = up[2] * n[0] - up[0] * n[2];
  a[2] = up[0] * n[1] - up[1] * n[0];
  const double al = std::max(ge::core::hypot3(a[0], a[1], a[2]), 1e-9);
  for (int k = 0; k < 3; ++k) a[k] /= al;
  b[0] = n[1] * a[2] - n[2] * a[1];
  b[1] = n[2] * a[0] - n[0] * a[2];
  b[2] = n[0] * a[1] - n[1] * a[0];
}

// `v` (a model's own frame) turned by the model's rotation `q` (x, y, z, w) into the world's.
void turn_by(const float q[4], const double v[3], double out[3]) {
  const double x = q[0], y = q[1], z = q[2], w = q[3];
  const double t[3] = {2 * (y * v[2] - z * v[1]), 2 * (z * v[0] - x * v[2]),
                       2 * (x * v[1] - y * v[0])};
  out[0] = v[0] + w * t[0] + (y * t[2] - z * t[1]);
  out[1] = v[1] + w * t[1] + (z * t[0] - x * t[2]);
  out[2] = v[2] + w * t[2] + (x * t[1] - y * t[0]);
}

// Triangle `k` of a body's skin (nine floats a triangle, its model's frame) in the world: its
// corners into `c`.
void skin_corners(const std::vector<float>& skin, size_t k, const double at[3], const float q[4],
                  double c[3][3]) {
  for (int i = 0; i < 3; ++i) {
    const double local[3] = {skin[k * 9 + size_t(i) * 3], skin[k * 9 + size_t(i) * 3 + 1],
                             skin[k * 9 + size_t(i) * 3 + 2]};
    double o[3];
    turn_by(q, local, o);
    for (int j = 0; j < 3; ++j) c[i][j] = at[j] + o[j];
  }
}

// The map's world faces (no sky, no liquid) as the marks' receivers, in engine metres, each
// triangle wound to face the way its face's normal does.
std::shared_ptr<const MarkFaces> receivers_of(const ge::brush::Map& m) {
  namespace gd = ge::detail;
  if (m.models.empty()) return nullptr;
  auto faces = std::make_shared<MarkFaces>();
  gd::TriMesh& mesh = faces->mesh;
  const ge::brush::Model& world = m.models[0];
  const uint32_t end = std::min<uint32_t>(world.first_surface + world.surface_count,
                                          static_cast<uint32_t>(m.surfaces.size()));
  for (uint32_t si = world.first_surface; si < end; ++si) {
    const ge::brush::Surface& sf = m.surfaces[si];
    if (sf.kind == ge::brush::SurfaceKind::sky || sf.kind == ge::brush::SurfaceKind::liquid)
      continue;
    if (sf.texture < m.textures.size() &&
        (m.textures[sf.texture].sky || m.textures[sf.texture].liquid != ge::brush::Liquid::none))
      continue;
    for (uint32_t t = 0; t + 2 < sf.count; t += 3) {
      gd::Vec3 c[3];
      for (int k = 0; k < 3; ++k) {
        double e[3];
        ge::brush::to_engine(m.vertices[m.indices[sf.first + t + k]].pos, e);
        c[k] = {e[0], e[1], e[2]};
      }
      double n[3];
      ge::brush::to_engine(m.vertices[m.indices[sf.first + t]].nrm, n);
      const gd::Vec3 front = gd::cross(gd::sub(c[1], c[0]), gd::sub(c[2], c[0]));
      if (front.x * n[0] + front.y * n[1] + front.z * n[2] < 0) std::swap(c[1], c[2]);
      for (const gd::Vec3& v : c) {
        mesh.idx.push_back(static_cast<uint32_t>(mesh.pos.size()));
        mesh.pos.push_back(v);
      }
      mesh.tag.push_back(0);
    }
  }
  faces->receivers = gd::bin_receivers(mesh, 1);  // points into faces->mesh, kept beside it
  return faces;
}

// A mark pressed onto the receivers (nothing without them).
void mark(FxState& s, ge::detail::DecalKind kind, const double at[3], const double n[3],
          double size_m, double spin, uint32_t variant, double t) {
  if (!s.receivers) return;
  ge::detail::DecalAt d;
  d.at = {at[0], at[1], at[2]};
  d.normal = {n[0], n[1], n[2]};
  d.size_m = size_m;
  d.spin_rad = spin;
  d.now_s = t;
  d.variant = variant % kMarkVariants;
  d.kind = kind;
  ge::detail::place_decal(s.marks, s.receivers->receivers, d);
}

// The marks as the beams' pass's burns: each one's soot and its embers, its masks' cell of the
// atlas.
void burns_of(const FxState& s, double t, std::vector<ge::render::BurnVertex>& out) {
  out.clear();
  if (!s.receivers) return;
  for (uint32_t i = 0; i < s.marks.capacity; ++i) {
    const ge::detail::Decal& d = s.marks.decals[i];
    if (!d.live) continue;
    const ge::detail::DecalLook look = ge::detail::look_at(d.kind, t - d.born_s);
    const float soot = static_cast<float>(look.opacity * (1 - (look.r + look.g + look.b) / 3));
    // A fresh scorch's embers at their hottest (look_at's 2.6) are kHotEmberCd.
    const float ember = static_cast<float>(look.er / 2.6) * kHotEmberCd;
    for (const ge::detail::DecalVertex& v : ge::detail::decal_verts(s.marks, i)) {
      ge::render::BurnVertex b;
      b.at[0] = v.pos.x, b.at[1] = v.pos.y, b.at[2] = v.pos.z;
      b.u = static_cast<float>((d.variant + v.u) / kMarkVariants);
      b.v = static_cast<float>((static_cast<int>(d.kind) + v.v) / ge::detail::kDecalKinds);
      b.fade = static_cast<float>(v.fade);
      b.soot = soot;
      b.ember = ember;
      b.relief = d.kind == ge::detail::DecalKind::bullet_hole ? 1.0f : 0.0f;
      out.push_back(b);
    }
  }
}

}  // namespace

void warp_dust(const Warp& w, const fx::Dust& d, double was, double age, uint32_t seed,
               fx::Particles* out) {
  if (w.arriving) {
    fx::dust_in(d, was, age, arrival_time, seed, out);
    return;
  }
  fx::dust_off(d, warp_shown(w, std::max(was, 0.0)), warp_shown(w, age), seed, out);
}

fx::Dust player_dust(const sh::Vec3f& origin, const sh::Vec3f& angles, int entity) {
  fx::Dust d;
  const ge::camera::Vec3 at = engine_point(origin);
  put(at, d.at);
  const float ang[3] = {angles.x, angles.y, angles.z};
  models::rotation_of(ang, true, d.rotation);
  const double yaw = double(angles.y) * 3.14159265358979323846 / 180;
  const ge::camera::Vec3 ahead = engine_point(
      {origin.x + float(64 * std::cos(yaw)), origin.y + float(64 * std::sin(yaw)), origin.z});
  double f[3] = {ahead.x - at.x, 0, ahead.z - at.z};
  const double fl = std::sqrt(f[0] * f[0] + f[2] * f[2]);
  for (double& c : f) c = fl > 0 ? c / fl : 0;
  if (fl == 0) f[2] = 1;
  for (int k = 0; k < 3; ++k) {
    d.facing[k] = f[k];
    d.wind[k] = -f[k];
  }
  d.low = kBoxLowUnits * ge::brush::kUnitM;
  d.high = (kBoxHighUnits - 4) * ge::brush::kUnitM;  // the head's top, under the box's
  // The sweep: ahead and up, so the face and the head lead.
  constexpr double kAhead = 0.894427191, kUp = 0.447213595;
  for (int k = 0; k < 3; ++k) d.warp.axis[k] = float(f[k] * kAhead + (k == 1 ? kUp : 0));
  d.warp.low = float(-d.half_depth * kAhead + d.low * kUp);
  d.warp.high = float(d.half_depth * kAhead + d.high * kUp);
  d.warp.seed = float(uint32_t(entity) % 16u) * 1.37f;
  return d;
}

const std::vector<uint8_t>& mark_atlas() {
  static const std::vector<uint8_t> kAtlas = ge::detail::bake_atlas(kMarkAtlasPx, kMarkVariants);
  return kAtlas;
}

FxSwitches fx_switches(const look::Picture& picture) {
  FxSwitches s;
  if (look::look_of(picture) == look::Look::classic) return s;
  s.muzzles = picture.ce.muzzles;
  s.explosions = picture.ce.explosions;
  s.trails = picture.ce.trails;
  s.beams = picture.ce.beams;
  s.hits = picture.ce.hits;
  s.decals = picture.ce.decals;
  s.warps = picture.ce.warps;
  s.bolt = &fx::bolt_look(static_cast<int>(picture.beam_style));
  return s;
}

namespace {

// The point on triangle `c` nearest `p`, as its place from the first corner (a, b).
void nearest_on(const double c[3][3], const double p[3], float* a, float* b, double* d2) {
  double e1[3], e2[3], w[3];
  for (int k = 0; k < 3; ++k) {
    e1[k] = c[1][k] - c[0][k];
    e2[k] = c[2][k] - c[0][k];
    w[k] = p[k] - c[0][k];
  }
  const double d11 = e1[0] * e1[0] + e1[1] * e1[1] + e1[2] * e1[2];
  const double d12 = e1[0] * e2[0] + e1[1] * e2[1] + e1[2] * e2[2];
  const double d22 = e2[0] * e2[0] + e2[1] * e2[1] + e2[2] * e2[2];
  const double w1 = w[0] * e1[0] + w[1] * e1[1] + w[2] * e1[2];
  const double w2 = w[0] * e2[0] + w[1] * e2[1] + w[2] * e2[2];
  const double det = d11 * d22 - d12 * d12;
  double u = det > 1e-12 ? (d22 * w1 - d12 * w2) / det : 0.0;
  double v = det > 1e-12 ? (d11 * w2 - d12 * w1) / det : 0.0;
  u = std::clamp(u, 0.0, 1.0);
  v = std::clamp(v, 0.0, 1.0 - u);
  double q[3];
  for (int k = 0; k < 3; ++k) q[k] = c[0][k] + u * e1[k] + v * e2[k] - p[k];
  *a = static_cast<float>(u);
  *b = static_cast<float>(v);
  *d2 = q[0] * q[0] + q[1] * q[1] + q[2] * q[2];
}

// A wound where a round struck `at` (map units; `p` the same in the world), on the body standing
// nearest it: the triangle of its skin nearest the point.
void wound(FxState& s, const std::vector<Placed>& placed, const sh::Vec3f& at, const double p[3],
           const ge::camera::Vec3& from, int drops, double t) {
  if (s.models == nullptr) return;
  const Placed* body = nullptr;
  float nearest = kWoundReachU * kWoundReachU;
  for (const Placed& m : placed) {
    if (m.entity < 1) continue;
    const float dx = m.origin.x - at.x, dy = m.origin.y - at.y, dz = m.origin.z - at.z;
    const float d2 = dx * dx + dy * dy + dz * dz;
    if (d2 < nearest) {
      nearest = d2;
      body = &m;
    }
  }
  if (body == nullptr || !body_triangles(*s.models, *body, t, s.skin) || s.skin.empty()) return;
  double origin[3];
  put(engine_point(body->origin), origin);
  float q[4];
  const float ang[3] = {body->angles.x, body->angles.y, body->angles.z};
  models::rotation_of(ang, true, q);
  Wound w;
  w.entity = body->entity;
  w.seed = s.seed++;
  w.size = 1 + 0.15f * static_cast<float>(drops - 1);
  // The round's own line, from the shooter through where it struck the body's box, onto the
  // skin: the first triangle it crosses; else the triangle nearest the point.
  double dir[3] = {p[0] - from.x, p[1] - from.y, p[2] - from.z};
  const double dl = std::max(ge::core::hypot3(dir[0], dir[1], dir[2]), 1e-9);
  for (double& k : dir) k /= dl;
  const double start[3] = {from.x, from.y, from.z};
  double first = 1e18, best = 1e18;
  bool crossed = false;
  for (size_t k = 0; k < s.skin.size() / 9; ++k) {
    double c[3][3];
    skin_corners(s.skin, k, origin, q, c);
    double e1[3], e2[3], tv[3];
    for (int j = 0; j < 3; ++j) {
      e1[j] = c[1][j] - c[0][j];
      e2[j] = c[2][j] - c[0][j];
      tv[j] = start[j] - c[0][j];
    }
    const double pv[3] = {dir[1] * e2[2] - dir[2] * e2[1], dir[2] * e2[0] - dir[0] * e2[2],
                          dir[0] * e2[1] - dir[1] * e2[0]};
    const double det = e1[0] * pv[0] + e1[1] * pv[1] + e1[2] * pv[2];
    if (std::abs(det) > 1e-12) {
      const double u = (tv[0] * pv[0] + tv[1] * pv[1] + tv[2] * pv[2]) / det;
      const double qv[3] = {tv[1] * e1[2] - tv[2] * e1[1], tv[2] * e1[0] - tv[0] * e1[2],
                            tv[0] * e1[1] - tv[1] * e1[0]};
      const double v = (dir[0] * qv[0] + dir[1] * qv[1] + dir[2] * qv[2]) / det;
      const double d = (e2[0] * qv[0] + e2[1] * qv[1] + e2[2] * qv[2]) / det;
      if (u >= 0 && v >= 0 && u + v <= 1 && d > 0 && d < first) {
        first = d;
        crossed = true;
        w.tri = static_cast<uint32_t>(k);
        w.a = static_cast<float>(u);
        w.b = static_cast<float>(v);
      }
    }
    if (crossed) continue;
    float a = 0, b = 0;
    double d2 = 0;
    nearest_on(c, p, &a, &b, &d2);
    if (d2 < best) {
      best = d2;
      w.tri = static_cast<uint32_t>(k);
      w.a = a;
      w.b = b;
    }
  }
  s.wounds.push_back(w);
  // At most kWoundsEach a body and kWoundsAll in all, the oldest let go.
  size_t on_body = 0;
  for (const Wound& o : s.wounds) on_body += o.entity == w.entity ? 1 : 0;
  if (on_body > kWoundsEach) {
    const auto oldest = std::find_if(s.wounds.begin(), s.wounds.end(),
                                     [&](const Wound& o) { return o.entity == w.entity; });
    s.wounds.erase(oldest);
  }
  if (s.wounds.size() > kWoundsAll) s.wounds.erase(s.wounds.begin());
}

constexpr double kWoundLift = 0.07;  // m toward the eye: past the particles' soft band

// A speck of a wound this frame: still, lit by the room (its `grow`, world_draw's rule for the
// smoke), alive until the next frame's takes its place.
ge::render::Particle wound_speck(const double at[3], double size, float albedo, float r, float g,
                                 float b, double dt) {
  ge::render::Particle q{};
  for (int k = 0; k < 3; ++k) q.p[k] = static_cast<float>(at[k]);
  q.life = static_cast<float>(std::max(dt, 1.0 / 240) * 1.5 + 0.004);
  q.age = q.life * 0.2f;  // past its fade-in
  q.size = static_cast<float>(size);
  q.grow = 1e-6f;  // lit by the room
  q.albedo = albedo;
  q.opacity = 1;
  q.tint[0] = r, q.tint[1] = g, q.tint[2] = b;
  return q;
}

// Every wound of a body drawn this frame on its skin as it stands: a dark hole, drops round it.
void draw_wounds(const Listen& l, FxState& s, const std::vector<Placed>& placed,
                 const ge::camera::Vec3& eye, double t, double dt, effects::Particles& out) {
  if (s.wounds.empty()) return;
  // A body come back to life (its health above nothing again, a respawn) is whole.
  for (auto it = s.wound_health.begin(); it != s.wound_health.end();) {
    const float h = field_named(l, it->first, "health");
    if (it->second <= 0 && h > 0) {
      std::erase_if(s.wounds, [&](const Wound& w) { return w.entity == it->first; });
      it = s.wound_health.erase(it);
    } else {
      it->second = h;
      ++it;
    }
  }
  for (const Wound& w : s.wounds)
    if (!s.wound_health.contains(w.entity))
      s.wound_health[w.entity] = field_named(l, w.entity, "health");
  if (s.models == nullptr) return;
  int skinned = -1;  // the body whose skin s.skin holds
  for (const Placed& m : placed) {
    if (m.entity < 1) continue;
    bool any = false;
    for (const Wound& w : s.wounds) any = any || w.entity == m.entity;
    if (!any) continue;
    if (!body_triangles(*s.models, m, t, s.skin)) continue;
    skinned = m.entity;
    double origin[3];
    put(engine_point(m.origin), origin);
    float q[4];
    const float ang[3] = {m.angles.x, m.angles.y, m.angles.z};
    models::rotation_of(ang, true, q);
    for (const Wound& w : s.wounds) {
      if (w.entity != skinned || size_t(w.tri) * 9 + 9 > s.skin.size()) continue;
      double c[3][3];
      skin_corners(s.skin, w.tri, origin, q, c);
      double e1[3], e2[3], n[3], at[3];
      for (int k = 0; k < 3; ++k) {
        e1[k] = c[1][k] - c[0][k];
        e2[k] = c[2][k] - c[0][k];
        at[k] = c[0][k] + w.a * e1[k] + w.b * e2[k];
      }
      n[0] = e1[1] * e2[2] - e1[2] * e2[1];
      n[1] = e1[2] * e2[0] - e1[0] * e2[2];
      n[2] = e1[0] * e2[1] - e1[1] * e2[0];
      const double nl = std::max(ge::core::hypot3(n[0], n[1], n[2]), 1e-9);
      for (double& k : n) k /= nl;
      double ta[3], tb[3];
      tangents(n, ta, tb);
      // Seen only on the side of the body toward the eye; lifted kWoundLift toward it, so the
      // particles' softening against the skin behind them leaves them whole.
      double to_eye[3] = {eye.x - at[0], eye.y - at[1], eye.z - at[2]};
      const double el = std::max(ge::core::hypot3(to_eye[0], to_eye[1], to_eye[2]), 1e-6);
      for (double& k : to_eye) k /= el;
      if (n[0] * to_eye[0] + n[1] * to_eye[1] + n[2] * to_eye[2] < 0) continue;
      // The hole on the skin; the drops round it, the same each frame (its seed).
      const double hole[3] = {at[0] + to_eye[0] * kWoundLift, at[1] + to_eye[1] * kWoundLift,
                              at[2] + to_eye[2] * kWoundLift};
      out.push_back(wound_speck(hole, 0.03, 0.05f, 0.6f, 0.12f, 0.1f, dt));
      ge::core::Rng r = ge::core::make_rng(w.seed);
      for (int i = 0; i < 6; ++i) {
        const double u = ge::core::range(r, -1, 1), v = ge::core::range(r, -1, 1);
        const double spread = 0.06 * w.size;
        const double d[3] = {hole[0] + (ta[0] * u + tb[0] * v) * spread,
                             hole[1] + (ta[1] * u + tb[1] * v) * spread,
                             hole[2] + (ta[2] * u + tb[2] * v) * spread};
        out.push_back(
            wound_speck(d, ge::core::range(r, 0.01, 0.025), 0.3f, 0.75f, 0.04f, 0.03f, dt));
      }
    }
  }
}

}  // namespace

void step_fx(const Listen& l, const ViewState& v, FxState& s, double t, double dt, FxFrame& out) {
  place_models(l, out.models);
  out.births.clear();
  out.lights.clear();
  out.puffs.clear();
  out.flames.clear();
  out.beams.clear();
  // The marks' receivers, made once a map (ce_decals).
  const ge::brush::Map* faces = l.map ? &l.map->bsp.map : nullptr;
  if (s.on.decals && faces != s.marked) {
    s.marked = faces;
    s.receivers = faces != nullptr ? receivers_of(*faces) : nullptr;
    s.marks = ge::detail::make_pool(kMarks);
  }
  if (!s.on.decals) {
    s.marked = nullptr;
    s.receivers.reset();
  }

  // The missiles: a trail over the stretch each flew since the last frame, a rocket's glow.
  s.next.clear();
  for (const Missile& m : v.missiles) {
    const ge::camera::Vec3 at = engine_point(m.origin);
    s.next[m.entity] = at;
    if (const auto f = s.flights.find(m.entity); f != s.flights.end()) {
      const sh::Vec3f d = {m.origin.x - f->second.at.x, m.origin.y - f->second.at.y,
                           m.origin.z - f->second.at.z};
      const float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
      s.flights_next[m.entity] = {
          m.origin, len > 0.5f ? sh::Vec3f{d.x / len, d.y / len, d.z / len} : f->second.dir};
    } else {
      s.flights_next[m.entity] = {m.origin, {0, 0, 0}};
    }
    if (!m.rocket) continue;
    double to[3];
    put(at, to);
    const auto was = s.last.find(m.entity);
    if (was != s.last.end()) {
      double from[3];
      put(was->second, from);
      if (!s.on.trails) {
        if (m.grenade)
          fx::grenade_trail(from, to, s.seed++, s.palette, &out.births);
        else
          fx::rocket_trail(from, to, s.seed++, s.palette, &out.births);
      } else if (m.grenade) {
        fx::smoke_trail(from, to, s.seed++, &out.births);
      } else {
        fx::fire_trail(from, to, s.seed++, &out.births);
        const double d[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
        const double len = ge::core::hypot3(d[0], d[1], d[2]);
        if (len > 1e-4) {
          const double dir[3] = {d[0] / len, d[1] / len, d[2] / len};
          out.flames.push_back(fx::exhaust(to, dir, static_cast<uint32_t>(m.entity), t));
        }
      }
    }
    if (!m.grenade || !s.on.trails) out.lights.push_back(fx::rocket_light(to));
  }
  std::swap(s.last, s.next);

  // The explosions: a burst once, then a light while it fades.
  if (l.explosions.size() < s.explosions_seen) {  // a new map
    s.explosions_seen = 0;
    s.lit.clear();
    s.last.clear();
    s.blasts.clear();
    s.mains.clear();
    s.flights.clear();
    s.wall_colours.clear();
    s.wounds.clear();
    s.wound_health.clear();
    s.marked = nullptr;
    s.beams.clear();
  }
  for (; s.explosions_seen < l.explosions.size(); s.explosions_seen++) {
    const ge::camera::Vec3 at = engine_point(l.explosions[s.explosions_seen]);
    double p[3];
    put(at, p);
    if (s.on.explosions) {
      // The surface it went off against, if one is within a ball's reach of it: the nearest hit
      // of six lines out of it.
      double normal[3] = {0, 1, 0}, off = 0;
      const sh::Vec3f boom = l.explosions[s.explosions_seen];
      bool against = l.map && struck_along(*l.map, s.flights, boom, normal, &off);
      if (!against) against = l.map && surface_near(*l.map, boom, normal, &off);
      const size_t first = s.blasts.size();
      fx::blast(p, against ? normal : nullptr, s.seed++, t, &s.blasts, &out.births);
      // Its scorch on the surface it went off against.
      ge::render::Scorch sc;
      if (against && s.on.decals && ge::render::scorch_of(s.blasts[first], sc)) {
        const double on[3] = {p[0] - normal[0] * off, p[1] - normal[1] * off,
                              p[2] - normal[2] * off};
        mark(s, ge::detail::DecalKind::scorch, on, normal, sc.size_m, sc.spin_rad, sc.variant, t);
      }
      s.mains.push_back(s.blasts[first]);
    } else {
      fx::explosion(p, s.seed++, s.palette, &out.births);
      s.lit.push_back({at, t, l.explosions[s.explosions_seen]});  // its light, and its sprite
    }
  }
  // The players' warps: who arrived and who left, from where each entity is now.
  s.bodies.clear();
  if (l.server) {
    const int clients = std::min(l.server->sv.num_edicts - 1, int(sh::kMaxClients));
    for (int e = 1; e <= clients; e++) {
      const sh::Edict& ed = l.server->sv.edicts[size_t(e)];
      if (ed.free || ed.modelindex <= 0) continue;
      s.bodies.push_back({e, ed.origin, field_named(l, e, "health") > 0});
    }
  }
  s.began.clear();
  watch_warps(s.warps, s.bodies, t, dt, s.began);
  const int own = l.server ? player_state(l).entity : 0;  // the local player's entity
  // A player as the window places him (the local player's own too, never drawn).
  const auto placed_of = [&](int e) {
    Placed m;
    if (l.server == nullptr) return m;  // a warp outliving its server draws nothing new
    const sh::Edict& ed = l.server->sv.edicts[size_t(e)];
    m.entity = e;
    m.modelindex = ed.modelindex;
    m.frame = int(field_named(l, e, "frame"));
    m.skin = int(field_named(l, e, "skin"));
    m.origin = ed.origin;
    m.angles = ed.angles;
    return m;
  };
  // ce_warps off (or the classic look): the original. The figure is there or gone at once, and
  // each end of the warp flashes the teleport's fog, the campaign's program throwing its own.
  if (!s.on.warps) {
    if (!l.campaign_host) {
      for (const Warp& w : s.began) {
        double p[3];
        put(engine_point(w.at), p);
        fx::teleport_splash(p, s.seed++, s.palette, &out.births);
      }
    }
    s.leaving.clear();
    s.seen.clear();
    out.held_shown = 1;
  } else {
    for (const Warp& w : s.began) {
      if (w.arriving) continue;
      // Leaving: the figure as it was drawn last frame, where it stood (the local player's own is
      // not drawn: his ash alone).
      const auto was = s.seen.find(w.entity);
      FxState::Leaving& g = s.leaving[w.entity];
      g.drawn = was != s.seen.end();
      g.figure = g.drawn ? was->second : placed_of(w.entity);
      g.figure.origin = w.at;
      if (s.models == nullptr || !body_triangles(*s.models, g.figure, t, g.skin)) g.skin.clear();
      g.dust = player_dust(w.at, g.figure.angles, w.entity);
    }
    s.seen.clear();
    for (Placed& m : out.models) {
      if (m.entity < 1 || m.entity > int(sh::kMaxClients)) continue;
      s.seen[m.entity] = m;
      const double shown = arrival_shown(s.warps, m.entity, t);
      if (shown >= 1) continue;
      m.warp = player_dust(m.origin, m.angles, m.entity).warp;
      m.warp.shown = float(shown);
    }
    out.held_shown = l.server ? float(arrival_shown(s.warps, own, t)) : 1;
    for (const Warp& w : s.warps.live) {
      const double age = t - w.t0, was = w.t0 >= t ? -1 : std::max(t - dt - w.t0, 0.0);
      if (w.arriving) {
        // Onto his body as it stands now.
        const Placed m = placed_of(w.entity);
        fx::Dust d = player_dust(m.origin, m.angles, w.entity);
        if (s.models != nullptr && body_triangles(*s.models, m, t, s.skin)) d.skin = s.skin;
        if (w.entity == own) {
          d.keep_off_eye = true;
          put(v.pose.eye, d.eye);
        }
        warp_dust(w, d, was, age, s.seed++, &out.births);
        continue;
      }
      const auto leaving = s.leaving.find(w.entity);
      if (leaving == s.leaving.end()) continue;
      const FxState::Leaving& g = leaving->second;
      fx::Dust d = g.dust;
      d.skin = g.skin;
      warp_dust(w, d, was, age, s.seed++, &out.births);
      if (!g.drawn) continue;
      Placed m = g.figure;
      m.entity = kLeavingBase - w.entity;
      m.warp = g.dust.warp;
      m.warp.shown = float(warp_shown(w, t));
      out.models.push_back(m);
    }
    std::erase_if(s.leaving, [&](const auto& kv) {
      return std::none_of(s.warps.live.begin(), s.warps.live.end(),
                          [&](const Warp& w) { return w.entity == kv.first && !w.arriving; });
    });
  }
  std::swap(s.flights, s.flights_next);  // the missiles flying now, for the next frame's blasts
  s.flights_next.clear();
  // A campaign's other effects (its temporary entities and particle bursts; its explosions are
  // drawn above): a shot's or a spike's hit throws sparks back toward the eye, blood falls, a
  // teleport flares, lava splashes, a beam lights and sparks where it ends.
  if (l.effects.size() < s.effects_seen) s.effects_seen = 0;  // a new map
  const ge::camera::Vec3 eye = v.pose.eye;
  const PlayerState me = player_state(l);
  const sh::Vec3f eye_map = {me.origin.x + me.view_ofs.x, me.origin.y + me.view_ofs.y,
                             me.origin.z + me.view_ofs.z};
  const ge::brush::Map* map = l.map ? &l.map->bsp.map : nullptr;
  for (; s.effects_seen < l.effects.size(); s.effects_seen++) {
    const Effect& e = l.effects[s.effects_seen];
    const ge::camera::Vec3 at = engine_point(e.kind == EffectKind::beam ? e.end : e.at);
    double p[3], to_eye[3] = {eye.x - at.x, eye.y - at.y, eye.z - at.z};
    put(at, p);
    const double n = ge::core::hypot3(to_eye[0], to_eye[1], to_eye[2]);
    for (double& c : to_eye) c = n > 0 ? c / n : 0;
    if (n == 0) to_eye[1] = 1;
    // The face it landed on, seen along the line from the eye (a beam's from its root).
    bool on_face = false;       // `face` found the map's triangle
    uint32_t face_texture = 0;  // and its texture
    const auto face = [&](const sh::Vec3f& from, double normal[3], double dir[3]) {
      sh::Vec3f fn{};
      ge::detail::SurfaceClass sc = ge::detail::SurfaceClass::unknown;
      const sh::Vec3f hit = e.kind == EffectKind::beam ? e.end : e.at;
      engine_dir({hit.x - from.x, hit.y - from.y, hit.z - from.z}, dir);
      on_face = map != nullptr && face_hit(*map, from, hit, &face_texture, &fn, &sc);
      if (on_face) {
        engine_dir(fn, normal);
      } else {
        for (int k = 0; k < 3; ++k) normal[k] = -dir[k];
      }
      return sc;
    };
    switch (e.kind) {
      case EffectKind::impact:
      case EffectKind::spike: {
        double normal[3], dir[3];
        ge::detail::SurfaceClass sc = ge::detail::SurfaceClass::unknown;
        if (s.on.hits || s.on.decals) sc = face(eye_map, normal, dir);
        const bool nail = e.kind == EffectKind::spike;
        // The hole it leaves in a solid face (none in the sky or a liquid).
        if (s.on.decals && on_face && sc != ge::detail::SurfaceClass::sky &&
            sc != ge::detail::SurfaceClass::liquid) {
          // One hole a pellet where it hit; a message that stands for several pellets (a classic
          // program's one gunshot a shot) has them spread round its point as the shot's cone
          // spreads them at that range.
          const int holes = nail ? 1 : std::clamp(e.count, 1, 16);
          const double range =
              ge::core::hypot3(eye.x - p[0], eye.y - p[1], eye.z - p[2]) * kPelletSpread;
          double ta[3], tb[3];
          tangents(normal, ta, tb);
          for (int i = 0; i < holes; ++i) {
            const uint32_t h = s.seed++ * 2654435761u;
            const double u = holes > 1 ? ((h & 0xff) / 127.5 - 1) * range : 0;
            const double w = holes > 1 ? (((h >> 16) & 0xff) / 127.5 - 1) * range : 0;
            const double q[3] = {p[0] + ta[0] * u + tb[0] * w, p[1] + ta[1] * u + tb[1] * w,
                                 p[2] + ta[2] * u + tb[2] * w};
            mark(s, ge::detail::DecalKind::bullet_hole, q, normal, nail ? kNailHoleM : kHoleM,
                 (h >> 8) % 628 / 100.0, h >> 24, t);
          }
        }
        if (s.on.hits) {
          // A gunshot's count is its pellets (a match's); each its own hit.
          fx::WallHit w;
          for (int k = 0; k < 3; ++k) {
            w.at[k] = p[k];
            w.normal[k] = normal[k];
            w.dir[k] = dir[k];
          }
          w.surface = ge::detail::surface_name(sc);
          w.nail = nail;
          if (on_face && map != nullptr) wall_colour(s, *map, face_texture, w.colour);
          if (l.map) w.floor_y = floor_under(*l.map, e.at, eye_map);
          const int hits = nail ? 1 : std::clamp(e.count, 1, 4);
          for (int i = 0; i < hits; ++i) fx::hit(w, s.seed++, t, &s.blasts, &out.births);
        } else {
          // 20 specks a pellet, 10 a nail, 20 a big nail.
          const int specks = e.kind == EffectKind::spike ? 10 * e.count : 20 * e.count;
          fx::impact(p, specks, s.seed++, s.palette, &out.births);
        }
        break;
      }
      case EffectKind::blood: {
        const bool arc = e.code == 13 && !l.campaign_host;  // a match's beam's blood
        fx::blood(p, arc ? 50 : 20 * e.count, arc, s.seed++, s.palette, &out.births);
        if (s.on.decals && !arc) wound(s, out.models, e.at, p, eye, std::clamp(e.count, 1, 8), t);
        break;
      }
      case EffectKind::teleport:
        // A player's own warp is the dust's when it is on: its fog is not thrown over it.
        if (!s.on.warps || !near_warp(s.warps, e.at, t))
          fx::teleport_splash(p, s.seed++, s.palette, &out.births);
        break;
      case EffectKind::lava:
        fx::lava_splash(p, s.seed++, s.palette, &out.births);
        break;
      case EffectKind::beam: {
        double normal[3], dir[3];
        face(e.at, normal, dir);
        const ge::camera::Vec3 root = engine_point(e.at);
        auto held = std::find_if(s.beams.begin(), s.beams.end(), [&](const HeldBeam& b) {
          return b.entity == e.entity && b.shot.end_s < 0;
        });
        if (held == s.beams.end()) {
          HeldBeam b;
          b.entity = e.entity;
          // The classic bolt it is drawn as: the arc gun's, the big monster's, the boss's.
          b.model = e.code == 5   ? EffectModel::bolt
                    : e.code == 9 ? EffectModel::bolt3
                                  : EffectModel::bolt2;
          b.shot.born_s = t;
          b.shot.seed = s.seed++;
          s.beams.push_back(b);
          held = s.beams.end() - 1;
        }
        put(root, held->shot.from);
        put(at, held->shot.to);
        for (int k = 0; k < 3; ++k) held->shot.normal[k] = normal[k];
        held->seen = t;
        // The classic roots the player's own bolt at his origin, under the gun.
        held->map_from = e.entity == me.entity ? me.origin : e.at;
        held->map_to = e.end;
        if (s.on.beams) fx::bolt_hit(p, normal, s.seed++, &out.births);
        break;
      }
      case EffectKind::explosion:
        break;
    }
  }
  // The enhanced blasts and puffs while they are seen, their main balls' light while it lasts.
  std::erase_if(s.blasts, [&](const ge::render::Blast& b) {
    return t - b.born_s > ge::render::blast_life(b) || t < b.born_s - 1;
  });
  out.puffs.insert(out.puffs.end(), s.blasts.begin(), s.blasts.end());
  std::erase_if(s.mains, [&](const ge::render::Blast& b) {
    fx::Light light;
    if (!fx::blast_light(b, t, &light)) return t - b.born_s > 0.1;
    out.lights.push_back(light);
    return false;
  });
  // The beams: let go once the program stops sending one, gone once faded; their lights.
  for (HeldBeam& b : s.beams)
    if (b.shot.end_s < 0 && t - b.seen > kBeamHoldS) b.shot.end_s = b.seen + kBeamHoldS;
  std::erase_if(s.beams, [&](const HeldBeam& b) {
    const double release = s.on.beams && b.look != nullptr ? b.look->release_s : 0;
    return b.shot.end_s >= 0 && t > b.shot.end_s + release;
  });
  ge::core::Rng roll = ge::core::make_rng(s.seed++);  // the classic bolt's segments' roll
  int segment = 0;
  for (HeldBeam& b : s.beams) {
    if (s.on.beams) {
      // Its burns where it lands, a few to fill a sweep's gap.
      ge::render::BeamBurn burn;
      for (int i = 0; s.on.decals && b.look != nullptr && i < 4 &&
                      ge::render::beam_burns(*b.look, b.shot, t, b.trail, burn);
           ++i)
        mark(s, ge::detail::DecalKind::scorch, burn.at, burn.normal, burn.size_m, burn.spin_rad,
             burn.variant, burn.born_s);
      out.beams.push_back(b);
      ge::render::LocalLight ll[ge::render::kBeamLights];
      const int n = ge::render::beam_lights(b.look ? *b.look : fx::bolt_look(), b.shot, t, ll);
      for (int i = 0; i < n; ++i) out.lights.push_back(fx::dynamic_light(ll[i]));
      continue;
    }
    // The classic bolt: its model every kBoltSegment units from the root to the end, each turned
    // to a fresh roll every frame (the bolt's crackle).
    const sh::Vec3f d = {b.map_to.x - b.map_from.x, b.map_to.y - b.map_from.y,
                         b.map_to.z - b.map_from.z};
    const double len = ge::core::hypot3(d.x, d.y, d.z);
    if (len < 1) continue;
    const float yaw = static_cast<float>(std::atan2(d.y, d.x) * 180 / ge::core::kPi);
    const float pitch =
        static_cast<float>(std::atan2(d.z, std::sqrt(d.x * d.x + d.y * d.y)) * 180 / ge::core::kPi);
    for (int i = 0; i * kBoltSegment < len; ++i) {
      Placed seg;
      seg.entity = kBoltEntities - segment++;
      seg.modelindex = effect_model(l, b.model);
      const float k = static_cast<float>(i * kBoltSegment / len);
      seg.origin = {b.map_from.x + d.x * k, b.map_from.y + d.y * k, b.map_from.z + d.z * k};
      seg.angles = {pitch, yaw, static_cast<float>(ge::core::range(roll, 0, 360))};
      out.models.push_back(seg);
    }
  }
  burns_of(s, t, out.burns);
  if (s.on.decals)
    draw_wounds(l, s, out.models, v.pose.eye, t, dt, out.births);
  else
    s.wounds.clear();
  std::erase_if(s.lit, [&](const Blast& b) {
    return t - b.t > std::max(fx::kExplosionLight, kSpriteFrames / kSpriteFps);
  });
  int sprite = 0;
  for (const Blast& b : s.lit) {
    double p[3];
    put(b.at, p);
    fx::Light light;
    if (fx::explosion_light(p, t - b.t, &light)) out.lights.push_back(light);
    // The pack's explosion sprite, its frames at ten a second.
    const int frame = static_cast<int>((t - b.t) * kSpriteFps);
    if (frame >= kSpriteFrames) continue;
    Placed spr;
    spr.entity = kSpriteEntities - sprite++;
    spr.modelindex = effect_model(l, EffectModel::explosion);
    spr.frame = frame;
    spr.origin = b.map_at;
    out.models.push_back(spr);
  }

  // The powers held: a glow round each holder (blue the quad's, red the shield's), the local
  // player's own among them.
  const PlayerState p = player_state(l);
  const auto glow = [&](int effects, const sh::Vec3f& at) {
    if (!(effects & kGlow)) return;
    fx::Light light;
    put(engine_point(at), light.pos);
    light.radius = 6;
    const bool blue = (effects & kGlowBlue) != 0, red = (effects & kGlowRed) != 0;
    light.colour[0] = red ? 1.6 : blue ? 0.35 : 1.0;
    light.colour[1] = red && blue ? 0.5 : blue || red ? 0.35 : 0.9;
    light.colour[2] = blue ? 1.6 : red ? 0.35 : 0.7;
    out.lights.push_back(light);
  };
  for (const Placed& m : out.models) glow(m.effects, m.origin);
  const int mine = int(field_named(l, p.entity, "effects"));
  glow(mine, p.origin);

  // The screen: the liquid round the eye, and a hit's flash from the blows reported (a campaign)
  // or the health lost (a match).
  s.hurt = fx::fade_hurt(s.hurt, dt);
  s.armour = fx::fade_hurt(s.armour, dt);
  if (l.blows.size() < s.blows_seen) s.blows_seen = 0;  // a new map
  if (l.campaign_host) {
    for (; s.blows_seen < l.blows.size(); s.blows_seen++) {
      s.hurt = fx::take_hurt(s.hurt, l.blows[s.blows_seen].blood);
      s.armour = fx::take_hurt(s.armour, l.blows[s.blows_seen].armor);
    }
  } else if (!p.dead && p.health < s.health) {
    s.hurt = fx::take_hurt(s.hurt, s.health - p.health);
  }
  s.health = p.health;
  fx::Screen screen;
  screen.hurt = s.hurt;
  screen.armour = s.armour;
  screen.power = (mine & kGlowBlue) != 0;
  screen.shield = (mine & kGlowRed) != 0;
  if (field_named(l, p.entity, "waterlevel") >= kEyesUnder) {
    const float type = field_named(l, p.entity, "watertype");
    screen.medium = type == kLava    ? fx::Medium::lava
                    : type == kSlime ? fx::Medium::slime
                    : type == kWater ? fx::Medium::water
                                     : fx::Medium::air;
  }
  fx::screen_blend(screen, out.blend);
  if (screen.medium == fx::Medium::lava || screen.medium == fx::Medium::slime)
    screen.medium = fx::Medium::air;
  fx::screen_blend(screen, out.blend_dry);
}

bool load_held_fire(HeldFire& f, std::string* error) {
  std::vector<fx::FireRow> rows;
  for (const view::WeaponLook& look : view::weapon_looks())
    rows.push_back({look.muzzle, look.muzzle_knobs});
  return fx::fire_looks(rows, view::kBuiltScale, f.looks, error);
}

void fire_held(HeldFire& f, const view::HandFrame& hand, bool on, double now_s, FxFrame& out) {
  const auto w = static_cast<size_t>(hand.weapon);
  const ge::render::MuzzleLook* look = on && w < f.looks.size() ? &f.looks[w] : nullptr;
  ge::render::MuzzlePose pose;
  for (int k = 0; k < 3; ++k) {
    pose.at[k] = hand.muzzle_at[k];
    pose.dir[k] = hand.muzzle_dir[k];
    pose.up[k] = hand.muzzle_up[k];
  }
  fx::step_muzzle(f.shots, look, hand.fired && hand.drawn, pose, now_s, &out.births, &out.puffs,
                  &out.flames);
  if (hand.drawn)
    for (HeldBeam& b : out.beams)
      if (b.entity == 1)
        for (int k = 0; k < 3; ++k) b.shot.from[k] = hand.muzzle_at[k];
}

}  // namespace shooters_edge::client
