// The window's effects each frame (world_fx.hpp).
//
// ge:summary The window's effects each frame: trails, explosions, their lights, the screen's blend
// ge:keywords world-fx step_fx trail explosion screen-blend
// ge:skill render shooter
#include "shooters_edge/client/world_fx.hpp"

#include <algorithm>

#include "shooters_edge/client/view.hpp"

namespace shooters_edge::client {

namespace fx = shooters_edge::effects;

namespace {

// The mod's contents codes for the liquids (games/shooters_edge/qc/defs.qc's CONTENT_*).
constexpr float kWater = -3, kSlime = -4, kLava = -5;
constexpr float kEyesUnder = 3;  // waterlevel: the eyes under the surface

void put(const ge::camera::Vec3& v, double out[3]) {
  out[0] = v.x;
  out[1] = v.y;
  out[2] = v.z;
}

}  // namespace

void step_fx(const Listen& l, const ViewState& v, FxState& s, double t, double dt, FxFrame& out) {
  place_models(l, out.models);
  out.births.clear();
  out.lights.clear();

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

  // The screen: the liquid round the eye, and a hit's flash from the health lost.
  s.hurt = fx::fade_hurt(s.hurt, dt);
  if (!p.dead && p.health < s.health) s.hurt = fx::take_hurt(s.hurt, s.health - p.health);
  s.health = p.health;
  fx::Screen screen;
  screen.hurt = s.hurt;
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

}  // namespace shooters_edge::client
