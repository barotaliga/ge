// THE WOUNDS ON THE BODY (ce_blood, client/world_fx.hpp § The blood): where a blow crosses a
// body's skin and, for one that goes through, where it comes out (`wound`); the wounds drawn on the
// skin as it stands (`draw_wounds`, ce_decals) and healed when the body comes back to life
// (`heal_wounds`); and each wound's blood on the body (`body_blood`): the patch soaking out into
// the cloth, the run down it, drying dark, as render/morph.hpp's MorphBlood in the wound's place on
// the model's rest frame, and the drops a fresh wound lets fall (world_fx_blood.cpp's `let_drop`).
//
// ge:summary A struck body's wounds: entry and exit on its skin, their blood soaking and running
// ge:keywords wound body-blood morph-blood exit-wound entry-wound skin_corners heal_wounds
// ge:keywords draw_wounds soak run-down drops ce_blood
// ge:skill render shooter
// ge:test tests/shooters_edge_weapon_fx_test.cpp
#include <algorithm>
#include <cmath>

#include "core/math.hpp"
#include "core/prng.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/world_fx.hpp"

namespace shooters_edge::client {

namespace {

void put(const ge::camera::Vec3& v, double out[3]) {
  out[0] = v.x;
  out[1] = v.y;
  out[2] = v.z;
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

}  // namespace

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

}  // namespace

// A wound where a round struck `at` (map units; `p` the same in the world), on the body standing
// nearest it: where the round's line from `from` first crosses its skin, else the triangle of its
// skin nearest the point. A blow `cut` through the body wounds it again where the line last
// crosses the skin (the exit), into `exit_at` (world, m); true when it did.
bool wound(FxState& s, const std::vector<Placed>& placed, const sh::Vec3f& at, const double p[3],
           const ge::camera::Vec3& from, int drops, double t, const WoundCut& cut,
           double exit_at[3]) {
  if (s.models == nullptr) return false;
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
  if (body == nullptr || !body_triangles(*s.models, *body, t, s.skin) || s.skin.empty())
    return false;
  double origin[3];
  put(engine_point(body->origin), origin);
  float q[4];
  const float ang[3] = {body->angles.x, body->angles.y, body->angles.z};
  models::rotation_of(ang, true, q);
  Wound w;
  w.entity = body->entity;
  w.seed = s.seed++;
  w.size = 1 + 0.15f * static_cast<float>(drops - 1);
  w.stain = cut.stain;
  w.run = cut.run;
  w.born = t;
  Wound out = w;  // the exit
  out.seed = s.seed++;
  out.exit = true;
  out.stain = cut.stain * 1.6f;
  out.run = cut.run * 1.4f;
  // The round's own line, from the shooter through where it struck the body's box, onto the
  // skin: the first triangle it crosses (and the last, its way out); else the triangle nearest
  // the point.
  double dir[3] = {p[0] - from.x, p[1] - from.y, p[2] - from.z};
  const double dl = std::max(ge::core::hypot3(dir[0], dir[1], dir[2]), 1e-9);
  for (double& k : dir) k /= dl;
  const double start[3] = {from.x, from.y, from.z};
  double first = 1e18, last = -1, best = 1e18;
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
      if (u >= 0 && v >= 0 && u + v <= 1 && d > 0) {
        if (d < first) {
          first = d;
          w.tri = static_cast<uint32_t>(k);
          w.a = static_cast<float>(u);
          w.b = static_cast<float>(v);
        }
        if (d > last) {
          last = d;
          out.tri = static_cast<uint32_t>(k);
          out.a = static_cast<float>(u);
          out.b = static_cast<float>(v);
        }
        crossed = true;
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
  const bool exits = cut.through && crossed && last - first > kExitMinM;
  const auto keep = [&](Wound& k) {
    k.has_rest = body_rest(*s.models, *body, t, k.tri, k.a, k.b, k.rest);
    s.wounds.push_back(k);
    // At most kWoundsEach a body and kWoundsAll in all, the oldest let go.
    size_t on_body = 0;
    for (const Wound& o : s.wounds) on_body += o.entity == k.entity ? 1 : 0;
    if (on_body > kWoundsEach) {
      const auto oldest = std::find_if(s.wounds.begin(), s.wounds.end(),
                                       [&](const Wound& o) { return o.entity == k.entity; });
      s.wounds.erase(oldest);
    }
    if (s.wounds.size() > kWoundsAll) s.wounds.erase(s.wounds.begin());
  };
  keep(w);
  if (!exits) return false;
  keep(out);
  if (exit_at != nullptr)
    for (int k = 0; k < 3; ++k) exit_at[k] = start[k] + dir[k] * last;
  return true;
}

namespace {

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

}  // namespace

// A body come back to life (its health above nothing again, a respawn) is whole.
void heal_wounds(const FxScene& sc, FxState& s) {
  if (s.wounds.empty()) return;
  for (auto it = s.wound_health.begin(); it != s.wound_health.end();) {
    const float h = sc.health_of ? sc.health_of(it->first) : 0;
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
      s.wound_health[w.entity] = sc.health_of ? sc.health_of(w.entity) : 0;
}

// Every wound of a body drawn this frame on its skin as it stands: a dark hole, drops round it.
void draw_wounds(FxState& s, const std::vector<Placed>& placed, const ge::camera::Vec3& eye,
                 double t, double dt, effects::Particles& out) {
  if (s.wounds.empty() || s.models == nullptr) return;
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

namespace {

constexpr double kSoakS = 1.2;    // a wound's patch soaks out to its size, s
constexpr double kWickS = 20;     // and on by half again into the cloth round it, s
constexpr double kBodyRunS = 6;   // its blood runs down to its length, s
constexpr double kBodyDryS = 40;  // and dries dark, s (shortened by a short ce_blood_time)
constexpr double kBodyDripS = 8;  // a fresh wound lets drops fall this long, s
constexpr double kEntryDrops = 1.6, kExitDrops = 3.0;  // drops a second, fresh, from each

}  // namespace

void body_blood(const FxScene& l, FxState& s, double t, double dt, FxFrame& out) {
  out.bloodied.clear();
  if (!s.on.blood) {
    s.dropping.clear();
    s.treads.clear();
    return;
  }
  land_drops(s, t);
  tread_blood(l, s, out.models, t);
  if (s.wounds.empty() || s.models == nullptr) return;
  const double life = s.on.blood_life_s > 0 ? s.on.blood_life_s : 1e9;
  const double dry_s = std::min(kBodyDryS, 0.6 * life);
  ge::core::Rng r = ge::core::make_rng(s.seed++ * 2654435761u + 0xb0d1u);
  for (const Placed& m : out.models) {
    if (m.entity < 1) continue;
    ge::render::MorphBlood blood;
    uint32_t n = 0;
    bool skinned = false, posed = false;
    double origin[3];
    float q[4];
    // The newest wounds of the body first, up to what a draw carries.
    for (auto it = s.wounds.rbegin(); it != s.wounds.rend() && n < ge::render::kMorphWounds; ++it) {
      const Wound& w = *it;
      if (w.entity != m.entity || !w.has_rest) continue;
      const double age = std::max(0.0, t - w.born);
      ge::render::MorphWound& o = blood.wounds[n++];
      for (int k = 0; k < 3; ++k) o.at[k] = w.rest[k];
      const double soak = std::min(age / kSoakS, 1.0), run = std::min(age / kBodyRunS, 1.0);
      // Soaking out fast, then on slowly into the cloth round it.
      const double wick = std::min(age / kWickS, 1.0);
      o.size = static_cast<float>(w.stain * (0.45 + 0.55 * (1 - (1 - soak) * (1 - soak))) *
                                  (1 + 0.5 * wick));
      o.run = static_cast<float>(w.run * (1 - (1 - run) * (1 - run)));
      o.wet = static_cast<float>(1 - std::min(age / dry_s, 1.0));
      o.seed = static_cast<float>(w.seed % 1000u) / 1000.0f;
      // A fresh wound bleeds: drops let go where its run has got to, fewer as it closes.
      if (age >= kBodyDripS || dt <= 0) continue;
      const double rate =
          (w.exit ? kExitDrops : kEntryDrops) * (1 - age / kBodyDripS) * s.on.blood_amount;
      if (ge::core::next(r) >= rate * dt) continue;
      if (!posed) {
        posed = true;
        skinned = body_triangles(*s.models, m, t, s.skin);
        put(engine_point(m.origin), origin);
        const float ang[3] = {m.angles.x, m.angles.y, m.angles.z};
        models::rotation_of(ang, true, q);
      }
      if (!skinned || size_t(w.tri) * 9 + 9 > s.skin.size()) continue;
      double c[3][3];
      skin_corners(s.skin, w.tri, origin, q, c);
      double at[3];
      for (int k = 0; k < 3; ++k)
        at[k] = c[0][k] + w.a * (c[1][k] - c[0][k]) + w.b * (c[2][k] - c[0][k]);
      at[1] -= o.run;
      let_drop(s, at, t, out);
    }
    if (n > 0) out.bloodied.push_back({m.entity, blood});
  }
}
}  // namespace shooters_edge::client
