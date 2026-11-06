// A demo's temporary entities and missiles kept for the effects (demo_fx.hpp).
//
// ge:summary demo_fx's gather: each frame's temporary entities appended, missiles by model name
// ge:keywords demo-fx demo_fx_frame effect_of missile_model
// ge:skill shooter render
// ge:test tests/shooters_edge_demo_test.cpp
#include "shooters_edge/client/demo_fx.hpp"

#include <cstddef>

namespace shooters_edge::client {

Effect effect_of(const DemoEffect& d) {
  Effect e;
  e.code = d.code;
  e.entity = d.entity;
  e.count = d.count;
  e.at = d.at;
  e.end = d.end;
  switch (d.kind) {
    case DemoEffectKind::spike:
      e.kind = EffectKind::spike;
      break;
    case DemoEffectKind::gunshot:
      e.kind = EffectKind::impact;
      break;
    case DemoEffectKind::explosion:
      e.kind = EffectKind::explosion;
      break;
    case DemoEffectKind::beam:
      e.kind = EffectKind::beam;
      break;
    case DemoEffectKind::lava:
      e.kind = EffectKind::lava;
      break;
    case DemoEffectKind::teleport:
      e.kind = EffectKind::teleport;
      break;
    case DemoEffectKind::blood:
      e.kind = EffectKind::blood;
      break;
  }
  return e;
}

bool missile_model(std::string_view name, Missile& kind) {
  kind.rocket = kind.grenade = false;
  if (name == "progs/missile.mdl") {
    kind.rocket = true;
  } else if (name == "progs/grenade.mdl") {
    kind.rocket = kind.grenade = true;
  } else if (name != "progs/spike.mdl" && name != "progs/s_spike.mdl") {
    return false;
  }
  return true;
}

void demo_fx_frame(const DemoState& s, DemoFx& fx) {
  if (s.levels != fx.levels) {  // a new level: the effects start again
    fx.levels = s.levels;
    fx.explosions.clear();
    fx.effects.clear();
  }
  for (const DemoEffect& e : s.effects) {
    if (e.kind == DemoEffectKind::explosion)
      fx.explosions.push_back(e.at);
    else
      fx.effects.push_back(effect_of(e));
  }
  fx.missiles.clear();
  const std::size_t players = s.format == DemoFormat::single ? 0 : s.players.size();
  for (std::size_t n = players + 1; n < s.entities.size(); n++) {
    const DemoEntity& d = s.entities[n];
    if (!d.present || d.model <= 0 || static_cast<std::size_t>(d.model) >= s.models.size())
      continue;
    Missile m;
    if (!missile_model(s.models[static_cast<std::size_t>(d.model)], m)) continue;
    m.entity = static_cast<int>(n);
    m.origin = d.origin;
    fx.missiles.push_back(m);
  }
}

}  // namespace shooters_edge::client
