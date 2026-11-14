// C++ mods on the sequel's rules: each mod's host over the sequel's own state, the hooks at its
// points, the services over its players and map entities (sequel_mods.hpp).
//
// ge:summary C++ mods on the sequel's rules: the hosts, the hooks and the services over its state
// ge:keywords mod mods sequel sequel-mods own_mods damage-hook death-hook services
// ge:skill mod shooter
// ge:test tests/shooters_edge_rules_mod_test.cpp
#include "shooters_edge/server/sequel_mods.hpp"

#include <memory>
#include <vector>

#include "core/num.hpp"
#include "shooters_edge/server/sequel_fight.hpp"
#include "shooters_edge/server/sequel_server.hpp"
#include "shooters_edge/sim/mod_host.hpp"

namespace shooters_edge::server {

namespace {

namespace num = ge::core::num;
using sim::ModHost;

ModHost& host_of(void* ctx) { return *static_cast<ModHost*>(ctx); }
SequelServer& rules_of(void* ctx) { return *static_cast<SequelServer*>(host_of(ctx).rules); }

// The rows: 0 the world, 1..kMaxClients the players, then the map's entities.
constexpr int kFirstEntity = 1 + sh::kMaxClients;

int32_t row_of_slot(int slot) { return slot < 0 ? 0 : slot + 1; }

sh::Vec3f unpack(const int16_t v[3]) {
  return {float(v[0]) * 0.125f, float(v[1]) * 0.125f, float(v[2]) * 0.125f};
}
void pack(int16_t out[3], const float v[3]) {
  for (int i = 0; i < 3; i++)
    out[i] = static_cast<int16_t>(num::to_int32(static_cast<double>(v[i]) * 8.0));
}
void put(float out[3], sh::Vec3f v) {
  out[0] = v.x;
  out[1] = v.y;
  out[2] = v.z;
}

// What row `e` is: a player's slot, a map entity's index, or the world; refused when it is out
// of range or not in use.
struct Row {
  int slot = -1;    // a player's slot
  int entity = -1;  // a map entity's index
  bool world = false;
};

bool row(void* ctx, int32_t e, const char* service, Row& out) {
  SequelServer& s = rules_of(ctx);
  out = {};
  if (e == 0) {
    out.world = true;
    return true;
  }
  if (e >= 1 && e < kFirstEntity) {
    if (!s.clients[e - 1].active) {
      sim::mod_refuse(host_of(ctx), service, "player " + std::to_string(e) + " is not playing");
      return false;
    }
    out.slot = e - 1;
    return true;
  }
  const int i = e - kFirstEntity;
  if (e < 0 || i >= static_cast<int>(s.entities.size())) {
    sim::mod_refuse(host_of(ctx), service, "entity " + std::to_string(e) + " is out of range");
    return false;
  }
  if (s.entities[static_cast<size_t>(i)].gone) {
    sim::mod_refuse(host_of(ctx), service, "entity " + std::to_string(e) + " is gone");
    return false;
  }
  out.entity = i;
  return true;
}

void not_kept(void* ctx, const char* service, int32_t field) {
  sim::mod_refuse(host_of(ctx), service,
                  "the sequel's rules keep no field " + std::to_string(field) +
                      " a mod may use there; they keep a player's origin, velocity, view and "
                      "health and an entity's origin, angles, velocity, box and health");
}

void not_offered(void* ctx, const char* service) {
  sim::mod_refuse(host_of(ctx), service,
                  "the sequel's rules keep their own entities and functions, so a mod may only "
                  "read and change their players and map entities there");
}

// -- the services
// ------------------------------------------------------------------------------

float svc_time(void* ctx) { return static_cast<float>(rules_of(ctx).level_time); }
float svc_frametime(void* /*ctx*/) { return 0.1f; }
int32_t svc_self(void* ctx) { return rules_of(ctx).mod_self; }
int32_t svc_other(void* ctx) { return rules_of(ctx).mod_other; }
int32_t svc_newmis(void* /*ctx*/) { return 0; }
int32_t svc_max_entities(void* ctx) {
  return kFirstEntity + static_cast<int32_t>(rules_of(ctx).entities.size());
}
int32_t svc_max_players(void* /*ctx*/) { return sh::kMaxClients; }

int32_t svc_in_use(void* ctx, int32_t e) {
  const SequelServer& s = rules_of(ctx);
  if (e == 0) return 1;
  if (e >= 1 && e < kFirstEntity) return s.clients[e - 1].active ? 1 : 0;
  const int i = e - kFirstEntity;
  if (e < 0 || i >= static_cast<int>(s.entities.size())) return 0;
  return s.entities[static_cast<size_t>(i)].gone ? 0 : 1;
}

float svc_get_float(void* ctx, int32_t e, int32_t field) {
  Row r;
  if (!row(ctx, e, "get_float", r)) return 0;
  const SequelServer& s = rules_of(ctx);
  if (r.slot >= 0) {
    const SequelPlayer& p = s.players[r.slot];
    if (field == SE_FIELD_HEALTH) return p.health;
    if (field == SE_FIELD_BUTTON0) return (p.buttons & sh::kButtonAttack) != 0 ? 1.0f : 0.0f;
  } else if (r.entity >= 0 && field == SE_FIELD_HEALTH) {
    return static_cast<float>(s.entities[static_cast<size_t>(r.entity)].health);
  }
  not_kept(ctx, "get_float", field);
  return 0;
}

void svc_set_float(void* ctx, int32_t e, int32_t field, float v) {
  Row r;
  if (!row(ctx, e, "set_float", r)) return;
  SequelServer& s = rules_of(ctx);
  if (r.slot >= 0 && field == SE_FIELD_HEALTH) {
    SequelPlayer& p = s.players[r.slot];
    if (!p.alive) {
      sim::mod_refuse(host_of(ctx), "set_float", "player " + std::to_string(e) + " is dead");
      return;
    }
    p.health = v;
    p.body.health = num::to_int32(static_cast<double>(v));
    if (p.health <= 0) fight_kill(s, r.slot, -1, 0);
    return;
  }
  if (r.entity >= 0 && field == SE_FIELD_HEALTH) {
    s.entities[static_cast<size_t>(r.entity)].health = num::to_int32(static_cast<double>(v));
    return;
  }
  not_kept(ctx, "set_float", field);
}

int32_t svc_get_int(void* ctx, int32_t e, int32_t field) {
  Row r;
  if (!row(ctx, e, "get_int", r)) return 0;
  const SequelServer& s = rules_of(ctx);
  if (r.slot >= 0) {
    const SequelPlayer& p = s.players[r.slot];
    if (field == SE_FIELD_MOVETYPE) return p.alive ? SE_MOVETYPE_WALK : SE_MOVETYPE_NONE;
    if (field == SE_FIELD_FLAGS) return p.on_ground ? sh::kFlagOnGround : 0;
  }
  not_kept(ctx, "get_int", field);
  return 0;
}

void svc_set_int(void* ctx, int32_t e, int32_t field, int32_t /*v*/) {
  Row r;
  if (!row(ctx, e, "set_int", r)) return;
  not_kept(ctx, "set_int", field);
}

void svc_get_vec(void* ctx, int32_t e, int32_t field, float out[3]) {
  out[0] = out[1] = out[2] = 0;
  Row r;
  if (!row(ctx, e, "get_vec", r)) return;
  const SequelServer& s = rules_of(ctx);
  if (r.slot >= 0) {
    const SequelPlayer& p = s.players[r.slot];
    switch (field) {
      case SE_FIELD_ORIGIN:
        return put(out, unpack(p.move.origin));
      case SE_FIELD_VELOCITY:
        return put(out, unpack(p.move.velocity));
      case SE_FIELD_ANGLES:
      case SE_FIELD_V_ANGLE:
        return put(out, p.view_angles);
      case SE_FIELD_MINS:
        return put(out, p.mins);
      case SE_FIELD_MAXS:
        return put(out, p.maxs);
      default:
        break;
    }
  } else if (r.entity >= 0) {
    const SequelEntity& m = s.entities[static_cast<size_t>(r.entity)];
    switch (field) {
      case SE_FIELD_ORIGIN:
        return put(out, m.origin);
      case SE_FIELD_ANGLES:
        return put(out, m.angles);
      case SE_FIELD_VELOCITY:
        return put(out, m.velocity);
      case SE_FIELD_MINS:
        return put(out, m.mins);
      case SE_FIELD_MAXS:
        return put(out, m.maxs);
      default:
        break;
    }
  }
  not_kept(ctx, "get_vec", field);
}

void svc_set_vec(void* ctx, int32_t e, int32_t field, const float v[3]) {
  Row r;
  if (!row(ctx, e, "set_vec", r)) return;
  SequelServer& s = rules_of(ctx);
  if (r.slot >= 0 && field == SE_FIELD_ORIGIN) {
    SequelPlayer& p = s.players[r.slot];
    pack(p.move.origin, v);
    p.changed = true;  // placed by the rules: the move takes it as it stands
    return;
  }
  if (r.slot >= 0 && field == SE_FIELD_VELOCITY) {
    pack(s.players[r.slot].move.velocity, v);
    return;
  }
  not_kept(ctx, "set_vec", field);
}

int32_t svc_spawn(void* ctx) {
  not_offered(ctx, "spawn");
  return -1;
}
void svc_remove(void* ctx, int32_t /*e*/) { not_offered(ctx, "remove"); }
void svc_set_size(void* ctx, int32_t /*e*/, const float* /*mins*/, const float* /*maxs*/) {
  not_offered(ctx, "set_size");
}
int32_t svc_function(void* ctx, uint32_t /*k*/) {
  not_offered(ctx, "function");
  return 0;
}
void svc_call_function(void* ctx, int32_t /*fn*/, int32_t /*self*/, int32_t /*other*/) {
  not_offered(ctx, "call_function");
}
void svc_sound(void* ctx, int32_t /*e*/, int32_t /*channel*/, const char* /*sample*/,
               float /*volume*/, float /*attenuation*/) {
  not_offered(ctx, "sound");
}

se_mod_services_v1 services() {
  se_mod_services_v1 g{};
  g.size = sizeof(se_mod_services_v1);
  g.api = SE_MOD_API;
  g.time = svc_time;
  g.frametime = svc_frametime;
  g.self = svc_self;
  g.other = svc_other;
  g.newmis = svc_newmis;
  g.max_entities = svc_max_entities;
  g.max_players = svc_max_players;
  g.in_use = svc_in_use;
  g.get_float = svc_get_float;
  g.set_float = svc_set_float;
  g.get_int = svc_get_int;
  g.set_int = svc_set_int;
  g.get_vec = svc_get_vec;
  g.set_vec = svc_set_vec;
  g.spawn = svc_spawn;
  g.remove = svc_remove;
  g.set_size = svc_set_size;
  g.function = svc_function;
  g.call_function = svc_call_function;
  g.sound = svc_sound;
  return g;
}

// Each mod's hook, lowest first, with `self` set to `self_row` for its services.
template <typename Get, typename... A>
void each(SequelServer& s, int32_t self_row, Get get, A... args) {
  const int32_t outer_self = s.mod_self;
  s.mod_self = self_row;
  for (const std::unique_ptr<ModHost>& h : s.mods.hosts) {
    const auto hook = get(h->hooks);
    if (hook == nullptr) continue;
    h->run.hook_calls++;
    hook(sim::mod_state(*h), &h->host, args...);
  }
  s.mod_self = outer_self;
}

}  // namespace

bool sequel_start_mods(RulesCommon& common, std::string& error) {
  SequelServer& s = static_cast<SequelServer&>(common);
  RulesMods& m = s.mods;
  m.hosts.clear();
  const se_mod_services_v1 g = services();
  for (size_t k = 0; k < m.tables.size(); k++) {
    auto h = std::make_unique<ModHost>();
    const std::vector<std::string> refused =
        sim::create_mod_host_over(*h, m.tables[k], g, &s, m.seed + static_cast<uint32_t>(k));
    if (!refused.empty()) {
      error = "the mod " + (k < m.names.size() ? m.names[k] : std::to_string(k + 1)) +
              " was refused: " + refused.front();
      m.hosts.clear();
      return false;
    }
    m.hosts.push_back(std::move(h));
  }
  each(s, 0, [](const se_mod_hooks_v1* k) {
    return GE_MOD_HAS(se_mod_hooks_v1, k, match_start) ? k->match_start : nullptr;
  });
  return true;
}

void sequel_mods_start_frame(SequelServer& s) {
  if (s.mods.hosts.empty()) return;
  each(s, 0, [](const se_mod_hooks_v1* k) {
    return GE_MOD_HAS(se_mod_hooks_v1, k, start_frame) ? k->start_frame : nullptr;
  });
}

void sequel_mods_pre_think(SequelServer& s, int slot) {
  if (s.mods.hosts.empty()) return;
  each(
      s, row_of_slot(slot),
      [](const se_mod_hooks_v1* k) {
        return GE_MOD_HAS(se_mod_hooks_v1, k, player_pre_think) ? k->player_pre_think : nullptr;
      },
      row_of_slot(slot));
}

void sequel_mods_post_think(SequelServer& s, int slot) {
  if (s.mods.hosts.empty()) return;
  each(
      s, row_of_slot(slot),
      [](const se_mod_hooks_v1* k) {
        return GE_MOD_HAS(se_mod_hooks_v1, k, player_post_think) ? k->player_post_think : nullptr;
      },
      row_of_slot(slot));
}

void sequel_mods_spawn(SequelServer& s, int slot) {
  if (s.mods.hosts.empty()) return;
  each(
      s, row_of_slot(slot),
      [](const se_mod_hooks_v1* k) {
        return GE_MOD_HAS(se_mod_hooks_v1, k, put_client_in_server) ? k->put_client_in_server
                                                                    : nullptr;
      },
      row_of_slot(slot));
}

void sequel_mods_connect(SequelServer& s, int slot) {
  if (s.mods.hosts.empty()) return;
  each(
      s, row_of_slot(slot),
      [](const se_mod_hooks_v1* k) {
        return GE_MOD_HAS(se_mod_hooks_v1, k, client_connect) ? k->client_connect : nullptr;
      },
      row_of_slot(slot));
}

void sequel_mods_disconnect(SequelServer& s, int slot) {
  if (s.mods.hosts.empty()) return;
  each(
      s, row_of_slot(slot),
      [](const se_mod_hooks_v1* k) {
        return GE_MOD_HAS(se_mod_hooks_v1, k, client_disconnect) ? k->client_disconnect : nullptr;
      },
      row_of_slot(slot));
}

int sequel_mods_damage(SequelServer& s, int target, int attacker, int points) {
  if (s.mods.hosts.empty()) return points;
  const int32_t outer_self = s.mod_self;
  s.mod_self = row_of_slot(target);
  int32_t dealt = points;
  for (const std::unique_ptr<ModHost>& h : s.mods.hosts) {
    const se_mod_hooks_v1* k = h->hooks;
    if (!GE_MOD_HAS(se_mod_hooks_v1, k, damage) || k->damage == nullptr) continue;
    h->run.hook_calls++;
    dealt =
        k->damage(sim::mod_state(*h), &h->host, row_of_slot(target), row_of_slot(attacker), dealt);
    if (dealt < 0) dealt = 0;
  }
  s.mod_self = outer_self;
  return dealt;
}

void sequel_mods_death(SequelServer& s, int slot, int killer) {
  if (s.mods.hosts.empty()) return;
  each(
      s, row_of_slot(slot),
      [](const se_mod_hooks_v1* k) {
        return GE_MOD_HAS(se_mod_hooks_v1, k, death) ? k->death : nullptr;
      },
      row_of_slot(slot), row_of_slot(killer));
}

}  // namespace shooters_edge::server
