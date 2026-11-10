// Every drawn body's burn by lava or slime (burning.hpp): the contents read, the lags stepped.
//
// ge:summary The bodies' burns stepped: each body's depth in lava or slime from the map's leaves
// ge:keywords scorches step_burning hull_contents contents lava slime depth
// ge:skill shooter render
#include "shooters_edge/client/burning.hpp"

#include <algorithm>

#include "shooter/server.hpp"
#include "shooters_edge/client/view.hpp"

namespace shooters_edge::client {

namespace gr = ge::render;
namespace br = ge::brush;

namespace {

constexpr int kSlime = -4, kLava = -5;  // the map's leaf contents
// A player's box, units over its origin: the feet, the waist, the eyes.
constexpr float kFeetUnits = -20, kEyesUnits = 22;
constexpr double kPlayerHeightM = 1.4, kPlayerRadiusM = 0.35;
constexpr double kThingHeightM = 0.5, kThingRadiusM = 0.2;  // a gib, a head, a pack
constexpr double kMostStep = 0.1;                           // s: a longer gap is a pause

}  // namespace

void step_burning(Burning& s, const br::Map& map, std::span<const Placed> placed, double t_s,
                  bool on, LevelAt level_at, const void* ctx) {
  s.births.clear();
  s.stirs.clear();
  s.fell_in.clear();
  if (s.last_s < 0 || t_s < s.last_s) s.bodies.clear();  // the first frame, or a seek back
  const double dt = std::min(std::max(t_s - s.last_s, 0.0), kMostStep);
  s.last_s = t_s;
  if (!on) {
    s.bodies.clear();
    return;
  }
  for (auto& [e, b] : s.bodies) b.seen = false;
  const gr::BodyBurnLook& look = gr::body_burn_look();
  for (const Placed& p : placed) {
    if (p.entity <= 0) continue;  // a static never burns
    const bool player = p.entity <= ge::shooter::kMaxClients;
    // How deep: the share of its samples in each liquid.
    double hot = 0, acid = 0;
    const float at[3][3] = {{p.origin.x, p.origin.y, p.origin.z + kFeetUnits},
                            {p.origin.x, p.origin.y, p.origin.z},
                            {p.origin.x, p.origin.y, p.origin.z + kEyesUnits}};
    const int samples = player ? 3 : 1;
    for (int k = 0; k < samples; ++k) {
      const int32_t c = br::hull_contents(map, at[player ? k : 1], 0, 0);
      if (c == kLava) hot += 1.0 / samples;
      if (c == kSlime) acid += 1.0 / samples;
    }
    auto found = s.bodies.find(p.entity);
    if (found == s.bodies.end()) {
      if (hot <= 0 && acid <= 0) continue;  // nothing to start
      Burning::Body fresh;
      fresh.scorch = gr::make_body_burn(static_cast<uint32_t>(p.entity) * 2654435761u);
      found = s.bodies.emplace(p.entity, fresh).first;
    }
    Burning::Body& b = found->second;
    b.seen = true;
    gr::step_body_burn(b.scorch, look, hot, acid, dt);
    const ge::camera::Vec3 o =
        engine_point({p.origin.x, p.origin.y, p.origin.z + (player ? kFeetUnits - 4 : 0.0f)});
    gr::BurnShape body;
    body.feet[0] = o.x, body.feet[1] = o.y, body.feet[2] = o.z;
    body.height_m = player ? kPlayerHeightM : kThingHeightM;
    body.radius_m = player ? kPlayerRadiusM : kThingRadiusM;
    body.heading = engine_heading(p.angles.y);
    if (dt > 0)
      for (int k = 0; k < 3; ++k) body.v[k] = (body.feet[k] - b.feet[k]) / dt;
    for (int k = 0; k < 3; ++k) b.feet[k] = body.feet[k];
    const double mid[3] = {o.x, o.y + body.height_m * 0.5, o.z};
    if (level_at != nullptr) body.level_y = level_at(ctx, mid);
    gr::body_burn_births(b.scorch, look, body, dt, s.births);
    const gr::BurnStir st = gr::body_burn_stir(b.scorch);
    if (st.heat > 0 || st.smoke > 0) {
      const double y = body.level_y > -1e8 ? body.level_y : mid[1];
      s.stirs.push_back({o.x, y, o.z, body.radius_m * 1.3, st.heat, st.smoke});
    }
    // In lava now and not a frame ago: it broke the crust where it went in.
    const bool in_lava = hot > 0;
    if (in_lava && !b.hot) {
      const double y = body.level_y > -1e8 ? body.level_y : mid[1];
      s.fell_in.push_back({o.x, y, o.z, body.radius_m * 1.8, 1, 0});
    }
    b.hot = in_lava;
  }
  // Gone from the list, or out and cold: dropped.
  std::erase_if(s.bodies, [](const auto& kv) {
    return !kv.second.seen || gr::body_burn_cold(kv.second.scorch);
  });
}

gr::MorphBurn burn_skin_of(const Burning& s, int entity) {
  const auto found = s.bodies.find(entity);
  if (found == s.bodies.end()) return {};
  const gr::BurnSkin k = gr::body_burn_skin(found->second.scorch);
  return {k.charred, k.embers, k.etched, k.seed};
}

}  // namespace shooters_edge::client
