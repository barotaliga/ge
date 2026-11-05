// The window's effects each frame (world_fx.hpp).
//
// ge:summary The window's effects each frame: trails, explosions, their lights, the screen's blend
// ge:keywords world-fx step_fx trail explosion screen-blend warp ghost
// ge:skill render shooter
#include "shooters_edge/client/world_fx.hpp"

#include <algorithm>
#include <cmath>

#include "core/math.hpp"
#include "shooters_edge/client/view.hpp"

namespace shooters_edge::client {

namespace fx = shooters_edge::effects;

namespace {

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

void step_fx(const Listen& l, const ViewState& v, FxState& s, double t, double dt, FxFrame& out) {
  place_models(l, out.models);
  out.births.clear();
  out.lights.clear();
  out.puffs.clear();
  out.flames.clear();

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
      fx::rocket_trail(from, to, s.seed++, &out.births);
    }
    out.lights.push_back(fx::rocket_light(to));
  }
  std::swap(s.last, s.next);

  // The explosions: a burst once, then a light while it fades.
  if (l.explosions.size() < s.explosions_seen) {  // a new map
    s.explosions_seen = 0;
    s.lit.clear();
    s.last.clear();
  }
  for (; s.explosions_seen < l.explosions.size(); s.explosions_seen++) {
    const ge::camera::Vec3 at = engine_point(l.explosions[s.explosions_seen]);
    double p[3];
    put(at, p);
    fx::explosion(p, s.seed++, &out.births);
    s.lit.push_back({at, t});
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
  const int me = l.server ? player_state(l).entity : 0;
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
  if (!s.enhanced_warps) {
    if (!l.campaign_host) {
      for (const Warp& w : s.began) {
        const sh::Vec3f feet{w.at.x, w.at.y, w.at.z + kBoxLowUnits};
        double p[3];
        put(engine_point(feet), p);
        fx::teleport_splash(p, s.seed++, &out.births);
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
    out.held_shown = l.server ? float(arrival_shown(s.warps, me, t)) : 1;
    for (const Warp& w : s.warps.live) {
      const double age = t - w.t0, was = w.t0 >= t ? -1 : std::max(t - dt - w.t0, 0.0);
      if (w.arriving) {
        // Onto his body as it stands now.
        const Placed m = placed_of(w.entity);
        fx::Dust d = player_dust(m.origin, m.angles, w.entity);
        if (s.models != nullptr && body_triangles(*s.models, m, t, s.skin)) d.skin = s.skin;
        if (w.entity == me) {
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
  for (; s.effects_seen < l.effects.size(); s.effects_seen++) {
    const Effect& e = l.effects[s.effects_seen];
    const ge::camera::Vec3 at = engine_point(e.kind == EffectKind::beam ? e.end : e.at);
    double p[3], to_eye[3] = {eye.x - at.x, eye.y - at.y, eye.z - at.z};
    put(at, p);
    const double n = ge::core::hypot3(to_eye[0], to_eye[1], to_eye[2]);
    for (double& c : to_eye) c = n > 0 ? c / n : 0;
    if (n == 0) to_eye[1] = 1;
    switch (e.kind) {
      case EffectKind::impact:
      case EffectKind::spike:
        fx::impact(p, to_eye, s.seed++, &out.births);
        break;
      case EffectKind::blood: {
        const double away[3] = {-to_eye[0], -to_eye[1], -to_eye[2]};
        fx::blood(p, away, s.seed++, &out.births);
        break;
      }
      case EffectKind::teleport:
        // A player's own warp is the dust's when it is on: its fog is not thrown over it.
        if (!s.enhanced_warps || !near_warp(s.warps, e.at, t))
          fx::teleport_splash(p, s.seed++, &out.births);
        break;
      case EffectKind::lava:
        fx::lava_splash(p, s.seed++, &out.births);
        break;
      case EffectKind::beam: {
        fx::impact(p, to_eye, s.seed++, &out.births);
        out.lights.push_back(fx::muzzle_light(p));
        break;
      }
      case EffectKind::explosion:
        break;
    }
  }

  std::erase_if(s.lit, [&](const Blast& b) { return t - b.t > fx::kExplosionLight; });
  for (const Blast& b : s.lit) {
    double p[3];
    put(b.at, p);
    fx::Light light;
    if (fx::explosion_light(p, t - b.t, &light)) out.lights.push_back(light);
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
}

}  // namespace shooters_edge::client
