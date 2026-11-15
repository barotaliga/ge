// The mod host's seam and services (mod_host.hpp states the contract).
//
// ge:summary The mod host's composite program, its checked services and the run's digest
// ge:keywords mod mod-host composite-program services refuse entity-field set_origin spawn
// ge:keywords remove call_function sound intern fnv mod_hash
// ge:skill mod shooter
// ge:test tests/shooters_edge_mod_test.cpp
#include "shooters_edge/sim/mod_host.hpp"

#include <cstddef>
#include <cstring>
#include <iterator>

#include "mod/host.hpp"
#include "mod/loader.hpp"

namespace shooters_edge::sim {

namespace w = ge::mod::wasm;

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

// `self` is the hook's own, as the loop set it before the game's function ran (a bytecode function
// may leave its `self` global elsewhere).
void mod_hook(ModHost& h, int32_t self, int slot) {
  const se_mod_hooks_v1* k = h.hooks;
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
    const int32_t self = sv.g.self;
    if (own != sh::kNoFunc && h.base.call) h.base.call(h.base.ctx, sv, own);
    mod_hook(h, self, slot);
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

se_mod_services_v1 game_services() {
  se_mod_services_v1 s{};
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
  return s;
}

// -- a WebAssembly mod: the services laid out in its memory, its hooks called from native ones
// -------------------------------------------------------------------------

constexpr size_t kWasmHookSlots = 10;  // match_start .. death

// The game's services in the module's memory (wasm32's layout: size, api, a slot each), each
// bound with the host as its context; their address, or 0 with `error`.
uint32_t lay_services(ModHost& h, w::WasmMod& m, const se_mod_services_v1& s, std::string& error) {
  w::Bindings& b = m.bindings;
  void* const ctx = &h;
  const uint32_t words[] = {0,
                            SE_MOD_API,
                            w::bind_native(m.in, b, s.time, ctx),
                            w::bind_native(m.in, b, s.frametime, ctx),
                            w::bind_native(m.in, b, s.self, ctx),
                            w::bind_native(m.in, b, s.other, ctx),
                            w::bind_native(m.in, b, s.newmis, ctx),
                            w::bind_native(m.in, b, s.max_entities, ctx),
                            w::bind_native(m.in, b, s.max_players, ctx),
                            w::bind_native(m.in, b, s.in_use, ctx),
                            w::bind_native(m.in, b, s.get_float, ctx),
                            w::bind_native(m.in, b, s.set_float, ctx),
                            w::bind_native(m.in, b, s.get_int, ctx),
                            w::bind_native(m.in, b, s.set_int, ctx),
                            w::bind_native(m.in, b, s.get_vec, ctx),
                            w::bind_native(m.in, b, s.set_vec, ctx),
                            w::bind_native(m.in, b, s.spawn, ctx),
                            w::bind_native(m.in, b, s.remove, ctx),
                            w::bind_native(m.in, b, s.set_size, ctx),
                            w::bind_native(m.in, b, s.function, ctx),
                            w::bind_native(m.in, b, s.call_function, ctx),
                            w::bind_native(m.in, b, s.sound, ctx)};
  for (size_t i = 2; i < std::size(words); i++) {
    if (words[i] == w::kNullFunc) {
      error =
          "the mod's function table cannot grow to hold the game's services, so the mod "
          "must be linked with a growable table, as the engine's template does";
      return 0;
    }
  }
  const uint32_t bytes = static_cast<uint32_t>(sizeof words);
  const uint32_t at = w::reserve(m, bytes);
  if (at == 0) {
    error =
        "the mod's memory cannot grow to hold the game's services, so the mod must use less memory";
    return 0;
  }
  uint32_t sized[std::size(words)];
  std::memcpy(sized, words, sizeof words);
  sized[0] = bytes;
  (void)w::write_memory(m.in, at, sized, sizeof sized);
  return at;
}

// Before a call into the module: the run's block (the one a snapshot restores) copied into the
// module's memory, so a seek takes the mod back with the match.
void wasm_before(ModHost& h) {
  w::WasmMod& m = *h.wasm;
  if (h.run.state.size() == m.state_bytes && m.state_bytes > 0)
    (void)w::write_memory(m.in, m.state_at, h.run.state.data(), m.state_bytes);
}

// After a call into the module: its block copied into the run (what snapshots and the digest
// read); a trap stops the mod for the match, said once.
void wasm_after(ModHost& h, w::Trap t) {
  w::WasmMod& m = *h.wasm;
  if (t != w::Trap::none) {
    h.wasm_stopped = true;
    refuse(h, "the mod's hooks",
           std::string("its code stopped (") + w::trap_name(t) + ": " + m.in.trap_message +
               "), so it does nothing more this match");
  }
  if (h.run.state.size() == m.state_bytes && m.state_bytes > 0)
    (void)w::read_memory(m.in, m.state_at, h.run.state.data(), m.state_bytes);
}

template <typename R, typename... A>
R wasm_hook(const ge_mod_host_v1* host, uint32_t slot, R otherwise, A... args) {
  ModHost& h = host_of(host->ctx);
  if (h.wasm_stopped) return otherwise;
  R r = otherwise;
  wasm_before(h);
  const w::Trap t = w::call_hook(*h.wasm, slot, &r, args...);
  wasm_after(h, t);
  return t == w::Trap::none ? r : otherwise;
}

template <typename... A>
void wasm_hook_void(const ge_mod_host_v1* host, uint32_t slot, A... args) {
  ModHost& h = host_of(host->ctx);
  if (h.wasm_stopped) return;
  wasm_before(h);
  wasm_after(h, w::call_hook<void>(*h.wasm, slot, nullptr, args...));
}

void wasm_init(void*, const ge_mod_host_v1* host) {
  ModHost& h = host_of(host->ctx);
  if (h.wasm_stopped) return;
  wasm_after(h, w::start_match(*h.wasm));
}
void wasm_match_start(void*, const ge_mod_host_v1* host) { wasm_hook_void(host, 0); }
void wasm_start_frame(void*, const ge_mod_host_v1* host) { wasm_hook_void(host, 1); }
void wasm_pre_think(void*, const ge_mod_host_v1* host, int32_t p) { wasm_hook_void(host, 2, p); }
void wasm_post_think(void*, const ge_mod_host_v1* host, int32_t p) { wasm_hook_void(host, 3, p); }
void wasm_connect(void*, const ge_mod_host_v1* host, int32_t p) { wasm_hook_void(host, 4, p); }
void wasm_put_in_server(void*, const ge_mod_host_v1* host, int32_t p) {
  wasm_hook_void(host, 5, p);
}
void wasm_disconnect(void*, const ge_mod_host_v1* host, int32_t p) { wasm_hook_void(host, 6, p); }
void wasm_call_hook(void*, const ge_mod_host_v1* host, uint32_t k, int32_t self, int32_t other) {
  wasm_hook_void(host, 7, k, self, other);
}
// A stopped mod leaves the damage as the game dealt it.
int32_t wasm_damage(void*, const ge_mod_host_v1* host, int32_t target, int32_t attacker,
                    int32_t points) {
  return wasm_hook<int32_t>(host, 8, points, target, attacker, points);
}
void wasm_death(void*, const ge_mod_host_v1* host, int32_t victim, int32_t killer) {
  wasm_hook_void(host, 9, victim, killer);
}

}  // namespace

std::vector<std::string> check_hooks(const ge_mod_v1* t) {
  std::vector<std::string> e;
  const auto* k = static_cast<const se_mod_hooks_v1*>(t ? t->hooks : nullptr);
  if (!k) {
    e.push_back("the mod has no hook table (it must point hooks at an se_mod_hooks_v1)");
    return e;
  }
  if (k->size < offsetof(se_mod_hooks_v1, match_start)) {
    e.push_back("the mod's hook table is " + std::to_string(k->size) +
                " bytes, too short to be one");
    return e;
  }
  if (k->api != SE_MOD_API)
    e.push_back("the mod's hooks were built against Shooter's Edge mod API " +
                std::to_string(k->api) + ", and this game offers " + std::to_string(SE_MOD_API) +
                ", so the mod must be rebuilt for this version of the game");
  return e;
}

std::vector<std::string> create_mod_host_over(ModHost& h, const ge_mod_v1* table,
                                              const se_mod_services_v1& services, void* rules,
                                              uint32_t seed) {
  std::vector<std::string> e = check_hooks(table);
  if (!e.empty()) return e;
  h.table = table;
  h.hooks = static_cast<const se_mod_hooks_v1*>(table->hooks);
  h.base = sh::Program{};
  h.sv = nullptr;
  h.rules = rules;
  h.log.clear();
  h.log_dropped = 0;
  h.sounds.clear();
  h.services = services;

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

void mod_refuse(ModHost& h, const char* service, const std::string& why) {
  refuse(h, service, why);
}

void* mod_state(ModHost& h) { return state_of(h); }

std::vector<std::string> create_mod_host(ModHost& h, const ge_mod_v1* table,
                                         const sh::Program& base, uint32_t seed) {
  std::vector<std::string> e = create_mod_host_over(h, table, game_services(), nullptr, seed);
  if (e.empty()) h.base = base;
  return e;
}

namespace {

// The module's checks, its services (`services`) and host table laid out, and `wasm_table` filled.
std::vector<std::string> prepare_wasm(ModHost& h, w::WasmMod& m,
                                      const se_mod_services_v1& services) {
  if (m.game != SE_MOD_GAME)
    return {"the mod is for the game '" + m.game +
            "', not " SE_MOD_GAME ", so it cannot run in this game"};
  if (m.game_api != SE_MOD_API)
    return {"the mod was built against Shooter's Edge mod API " + std::to_string(m.game_api) +
            ", and this game offers " + std::to_string(SE_MOD_API) +
            ", so the mod must be rebuilt for this version of the game"};
  if (m.host_at != 0)
    return {"the WebAssembly mod is in a match already, so it must be loaded again for another"};
  h.wasm = &m;
  h.wasm_stopped = false;
  std::string error;
  const uint32_t svc = lay_services(h, m, services, error);
  if (svc == 0) return {error};
  ge_mod_host_v1 native{};
  ge::mod::fill_host(native);
  native.ctx = &h;
  native.random_u32 = svc_random_u32;
  native.random = svc_random;
  native.log_line = svc_log;
  if (!w::lay_host(m, native, svc, error)) return {error};

  se_mod_hooks_v1& k = h.wasm_hooks;
  k = se_mod_hooks_v1{};
  k.api = m.hooks_api;
  const size_t slots = m.hooks.size() < kWasmHookSlots ? m.hooks.size() : kWasmHookSlots;
  k.size = static_cast<uint32_t>(offsetof(se_mod_hooks_v1, match_start) + slots * sizeof(void*));
  const auto on = [&](uint32_t slot) { return slot < slots && m.hooks[slot] != 0; };
  if (on(0)) k.match_start = wasm_match_start;
  if (on(1)) k.start_frame = wasm_start_frame;
  if (on(2)) k.player_pre_think = wasm_pre_think;
  if (on(3)) k.player_post_think = wasm_post_think;
  if (on(4)) k.client_connect = wasm_connect;
  if (on(5)) k.put_client_in_server = wasm_put_in_server;
  if (on(6)) k.client_disconnect = wasm_disconnect;
  if (on(7)) k.call = wasm_call_hook;
  if (on(8)) k.damage = wasm_damage;
  if (on(9)) k.death = wasm_death;
  if (m.hooks_size == 0) k.size = 0;  // no hook table: check_hooks names it
  h.wasm_table =
      ge_mod_v1{sizeof(ge_mod_v1), m.abi,          m.name.c_str(),
                m.version.c_str(), m.game.c_str(), m.game_api,
                m.state_bytes,     wasm_init,      m.hooks_size == 0 ? nullptr : &h.wasm_hooks};
  return {};
}

}  // namespace

std::vector<std::string> create_wasm_mod_host(ModHost& h, w::WasmMod& m, const sh::Program& base,
                                              uint32_t seed) {
  std::vector<std::string> e = prepare_wasm(h, m, game_services());
  if (!e.empty()) return e;
  return create_mod_host(h, &h.wasm_table, base, seed);
}

std::vector<std::string> create_wasm_mod_host_over(ModHost& h, w::WasmMod& m,
                                                   const se_mod_services_v1& services, void* rules,
                                                   uint32_t seed) {
  std::vector<std::string> e = prepare_wasm(h, m, services);
  if (!e.empty()) return e;
  return create_mod_host_over(h, &h.wasm_table, services, rules, seed);
}

std::vector<std::string> host_package(ModHost& h, const ge::mod::LoadedMod& package,
                                      std::unique_ptr<w::WasmMod>& instance,
                                      const se_mod_services_v1* services, void* rules,
                                      const sh::Program& base, uint32_t seed) {
  if (!package.is_wasm()) {
    if (services) return create_mod_host_over(h, package.table, *services, rules, seed);
    return create_mod_host(h, package.table, base, seed);
  }
  instance = std::make_unique<w::WasmMod>();
  std::string error;
  if (!w::load_wasm_mod(package.wasm.data(), package.wasm.size(), GE_MOD_ABI, *instance, error))
    return {error};
  if (services) return create_wasm_mod_host_over(h, *instance, *services, rules, seed);
  return create_wasm_mod_host(h, *instance, base, seed);
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
