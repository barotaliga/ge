// The window's effects each frame (world_fx.hpp).
//
// ge:summary The window's effects each frame: trails, explosions, their lights, the screen's blend
// ge:keywords world-fx step_fx trail explosion screen-blend face_hit fx_switches held-beam warp ghost
// ge:skill render shooter
#include "shooters_edge/client/world_fx.hpp"

#include <algorithm>
#include <cmath>

#include "brush/map.hpp"
#include "core/math.hpp"
#include "core/prng.hpp"
#include "detail/surfaces.hpp"
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
bool face_hit(const ge::brush::Map& m, const sh::Vec3f& from, const sh::Vec3f& at, sh::Vec3f* n,
              ge::detail::SurfaceClass* surface) {
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
// it, engine axes, unit); false in the open.
constexpr float kNearSurfaceU = 40;
bool surface_near(const LoadedMap& m, const sh::Vec3f& at, double normal[3]) {
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
  return best < 1;
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

FxSwitches fx_switches(const look::Picture& picture) {
  FxSwitches s;
  if (look::look_of(picture) == look::Look::classic) return s;
  s.muzzles = picture.ce.muzzles;
  s.explosions = picture.ce.explosions;
  s.trails = picture.ce.trails;
  s.beams = picture.ce.beams;
  s.hits = picture.ce.hits;
  s.warps = picture.ce.warps;
  s.bolt = &fx::bolt_look(static_cast<int>(picture.beam_style));
  return s;
}

void step_fx(const Listen& l, const ViewState& v, FxState& s, double t, double dt, FxFrame& out) {
  place_models(l, out.models);
  out.births.clear();
  out.lights.clear();
  out.puffs.clear();
  out.flames.clear();
  out.beams.clear();

  // The missiles: a trail over the stretch each flew since the last frame, a rocket's glow.
  s.next.clear();
  for (const Missile& m : v.missiles) {
    const ge::camera::Vec3 at = engine_point(m.origin);
    s.next[m.entity] = at;
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
    s.beams.clear();
  }
  for (; s.explosions_seen < l.explosions.size(); s.explosions_seen++) {
    const ge::camera::Vec3 at = engine_point(l.explosions[s.explosions_seen]);
    double p[3];
    put(at, p);
    if (s.on.explosions) {
      // The surface it went off against, if one is within a ball's reach of it: the nearest hit
      // of six lines out of it.
      double normal[3];
      const bool against = l.map && surface_near(*l.map, l.explosions[s.explosions_seen], normal);
      const size_t first = s.blasts.size();
      fx::blast(p, against ? normal : nullptr, s.seed++, t, &s.blasts, &out.births);
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
    const auto face = [&](const sh::Vec3f& from, double normal[3], double dir[3]) {
      sh::Vec3f fn{};
      ge::detail::SurfaceClass sc = ge::detail::SurfaceClass::unknown;
      const sh::Vec3f hit = e.kind == EffectKind::beam ? e.end : e.at;
      engine_dir({hit.x - from.x, hit.y - from.y, hit.z - from.z}, dir);
      if (map != nullptr && face_hit(*map, from, hit, &fn, &sc)) {
        engine_dir(fn, normal);
      } else {
        for (int k = 0; k < 3; ++k) normal[k] = -dir[k];
      }
      return sc;
    };
    switch (e.kind) {
      case EffectKind::impact:
      case EffectKind::spike:
        if (s.on.hits) {
          double normal[3], dir[3];
          const ge::detail::SurfaceClass sc = face(eye_map, normal, dir);
          const bool nail = e.kind == EffectKind::spike;
          // A gunshot's count is its pellets (a match's); each its own hit, spread a little.
          const int hits = nail ? 1 : std::clamp(e.count, 1, 4);
          for (int i = 0; i < hits; ++i)
            fx::hit(p, normal, dir, ge::detail::surface_name(sc), nail, s.seed++, t, &s.blasts,
                    &out.births);
        } else {
          // 20 specks a pellet, 10 a nail, 20 a big nail.
          const int specks = e.kind == EffectKind::spike ? 10 * e.count : 20 * e.count;
          fx::impact(p, specks, s.seed++, s.palette, &out.births);
        }
        break;
      case EffectKind::blood: {
        const bool arc = e.code == 13 && !l.campaign_host;  // a match's beam's blood
        fx::blood(p, arc ? 50 : 20 * e.count, arc, s.seed++, s.palette, &out.births);
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
  for (const HeldBeam& b : s.beams) {
    if (s.on.beams) {
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
