// Quake II's screen tint (q2_blend.hpp states the rules).
//
// ge:summary Quake II's screen tint, worked out a frame at a time for each player
// ge:keywords q2 blend screen-tint damage-flash pickup-flash
// ge:skill shooter
// ge:test tests/shooters_edge_q2_fight_test.cpp
#include "shooters_edge/server/q2_blend.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "brush/trace.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {

namespace {

constexpr int32_t kSolid = 1, kLava = 8, kSlime = 16, kWater = 32;

// The frames of 0.1 s a power has left (0 when it is not held).
int64_t frames_left(double until, double now) {
  return until > now ? static_cast<int64_t>(std::floor((until - now) * 10 + 0.5)) : 0;
}

}  // namespace

void blend_frame(Q2Server& s, int slot) {
  Q2Player& p = s.players[slot];
  float b[4] = {0, 0, 0, 0};
  const auto lay = [&b](float r, float g, float bl, float a) {
    if (a <= 0) return;
    const float now = b[3] + (1 - b[3]) * a;
    const float keep = b[3] / now;
    b[0] = b[0] * keep + r * (1 - keep);
    b[1] = b[1] * keep + g * (1 - keep);
    b[2] = b[2] * keep + bl * (1 - keep);
    b[3] = now;
  };
  // What is at the eye.
  const float eye[3] = {float(p.move.origin[0]) * 0.125f + p.seen.offset.x,
                        float(p.move.origin[1]) * 0.125f + p.seen.offset.y,
                        float(p.move.origin[2]) * 0.125f + p.seen.offset.z};
  const int32_t at = s.tracer != nullptr ? s.tracer->contents(eye) : 0;
  if ((at & (kSolid | kLava)) != 0)
    lay(1, 0.3f, 0, 0.6f);
  else if ((at & kSlime) != 0)
    lay(0, 0.1f, 0.05f, 0.6f);
  else if ((at & kWater) != 0)
    lay(0.5f, 0.3f, 0.2f, 0.4f);
  // One power, the first held, blinking in its last 3 s.
  struct Power {
    double until;
    float r, g, bl, a;
  };
  const Power powers[] = {{p.quad_until, 0, 0, 1, 0.08f},
                          {p.invulnerable_until, 1, 1, 0, 0.08f},
                          {p.suit_until, 0, 1, 0, 0.08f},
                          {p.breather_until, 0.4f, 1, 0.4f, 0.04f}};
  for (const Power& w : powers) {
    const int64_t left = frames_left(w.until, s.level_time);
    if (left <= 0) continue;
    if (left > 30 || (left & 4) != 0) lay(w.r, w.g, w.bl, w.a);
    break;
  }
  // The blows this frame, then the flashes.
  const int real = p.blow_parts[0] + p.blow_parts[1] + p.blow_parts[2];
  if (real > 0) {
    const int count = std::max(real, 10);
    p.damage_alpha = std::clamp(std::max(p.damage_alpha, 0.0f) + float(count) * 0.01f, 0.2f, 0.6f);
    const float blood = float(p.blow_parts[0]) / float(real);
    const float armour = float(p.blow_parts[1]) / float(real);
    const float power = float(p.blow_parts[2]) / float(real);
    p.damage_rgb[0] = blood + armour;
    p.damage_rgb[1] = armour + power;
    p.damage_rgb[2] = armour;
  }
  for (int& part : p.blow_parts) part = 0;
  if (p.taken.size() < p.taken_seen) p.taken_seen = 0;  // a new life's list
  if (p.taken.size() > p.taken_seen) p.bonus_alpha = 0.25f;
  p.taken_seen = p.taken.size();
  lay(p.damage_rgb[0], p.damage_rgb[1], p.damage_rgb[2], p.damage_alpha);
  lay(0.85f, 0.7f, 0.3f, p.bonus_alpha);
  p.damage_alpha = std::max(0.0f, p.damage_alpha - 0.06f);
  p.bonus_alpha = std::max(0.0f, p.bonus_alpha - 0.1f);
  for (int k = 0; k < 4; k++) p.blend[k] = b[k];
}

}  // namespace shooters_edge::server
