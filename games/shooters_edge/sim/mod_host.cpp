// The mod host's seam and services (mod_host.hpp states the contract).
//
// ge:summary The mod host's composite program, its checked services and the run's digest
// ge:keywords mod mod-host composite-program services refuse entity-field set_origin spawn
// ge:keywords remove call_function sound intern fnv mod_hash
// ge:skill mod shooter
// ge:test tests/shooters_edge_mod_test.cpp
#include "shooters_edge/sim/mod_host.hpp"

#include <cstddef>

#include "mod/host.hpp"

namespace shooters_edge::sim {

namespace {

// The seam's hooks, in the order of their composite numbers (kModHookBase + slot).
enum HookSlot : uint8_t {
  kStartFrame,
  kPreThink,
  kPostThink,
  kConnect,
  kPutInServer,
  kDisconnect,
  kClientKill,
  kSpectatorConnect,
  kSpectatorThink,
  kSpectatorDisconnect,
  kSetNewParms,
  kSetChangeParms,
  kHookSlots,
};

sh::Func base_hook(const sh::Program& p, int slot) {
  switch (slot) {
    case kStartFrame:
      return p.start_frame;
    case kPreThink:
      return p.player_pre_think;
    case kPostThink:
      return p.player_post_think;
    case kConnect:
      return p.client_connect;
    case kPutInServer:
      return p.put_client_in_server;
    case kDisconnect:
      return p.client_disconnect;
    case kClientKill:
      return p.client_kill;
    case kSpectatorConnect:
      return p.spectator_connect;
    case kSpectatorThink:
      return p.spectator_think;
    case kSpectatorDisconnect:
      return p.spectator_disconnect;
    case kSetNewParms:
      return p.set_new_parms;
    case kSetChangeParms:
      return p.set_change_parms;
    default:
      return sh::kNoFunc;
  }
}

ModHost& host_of(void* ctx) { return *static_cast<ModHost*>(ctx); }

void add_log(ModHost& h, std::string line) {
  if (static_cast<int>(h.log.size()) >= kModLogMax) {
    h.log_dropped++;
    return;
  }
  h.log.push_back(std::move(line));
}

void refuse(ModHost& h, const char* service, const std::string& why) {
  h.run.refusals++;
  add_log(h, std::string("refused ") + service + ": " + why);
}

// The entity `e` of the running match, in use; null (refused) otherwise.
sh::Edict* entity(ModHost& h, int32_t e, const char* service) {
  if (!h.sv) {
    refuse(h, service, "no match is running (a service is called from a hook)");
    return nullptr;
  }
  if (e < 0 || e >= h.sv->num_edicts) {
    refuse(h, service, "entity " + std::to_string(e) + " is out of range");
    return nullptr;
  }
  sh::Edict& ed = h.sv->edicts[static_cast<size_t>(e)];
  if (ed.free) {
    refuse(h, service, "entity " + std::to_string(e) + " is free");
    return nullptr;
  }
  return &ed;
}

void* state_of(ModHost& h) { return h.run.state.empty() ? nullptr : h.run.state.data(); }

// -- the engine's half: the stream and the log ---------------------------------------------------

uint32_t svc_random_u32(void* ctx) { return ge::mod::random_u32(host_of(ctx).run.rng); }
double svc_random(void* ctx) { return ge::core::next(host_of(ctx).run.rng); }
void svc_log(void* ctx, int32_t level, const char* text) {
  const char* tag = level >= GE_MOD_LOG_ERROR  ? "error: "
                    : level == GE_MOD_LOG_WARN ? "warning: "
                                               : "";
  add_log(host_of(ctx), std::string(tag) + (text ? text : ""));
}

// -- the game's services
// ---------------------------------------------------------------------------

float svc_time(void* ctx) {
  const ModHost& h = host_of(ctx);
  return h.sv ? h.sv->g.time : 0;
}
float svc_frametime(void* ctx) {
  const ModHost& h = host_of(ctx);
  return h.sv ? h.sv->g.frametime : 0;
}
int32_t svc_self(void* ctx) {
  const ModHost& h = host_of(ctx);
  return h.sv ? h.sv->g.self : 0;
}
int32_t svc_other(void* ctx) {
  const ModHost& h = host_of(ctx);
  return h.sv ? h.sv->g.other : 0;
}
int32_t svc_newmis(void* ctx) {
  const ModHost& h = host_of(ctx);
  return h.sv ? h.sv->g.newmis : 0;
}
int32_t svc_max_entities(void* ctx) {
  const ModHost& h = host_of(ctx);
  return h.sv ? h.sv->set.max_edicts : 0;
}
int32_t svc_max_players(void* ctx) {
  const ModHost& h = host_of(ctx);
  return h.sv ? h.sv->set.maxclients : 0;
}
int32_t svc_in_use(void* ctx, int32_t e) {
  const ModHost& h = host_of(ctx);
  if (!h.sv || e < 0 || e >= h.sv->num_edicts) return 0;
  return h.sv->edicts[static_cast<size_t>(e)].free ? 0 : 1;
}

float* float_field(sh::Edict& ed, int32_t field) {
  switch (field) {
    case SE_FIELD_NEXTTHINK:
      return &ed.nextthink;
    case SE_FIELD_HEALTH:
      return &ed.health;
    case SE_FIELD_GRAVITY:
      return &ed.gravity;
    case SE_FIELD_MAXSPEED:
      return &ed.maxspeed;
    case SE_FIELD_BUTTON0:
      return &ed.button0;
    case SE_FIELD_IMPULSE:
      return &ed.impulse;
    case SE_FIELD_LTIME:
      return &ed.ltime;
    default:
      return nullptr;
  }
}

int32_t* int_field(sh::Edict& ed, int32_t field) {
  switch (field) {
    case SE_FIELD_MOVETYPE:
      return &ed.movetype;
    case SE_FIELD_SOLID:
      return &ed.solid;
    case SE_FIELD_FLAGS:
      return &ed.flags;
    case SE_FIELD_OWNER:
      return &ed.owner;
    case SE_FIELD_GROUNDENTITY:
      return &ed.groundentity;
    case SE_FIELD_THINK:
      return &ed.think;
    case SE_FIELD_TOUCH:
      return &ed.touch;
    case SE_FIELD_BLOCKED:
      return &ed.blocked;
    default:
      return nullptr;
  }
}

sh::Vec3f* vec_field(sh::Edict& ed, int32_t field) {
  switch (field) {
    case SE_FIELD_ORIGIN:
      return &ed.origin;
    case SE_FIELD_VELOCITY:
      return &ed.velocity;
    case SE_FIELD_ANGLES:
      return &ed.angles;
    case SE_FIELD_AVELOCITY:
      return &ed.avelocity;
    case SE_FIELD_MINS:
      return &ed.mins;
    case SE_FIELD_MAXS:
      return &ed.maxs;
    case SE_FIELD_V_ANGLE:
      return &ed.v_angle;
    default:
      return nullptr;
  }
}

std::string field_text(int32_t field) { return "field " + std::to_string(field); }

float svc_get_float(void* ctx, int32_t e, int32_t field) {
  ModHost& h = host_of(ctx);
  sh::Edict* ed = entity(h, e, "get_float");
  if (!ed) return 0;
  const float* f = float_field(*ed, field);
  if (!f) {
    refuse(h, "get_float", field_text(field) + " is not a float field");
    return 0;
  }
  return *f;
}

void svc_set_float(void* ctx, int32_t e, int32_t field, float v) {
  ModHost& h = host_of(ctx);
  sh::Edict* ed = entity(h, e, "set_float");
  if (!ed) return;
  float* f = float_field(*ed, field);
  if (!f) {
    refuse(h, "set_float", field_text(field) + " is not a float field");
    return;
  }
  *f = v;
}

int32_t svc_get_int(void* ctx, int32_t e, int32_t field) {
  ModHost& h = host_of(ctx);
  sh::Edict* ed = entity(h, e, "get_int");
  if (!ed) return 0;
  const int32_t* f = int_field(*ed, field);
  if (!f) {
    refuse(h, "get_int", field_text(field) + " is not an int field");
    return 0;
  }
  return *f;
}

void svc_set_int(void* ctx, int32_t e, int32_t field, int32_t v) {
  ModHost& h = host_of(ctx);
  sh::Edict* ed = entity(h, e, "set_int");
  if (!ed) return;
  int32_t* f = int_field(*ed, field);
  if (!f) {
    refuse(h, "set_int", field_text(field) + " is not an int field");
    return;
  }
  *f = v;
}

void svc_get_vec(void* ctx, int32_t e, int32_t field, float out[3]) {
  ModHost& h = host_of(ctx);
  out[0] = out[1] = out[2] = 0;
  sh::Edict* ed = entity(h, e, "get_vec");
  if (!ed) return;
  const sh::Vec3f* v = vec_field(*ed, field);
  if (!v) {
    refuse(h, "get_vec", field_text(field) + " is not a vector field");
    return;
  }
  out[0] = v->x;
  out[1] = v->y;
  out[2] = v->z;
}

void svc_set_vec(void* ctx, int32_t e, int32_t field, const float in[3]) {
  ModHost& h = host_of(ctx);
  sh::Edict* ed = entity(h, e, "set_vec");
  if (!ed) return;
  if (field == SE_FIELD_MINS || field == SE_FIELD_MAXS) {
    refuse(h, "set_vec", "the box is set with set_size");
    return;
  }
  const sh::Vec3f v = {in[0], in[1], in[2]};
  if (field == SE_FIELD_ORIGIN) {
    sh::set_origin(*h.sv, e, v);
    return;
  }
  sh::Vec3f* f = vec_field(*ed, field);
  if (!f) {
    refuse(h, "set_vec", field_text(field) + " is not a vector field");
    return;
  }
  *f = v;
}

int32_t svc_spawn(void* ctx) {
  ModHost& h = host_of(ctx);
  if (!h.sv) {
    refuse(h, "spawn", "no match is running");
    return -1;
  }
  return sh::spawn_edict(*h.sv);
}

void svc_remove(void* ctx, int32_t e) {
  ModHost& h = host_of(ctx);
  if (!entity(h, e, "remove")) return;
  if (e <= h.sv->set.maxclients) {
    refuse(h, "remove", "entity " + std::to_string(e) + " is the world or a player");
    return;
  }
  sh::free_edict(*h.sv, e);
}

void svc_set_size(void* ctx, int32_t e, const float mins[3], const float maxs[3]) {
  ModHost& h = host_of(ctx);
  if (!entity(h, e, "set_size")) return;
  sh::set_size(*h.sv, e, {mins[0], mins[1], mins[2]}, {maxs[0], maxs[1], maxs[2]});
}

int32_t svc_function(void* ctx, uint32_t k) {
  ModHost& h = host_of(ctx);
  if (k < 1 || k > kModFuncMax) {
    refuse(h, "function",
           "function " + std::to_string(k) + " is outside 1.." + std::to_string(kModFuncMax));
    return sh::kNoFunc;
  }
  return kModFuncBase + static_cast<sh::Func>(k);
}

void svc_call_function(void* ctx, int32_t fn, int32_t self, int32_t other) {
  ModHost& h = host_of(ctx);
  if (!h.sv) {
    refuse(h, "call_function", "no match is running");
    return;
  }
  if (fn == sh::kNoFunc) return;
  if (fn < 0 || fn >= kModHookBase) {
    refuse(h, "call_function", "function " + std::to_string(fn) + " is not a function number");
    return;
  }
  sh::Server& sv = *h.sv;
  const int keep_self = sv.g.self, keep_other = sv.g.other;
  sv.g.self = self;
  sv.g.other = other;
  sh::call_program(sv, fn);
  sv.g.self = keep_self;
  sv.g.other = keep_other;
}

void svc_sound(void* ctx, int32_t e, int32_t channel, const char* sample, float volume,
               float attenuation) {
  ModHost& h = host_of(ctx);
  if (!entity(h, e, "sound") || !sample) return;
  const std::string* name = nullptr;
  for (const std::string& s : h.sounds)
    if (s == sample) name = &s;
  if (!name) name = &h.sounds.emplace_back(sample);
  sh::start_sound(*h.sv, e, channel, name->c_str(), volume, attenuation);
}

// -- the composite program
// -------------------------------------------------------------------------

template <typename Hook, typename... A>
void run_hook(ModHost& h, Hook hook, A... args) {
  if (!hook) return;
  h.run.hook_calls++;
  hook(state_of(h), &h.host, args...);
}

void mod_hook(ModHost& h, sh::Server& sv, int slot) {
  const se_mod_hooks_v1* k = h.hooks;
  const int32_t self = sv.g.self;
  switch (slot) {
    case kStartFrame:
      if (GE_MOD_HAS(se_mod_hooks_v1, k, start_frame)) run_hook(h, k->start_frame);
      break;
    case kPreThink:
      if (GE_MOD_HAS(se_mod_hooks_v1, k, player_pre_think)) run_hook(h, k->player_pre_think, self);
      break;
    case kPostThink:
      if (GE_MOD_HAS(se_mod_hooks_v1, k, player_post_think))
        run_hook(h, k->player_post_think, self);
      break;
    case kConnect:
      if (GE_MOD_HAS(se_mod_hooks_v1, k, client_connect)) run_hook(h, k->client_connect, self);
      break;
    case kPutInServer:
      if (GE_MOD_HAS(se_mod_hooks_v1, k, put_client_in_server))
        run_hook(h, k->put_client_in_server, self);
      break;
    case kDisconnect:
      if (GE_MOD_HAS(se_mod_hooks_v1, k, client_disconnect))
        run_hook(h, k->client_disconnect, self);
      break;
    default:
      break;
  }
}

bool mod_has_hook(const se_mod_hooks_v1* k, int slot) {
  switch (slot) {
    case kStartFrame:
      return GE_MOD_HAS(se_mod_hooks_v1, k, start_frame) && k->start_frame;
    case kPreThink:
      return GE_MOD_HAS(se_mod_hooks_v1, k, player_pre_think) && k->player_pre_think;
    case kPostThink:
      return GE_MOD_HAS(se_mod_hooks_v1, k, player_post_think) && k->player_post_think;
    case kConnect:
      return GE_MOD_HAS(se_mod_hooks_v1, k, client_connect) && k->client_connect;
    case kPutInServer:
      return GE_MOD_HAS(se_mod_hooks_v1, k, put_client_in_server) && k->put_client_in_server;
    case kDisconnect:
      return GE_MOD_HAS(se_mod_hooks_v1, k, client_disconnect) && k->client_disconnect;
    default:
      return false;
  }
}

void composite_call(void* ctx, sh::Server& sv, sh::Func fn) {
  ModHost& h = host_of(ctx);
  sh::Server* const outer = h.sv;
  h.sv = &sv;
  if (fn >= kModHookBase) {
    const int slot = fn - kModHookBase;
    const sh::Func own = base_hook(h.base, slot);
    if (own != sh::kNoFunc && h.base.call) h.base.call(h.base.ctx, sv, own);
    mod_hook(h, sv, slot);
  } else if (fn >= kModFuncBase) {
    const se_mod_hooks_v1* k = h.hooks;
    if (GE_MOD_HAS(se_mod_hooks_v1, k, call) && k->call)
      run_hook(h, k->call, static_cast<uint32_t>(fn - kModFuncBase), sv.g.self, sv.g.other);
  } else if (h.base.call) {
    h.base.call(h.base.ctx, sv, fn);
  }
  h.sv = outer;
}

struct Fnv {
  uint64_t h = 0xcbf29ce484222325ull;
  void bytes(const void* p, size_t n) {
    const auto* b = static_cast<const uint8_t*>(p);
    for (size_t i = 0; i < n; i++) {
      h ^= b[i];
      h *= 0x100000001b3ull;
    }
  }
  template <typename T>
  void add(const T& v) {
    bytes(&v, sizeof v);
  }
};

}  // namespace

std::vector<std::string> check_hooks(const ge_mod_v1* t) {
  std::vector<std::string> e;
  const auto* k = static_cast<const se_mod_hooks_v1*>(t ? t->hooks : nullptr);
  if (!k) {
    e.push_back(
        "the mod has no hook table; next: point `hooks` at an se_mod_hooks_v1 "
        "(games/shooters_edge/sim/mod_api.hpp)");
    return e;
  }
  if (k->size < offsetof(se_mod_hooks_v1, match_start)) {
    e.push_back("the mod's hook table is " + std::to_string(k->size) +
                " bytes, too short to be one; next: set `size` to sizeof(se_mod_hooks_v1)");
    return e;
  }
  if (k->api != SE_MOD_API)
    e.push_back("the mod's hooks were built against Shooter's Edge mod API " +
                std::to_string(k->api) + ", and this game offers " + std::to_string(SE_MOD_API) +
                "; next: rebuild the mod against this game's mod_api.hpp (ge mod build)");
  return e;
}

std::vector<std::string> create_mod_host(ModHost& h, const ge_mod_v1* table,
                                         const sh::Program& base, uint32_t seed) {
  std::vector<std::string> e = check_hooks(table);
  if (!e.empty()) return e;
  h.table = table;
  h.hooks = static_cast<const se_mod_hooks_v1*>(table->hooks);
  h.base = base;
  h.sv = nullptr;
  h.log.clear();
  h.log_dropped = 0;
  h.sounds.clear();

  se_mod_services_v1& s = h.services;
  s = se_mod_services_v1{};
  s.size = sizeof(se_mod_services_v1);
  s.api = SE_MOD_API;
  s.time = svc_time;
  s.frametime = svc_frametime;
  s.self = svc_self;
  s.other = svc_other;
  s.newmis = svc_newmis;
  s.max_entities = svc_max_entities;
  s.max_players = svc_max_players;
  s.in_use = svc_in_use;
  s.get_float = svc_get_float;
  s.set_float = svc_set_float;
  s.get_int = svc_get_int;
  s.set_int = svc_set_int;
  s.get_vec = svc_get_vec;
  s.set_vec = svc_set_vec;
  s.spawn = svc_spawn;
  s.remove = svc_remove;
  s.set_size = svc_set_size;
  s.function = svc_function;
  s.call_function = svc_call_function;
  s.sound = svc_sound;

  ge::mod::fill_host(h.host);
  h.host.ctx = &h;
  h.host.random_u32 = svc_random_u32;
  h.host.random = svc_random;
  h.host.log_line = svc_log;
  h.host.game = &h.services;

  h.run = ModRun{};
  h.run.state.assign(table->state_bytes, 0);
  h.run.rng = ge::core::make_rng(seed);
  if (table->init) table->init(state_of(h), &h.host);
  return e;
}

sh::Program mod_program(ModHost& h) {
  sh::Program p;
  p.ctx = &h;
  p.call = composite_call;
  const auto hook = [&](int slot) {
    return base_hook(h.base, slot) != sh::kNoFunc || mod_has_hook(h.hooks, slot)
               ? kModHookBase + slot
               : sh::kNoFunc;
  };
  p.start_frame = hook(kStartFrame);
  p.player_pre_think = hook(kPreThink);
  p.player_post_think = hook(kPostThink);
  p.client_connect = hook(kConnect);
  p.put_client_in_server = hook(kPutInServer);
  p.client_disconnect = hook(kDisconnect);
  p.client_kill = hook(kClientKill);
  p.spectator_connect = hook(kSpectatorConnect);
  p.spectator_think = hook(kSpectatorThink);
  p.spectator_disconnect = hook(kSpectatorDisconnect);
  p.set_new_parms = hook(kSetNewParms);
  p.set_change_parms = hook(kSetChangeParms);
  return p;
}

void mod_match_start(ModHost& h, sh::Server& sv) {
  const se_mod_hooks_v1* k = h.hooks;
  if (!GE_MOD_HAS(se_mod_hooks_v1, k, match_start) || !k->match_start) return;
  sh::Server* const outer = h.sv;
  h.sv = &sv;
  run_hook(h, k->match_start);
  h.sv = outer;
}

uint64_t mod_hash(const ModRun& run, uint64_t server_hash) {
  Fnv f;
  f.add(server_hash);
  if (!run.state.empty()) f.bytes(run.state.data(), run.state.size());
  f.add(run.rng.a);
  f.add(run.hook_calls);
  f.add(run.refusals);
  return f.h;
}

}  // namespace shooters_edge::sim
