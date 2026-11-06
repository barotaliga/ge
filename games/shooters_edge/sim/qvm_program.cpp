// The compiled-module adapter (qvm_program.hpp).
//
// ge:summary The compiled-module adapter: the game data block, the mirror, tags, exports, entities
// ge:keywords qvm adapter mirror sync tags exports spawn free clear-edict reference-mode
// ge:skill progs shooter
// ge:test tests/shooters_edge_qvm_server_test.cpp
#include "shooters_edge/sim/qvm_program.hpp"

#include <cstring>

#include "progs/layout.hpp"

namespace shooters_edge::sim {

namespace pg = ge::progs;

namespace {

const pg::LayoutFields& F = pg::kMultiplayerLayout.f;
const pg::LayoutGlobals& G = pg::kMultiplayerLayout.g;

constexpr int32_t kEngineBytes = pg::kEngineFields * 4;

// The hooks' tags (after the function tags).
constexpr sh::Func kHookStartFrame = 10, kHookPreThink = 11, kHookPostThink = 12;
constexpr sh::Func kHookSpectatorThink = 13, kHookConnect = 14, kHookPut = 15;
constexpr sh::Func kHookSpectatorConnect = 16, kHookDisconnect = 17;
constexpr sh::Func kHookSpectatorDisconnect = 18, kHookNewParms = 19, kHookChangeParms = 20;

int32_t word_at(const QvmProgram& q, int32_t addr) { return pg::qvm_word(q.vm, addr); }
void put_word(QvmProgram& q, int32_t addr, int32_t v) { pg::qvm_set_word(q.vm, addr, v); }

int32_t faddr(const QvmProgram& q, int e, int word) { return record_of(q, e) + word * 4; }

void put_e(QvmProgram& q, int e, int word, int n) { set_field_i(q, e, word, entity_value(q, n)); }
int get_e(const QvmProgram& q, int e, int word) {
  const int n = entity_from_value(q, field_i(q, e, word));
  return n < 0 ? 0 : n;
}
int float_int(float v) { return v != v ? 0 : static_cast<int>(v); }
float int_float(int v) { return static_cast<float>(v); }

void mirror_in(QvmProgram& q, int e) {
  const sh::Edict& d = q.sv->edicts[static_cast<size_t>(e)];
  set_field_f(q, e, F.modelindex, int_float(d.modelindex));
  set_field_v(q, e, F.absmin, d.absmin);
  set_field_v(q, e, F.absmax, d.absmax);
  set_field_f(q, e, F.ltime, d.ltime);
  set_field_f(q, e, F.movetype, int_float(d.movetype));
  set_field_f(q, e, F.solid, int_float(d.solid));
  set_field_v(q, e, F.origin, d.origin);
  set_field_v(q, e, F.velocity, d.velocity);
  set_field_v(q, e, F.angles, d.angles);
  set_field_v(q, e, F.avelocity, d.avelocity);
  set_field_v(q, e, F.mins, d.mins);
  set_field_v(q, e, F.maxs, d.maxs);
  set_field_v(q, e, F.size, d.size);
  set_field_f(q, e, F.nextthink, d.nextthink);
  put_e(q, e, F.groundentity, d.groundentity);
  set_field_f(q, e, F.health, d.health);
  set_field_f(q, e, F.button0, d.button0);
  set_field_f(q, e, F.button1, d.button1);
  set_field_f(q, e, F.button2, d.button2);
  set_field_f(q, e, F.impulse, d.impulse);
  set_field_f(q, e, F.fixangle, d.fixangle ? 1.0f : 0.0f);
  set_field_v(q, e, F.v_angle, d.v_angle);
  set_field_f(q, e, F.flags, int_float(d.flags));
  set_field_f(q, e, F.teleport_time, d.teleport_time);
  set_field_f(q, e, F.waterlevel, int_float(d.waterlevel));
  set_field_f(q, e, F.watertype, int_float(d.watertype));
  put_e(q, e, F.owner, d.owner);
  set_ext_f(q, e, q.fields.gravity, d.gravity);
  set_ext_f(q, e, q.fields.maxspeed, d.maxspeed);
  if (q.fields.movement != 0) {
    set_ext_f(q, e, q.fields.movement, d.movement.x);
    set_ext_f(q, e, q.fields.movement + 4, d.movement.y);
    set_ext_f(q, e, q.fields.movement + 8, d.movement.z);
  }
}

// The loop's own (absmin, absmax: link_edict's) are not read back; the function fields become
// tags.
void mirror_out(QvmProgram& q, int e) {
  sh::Edict& d = q.sv->edicts[static_cast<size_t>(e)];
  d.modelindex = float_int(field_f(q, e, F.modelindex));
  d.ltime = field_f(q, e, F.ltime);
  d.movetype = float_int(field_f(q, e, F.movetype));
  d.solid = float_int(field_f(q, e, F.solid));
  d.origin = field_v(q, e, F.origin);
  d.velocity = field_v(q, e, F.velocity);
  d.angles = field_v(q, e, F.angles);
  d.avelocity = field_v(q, e, F.avelocity);
  d.mins = field_v(q, e, F.mins);
  d.maxs = field_v(q, e, F.maxs);
  d.size = field_v(q, e, F.size);
  d.touch = field_i(q, e, F.touch) != 0 ? kTagTouch : sh::kNoFunc;
  d.think = kTagThink;
  d.blocked = field_i(q, e, F.blocked) != 0 ? kTagBlocked : sh::kNoFunc;
  d.nextthink = field_f(q, e, F.nextthink);
  d.groundentity = get_e(q, e, F.groundentity);
  d.health = field_f(q, e, F.health);
  d.button0 = field_f(q, e, F.button0);
  d.button1 = field_f(q, e, F.button1);
  d.button2 = field_f(q, e, F.button2);
  d.impulse = field_f(q, e, F.impulse);
  d.fixangle = field_f(q, e, F.fixangle) != 0;
  d.v_angle = field_v(q, e, F.v_angle);
  d.view_ofs = field_v(q, e, F.view_ofs);  // the module's: read, never written
  d.flags = float_int(field_f(q, e, F.flags));
  d.teleport_time = field_f(q, e, F.teleport_time);
  d.waterlevel = float_int(field_f(q, e, F.waterlevel));
  d.watertype = float_int(field_f(q, e, F.watertype));
  d.owner = get_e(q, e, F.owner);
  if (q.fields.gravity != 0) d.gravity = ext_f(q, e, q.fields.gravity);
  if (q.fields.maxspeed != 0) d.maxspeed = ext_f(q, e, q.fields.maxspeed);
}

void zero_record(QvmProgram& q, int e) {
  std::memset(q.vm.mem.data() + record_of(q, e), 0, static_cast<size_t>(q.data.entity_size));
}

// The free rule on the module's record: these cleared, nextthink -1, the rest kept.
void free_rule(QvmProgram& q, int e) {
  for (int f : {F.model, F.takedamage, F.modelindex, F.colormap, F.skin, F.frame, F.health,
                F.classname, F.solid})
    set_field_i(q, e, f, 0);
  set_field_v(q, e, F.origin, {});
  set_field_v(q, e, F.angles, {});
  set_field_f(q, e, F.nextthink, -1);
}

void sync_entities_in(QvmProgram& q) {
  sh::Server& sv = *q.sv;
  const int n = sv.num_edicts < q.max_edicts ? sv.num_edicts : q.max_edicts;
  for (int e = 0; e < n; e++) {
    const bool used = !sv.edicts[static_cast<size_t>(e)].free || e == 0;
    uint8_t& was = q.live[static_cast<size_t>(e)];
    if (used && was == 0) qvm_clear_entity(q, e);  // one the loop took: the module's clear
    if (!used && was != 0) free_rule(q, e);
    was = used ? 1 : 0;
    if (used) mirror_in(q, e);
  }
}

void sync_in(QvmProgram& q) {
  const sh::Server& sv = *q.sv;
  sync_entities_in(q);
  set_global_i(q, G.self, entity_value(q, sv.g.self));
  set_global_i(q, G.other, entity_value(q, sv.g.other));
  set_global_i(q, G.world, 0);
  set_global_f(q, G.time, sv.g.time);
  set_global_f(q, G.frametime, sv.g.frametime);
  set_global_i(q, G.newmis, entity_value(q, sv.g.newmis));
  set_global_f(q, G.force_retouch, sv.g.force_retouch);
}

void sync_out(QvmProgram& q) {
  sh::Server& sv = *q.sv;
  const int n = sv.num_edicts < q.max_edicts ? sv.num_edicts : q.max_edicts;
  for (int e = 1; e < n; e++)
    if (q.live[static_cast<size_t>(e)] != 0 && !sv.edicts[static_cast<size_t>(e)].free)
      mirror_out(q, e);
  sv.g.self = global_entity(q, G.self);
  sv.g.other = global_entity(q, G.other);
  sv.g.newmis = global_entity(q, G.newmis);
  sv.g.force_retouch = global_f(q, G.force_retouch);
}

void enter(QvmProgram& q, sh::Server& sv) {
  if (q.nesting < kMaxQvmNesting) q.outer[q.nesting] = q.sv;
  q.nesting++;
  q.sv = &sv;
  q.calls++;
  sync_in(q);
}

void leave(QvmProgram& q) {
  sync_out(q);
  q.nesting--;
  q.sv = q.nesting < kMaxQvmNesting ? q.outer[q.nesting] : nullptr;
}

int32_t run(QvmProgram& q, int32_t command, int32_t a0, int32_t a1, int32_t a2) {
  if (command >= 0 && command < kQvmExports) q.export_counts[command]++;
  const int32_t r = pg::qvm_call(q.vm, command, a0, a1, a2);
  // Only the outermost call clears a fault: a nested one unwinds every call above it first.
  if (!q.vm.fault.empty() && q.vm.depth == 0) {
    q.faults++;
    q.last_fault = q.vm.fault;
    pg::qvm_clear_fault(q.vm);
  }
  return r;
}

// Gives a client being set up its gravity and maximum speed before its first hook.
void set_up(QvmProgram& q, sh::Server& sv) {
  const int e = q.setting_up;
  if (e <= 0 || sv.g.self != e) return;
  sh::Edict& d = sv.edicts[static_cast<size_t>(e)];
  d.gravity = 1;
  d.maxspeed = q.setup_maxspeed;
}

void call(void* ctx, sh::Server& sv, sh::Func fn) {
  auto& q = *static_cast<QvmProgram*>(ctx);
  int32_t cmd = -1, a0 = 0;
  switch (fn) {
    case kTagTouch:
      cmd = kQvmTouch;
      break;
    case kTagThink:
      cmd = kQvmThink;
      break;
    case kTagBlocked:
      cmd = kQvmBlocked;
      break;
    case kHookStartFrame:
      cmd = kQvmStartFrame;
      a0 = static_cast<int32_t>(sv.time * 1000);
      break;
    case kHookPreThink:
      cmd = kQvmPreThink;
      break;
    case kHookPostThink:
      cmd = kQvmPostThink;
      break;
    case kHookSpectatorThink:
      cmd = kQvmPostThink;
      a0 = 1;
      break;
    case kHookConnect:
      cmd = kQvmClientConnect;
      break;
    case kHookPut:
      cmd = kQvmPutClientInServer;
      break;
    case kHookSpectatorConnect:
      set_up(q, sv);
      qvm_export(q, sv, kQvmClientConnect, 1);
      cmd = kQvmPutClientInServer;
      a0 = 1;
      break;
    case kHookDisconnect:
      cmd = kQvmClientDisconnect;
      break;
    case kHookSpectatorDisconnect:
      cmd = kQvmClientDisconnect;
      a0 = 1;
      break;
    case kHookNewParms:
      cmd = kQvmSetNewParms;
      break;
    case kHookChangeParms:
      cmd = kQvmSetChangeParms;
      break;
    default:
      return;
  }
  set_up(q, sv);
  const int self = sv.g.self;
  qvm_export(q, sv, cmd, a0);
  if ((cmd == kQvmSetNewParms || cmd == kQvmSetChangeParms) && q.after_parms != nullptr)
    q.after_parms(q, self);
}

// The extension field `name`'s byte offset in the module's table (case-insensitive, the first
// match); 0 when absent or when the offset leaves the record.
int32_t find_field(QvmProgram& q, const char* name, int32_t width) {
  if (q.data.fields <= 0) return 0;
  for (int32_t at = q.data.fields;; at += 12) {
    if (pg::qvm_ptr(q.vm, at, 12, "the extension field table") == nullptr) {
      pg::qvm_clear_fault(q.vm);
      return 0;
    }
    const int32_t name_at = word_at(q, at);
    if (name_at == 0) return 0;
    const std::string_view n = pg::qvm_string(q.vm, name_at, "an extension field's name");
    if (!q.vm.fault.empty()) {
      pg::qvm_clear_fault(q.vm);
      return 0;
    }
    if (n.size() != std::strlen(name)) continue;
    bool same = true;
    for (size_t i = 0; i < n.size() && same; i++) {
      const auto a = static_cast<unsigned char>(n[i]), b = static_cast<unsigned char>(name[i]);
      same = (a | 0x20) == (b | 0x20);
    }
    if (!same) continue;
    const int32_t ofs = word_at(q, at + 4);
    return ofs >= kEngineBytes && ofs + width <= q.data.entity_size ? ofs : 0;
  }
}

}  // namespace

bool bind_game_data(QvmProgram& q, int32_t addr, int max_edicts, std::string& error) {
  if (addr == 0 || pg::qvm_ptr(q.vm, addr, 24, "the game data block") == nullptr) {
    pg::qvm_clear_fault(q.vm);
    error = "the module's init returned no game data block (address " + std::to_string(addr) +
            "); next: the module refused to start (its console lines say why)";
    return false;
  }
  QvmGameData& d = q.data;
  d.ents = word_at(q, addr);
  d.entity_size = word_at(q, addr + 4);
  d.globals = word_at(q, addr + 8);
  d.fields = word_at(q, addr + 12);
  d.api = word_at(q, addr + 16);
  d.max_entities = word_at(q, addr + 20);
  if (d.api < 16 || d.api > 17) {
    error = "the module's interface version " + std::to_string(d.api) +
            " is not 16 or 17; next: run a module built for this server's interface";
    return false;
  }
  if (d.entity_size < kEngineBytes || d.max_entities < 1) {
    error = "the module's entity records (" + std::to_string(d.entity_size) + " bytes, " +
            std::to_string(d.max_entities) + " of them) cannot hold the engine fields (" +
            std::to_string(kEngineBytes) + " bytes); next: check the module's game data block";
    return false;
  }
  q.max_edicts = max_edicts < d.max_entities ? max_edicts : d.max_entities;
  const auto mem = static_cast<int64_t>(q.vm.mem.size());
  const int64_t ents_end = int64_t{d.ents} + int64_t{d.entity_size} * q.max_edicts;
  if (d.ents <= 0 || ents_end > mem || d.globals <= 0 ||
      int64_t{d.globals} + kQvmGlobalBytes > mem) {
    error =
        "the module's entity records or globals lie outside its memory; next: check the "
        "module's game data block";
    return false;
  }
  q.live.assign(static_cast<size_t>(q.max_edicts), 0);
  QvmFields& f = q.fields;
  f = QvmFields{};
  f.vw_index = find_field(q, "vw_index", 4);
  f.movement = find_field(q, "movement", 12);
  f.maxspeed = find_field(q, "maxspeed", 4);
  f.gravity = find_field(q, "gravity", 4);
  f.isbot = find_field(q, "isBot", 4);
  f.items2 = find_field(q, "items2", 4);
  f.hideentity = find_field(q, "hideentity", 4);
  f.trackent = find_field(q, "trackent", 4);
  f.hideplayers = find_field(q, "hideplayers", 4);
  f.teleported = find_field(q, "teleported", 4);
  f.visclients = find_field(q, "visclients", 4);
  f.mod_admin = find_field(q, "mod_admin", 4);
  return true;
}

sh::Program qvm_program(QvmProgram& q) {
  sh::Program p;
  p.ctx = &q;
  p.call = &call;
  p.start_frame = kHookStartFrame;
  p.player_pre_think = kHookPreThink;
  p.player_post_think = kHookPostThink;
  p.spectator_think = kHookSpectatorThink;
  p.client_connect = kHookConnect;
  p.put_client_in_server = kHookPut;
  p.spectator_connect = kHookSpectatorConnect;
  p.client_disconnect = kHookDisconnect;
  p.spectator_disconnect = kHookSpectatorDisconnect;
  p.set_new_parms = kHookNewParms;
  p.set_change_parms = kHookChangeParms;
  return p;
}

int32_t qvm_export(QvmProgram& q, sh::Server& sv, int32_t command, int32_t a0, int32_t a1,
                   int32_t a2) {
  enter(q, sv);
  const int32_t r = run(q, command, a0, a1, a2);
  leave(q);
  return r;
}

int32_t qvm_reenter(QvmProgram& q, int32_t command, int32_t a0, int32_t a1, int32_t a2) {
  return run(q, command, a0, a1, a2);
}

void qvm_flush(QvmProgram& q) {
  if (q.sv != nullptr) sync_out(q);
}

void qvm_refresh(QvmProgram& q) {
  if (q.sv != nullptr) sync_entities_in(q);
}

void qvm_flush_entity(QvmProgram& q, int e) {
  if (q.sv != nullptr && e > 0 && q.live[static_cast<size_t>(e)] != 0 &&
      !q.sv->edicts[static_cast<size_t>(e)].free)
    mirror_out(q, e);
}

void qvm_refresh_entity(QvmProgram& q, int e) {
  if (q.sv != nullptr && q.live[static_cast<size_t>(e)] != 0) mirror_in(q, e);
}

int entity_from_value(const QvmProgram& q, int32_t value) {
  if (value < 0 || q.data.entity_size <= 0 || value % q.data.entity_size != 0) return -1;
  const int n = value / q.data.entity_size;
  if (n >= q.max_edicts) return -1;
  const sh::Server* sv = q.sv != nullptr ? q.sv : q.home;
  if (n != 0 && (sv == nullptr || n >= sv->num_edicts)) return -1;
  return n;
}

int entity_from_address(const QvmProgram& q, int32_t addr) {
  return entity_from_value(q, addr - q.data.ents);
}

float field_f(const QvmProgram& q, int e, int word) {
  return pg::qvm_float(word_at(q, faddr(q, e, word)));
}
int32_t field_i(const QvmProgram& q, int e, int word) { return word_at(q, faddr(q, e, word)); }
sh::Vec3f field_v(const QvmProgram& q, int e, int word) {
  const int32_t a = faddr(q, e, word);
  return {pg::qvm_float(word_at(q, a)), pg::qvm_float(word_at(q, a + 4)),
          pg::qvm_float(word_at(q, a + 8))};
}
void set_field_f(QvmProgram& q, int e, int word, float v) {
  put_word(q, faddr(q, e, word), pg::qvm_bits(v));
}
void set_field_i(QvmProgram& q, int e, int word, int32_t v) { put_word(q, faddr(q, e, word), v); }
void set_field_v(QvmProgram& q, int e, int word, sh::Vec3f v) {
  const int32_t a = faddr(q, e, word);
  put_word(q, a, pg::qvm_bits(v.x));
  put_word(q, a + 4, pg::qvm_bits(v.y));
  put_word(q, a + 8, pg::qvm_bits(v.z));
}

float ext_f(const QvmProgram& q, int e, int32_t ofs) {
  return ofs == 0 ? 0.0f : pg::qvm_float(word_at(q, record_of(q, e) + ofs));
}
void set_ext_f(QvmProgram& q, int e, int32_t ofs, float v) {
  if (ofs != 0) put_word(q, record_of(q, e) + ofs, pg::qvm_bits(v));
}
int32_t ext_i(const QvmProgram& q, int e, int32_t ofs) {
  return ofs == 0 ? 0 : word_at(q, record_of(q, e) + ofs);
}
void set_ext_i(QvmProgram& q, int e, int32_t ofs, int32_t v) {
  if (ofs != 0) put_word(q, record_of(q, e) + ofs, v);
}

float global_f(const QvmProgram& q, int word) {
  return pg::qvm_float(word_at(q, q.data.globals + word * 4));
}
int32_t global_i(const QvmProgram& q, int word) { return word_at(q, q.data.globals + word * 4); }
void set_global_f(QvmProgram& q, int word, float v) {
  put_word(q, q.data.globals + word * 4, pg::qvm_bits(v));
}
void set_global_i(QvmProgram& q, int word, int32_t v) { put_word(q, q.data.globals + word * 4, v); }
void set_global_v(QvmProgram& q, int word, sh::Vec3f v) {
  set_global_f(q, word, v.x);
  set_global_f(q, word + 1, v.y);
  set_global_f(q, word + 2, v.z);
}
int global_entity(const QvmProgram& q, int word) {
  const int n = entity_from_value(q, global_i(q, word));
  return n < 0 ? 0 : n;
}

int32_t text_slot(const QvmProgram& q, int e, int word) {
  if (!q.references) return faddr(q, e, word);
  const int32_t w = field_i(q, e, word);
  if (w <= 0 || int64_t{w} >= int64_t{q.data.entity_size} * q.max_edicts - 4) return -1;
  return q.data.ents + w;
}

std::string_view entity_text(const QvmProgram& q, int e, int word) {
  const int32_t slot = text_slot(q, e, word);
  if (slot < 0) return {};
  const int32_t addr = word_at(q, slot);
  if (addr <= 0) return {};
  // Read without faulting the running call: a bad address reads as no text.
  const auto a = static_cast<uint32_t>(addr);
  if (a >= q.vm.mem.size()) return {};
  const void* end = std::memchr(q.vm.mem.data() + a, 0, q.vm.mem.size() - a);
  if (end == nullptr) return {};
  return {reinterpret_cast<const char*>(q.vm.mem.data() + a),
          static_cast<size_t>(static_cast<const uint8_t*>(end) - (q.vm.mem.data() + a))};
}

void qvm_clear_entity(QvmProgram& q, int e) {
  zero_record(q, e);
  q.live[static_cast<size_t>(e)] = 1;
  if (!q.references) return;
  const int32_t was = global_i(q, G.self);
  set_global_i(q, G.self, entity_value(q, e));
  if (q.vm.depth > 0)
    qvm_reenter(q, kQvmClearEdict);
  else
    run(q, kQvmClearEdict, 0, 0, 0);
  set_global_i(q, G.self, was);
}

int qvm_spawn_entity(QvmProgram& q) {
  if (q.sv == nullptr) return -1;
  const int e = sh::spawn_edict(*q.sv);
  if (e < 0 || e >= q.max_edicts) return -1;
  qvm_clear_entity(q, e);
  mirror_in(q, e);
  return e;
}

void qvm_free_entity(QvmProgram& q, int e) {
  sh::free_edict(*q.sv, e);
  free_rule(q, e);
  q.live[static_cast<size_t>(e)] = 0;
}

}  // namespace shooters_edge::sim
