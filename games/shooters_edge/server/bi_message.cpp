// The builtins whose effect is network output (builtins.hpp; catalogue.hpp states each rule): the
// Write family into the sink's buffers, multicast, sounds, prints, stuffed commands, light styles,
// particles and ambient sounds as the sink's events (messages.hpp), with their recipients.
//
// SPECTATOR COPIES: a spectator tracking a client (its `trackent`) gets a copy of that client's
// sprint when the server's `sv_specprint` has bit 2 and the spectator's own user info `sp` does
// too, and of its centerprint under bit 1 of both.
//
// ge:summary The network builtins: Write*, multicast, sound, prints, stuffcmd, lightstyle, particle
// ge:keywords sound stuffcmd bprint sprint dprint centerprint lightstyle particle ambientsound
// ge:keywords writebyte writechar writeshort writelong writecoord writeangle writestring
// ge:keywords writeentity multicast msg_entity recipients phs pvs spectator-copies sv_specprint
// ge:skill progs shooter
// ge:test tests/shooters_edge_qc_messages_test.cpp
#include <cstring>

#include "progs/stdlib.hpp"
#include "shooters_edge/server/builtins.hpp"

namespace shooters_edge::server {

namespace {

constexpr float kHearing = 1024;  // units: anyone this near hears whatever the sets say

float vm_f(const pg::Vm& vm, int e, int f) { return pg::as_float(*pg::entity_field(vm, e, f)); }
sh::Vec3f vm_v(const pg::Vm& vm, int e, int f) {
  const uint32_t* w = pg::entity_field(vm, e, f);
  return {pg::as_float(w[0]), pg::as_float(w[1]), pg::as_float(w[2])};
}

int32_t trunc_i32(double d) {
  if (!(d > -2147483649.0 && d < 2147483648.0)) return INT32_MIN;
  return static_cast<int32_t>(d);
}

void push(QcServer& s, NetEvent ev) {
  ev.time = s.sv.time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

// The client a spectator in `slot` tracks (its trackent, an entity 1..32), or -1.
int tracked_by(const QcServer& s, int slot) {
  const int f = s.q.fields.trackent;
  if (f < 0) return -1;
  const int n =
      pg::entity_number(s.q.vm, static_cast<int32_t>(*pg::entity_field(s.q.vm, slot + 1, f)));
  return n >= 1 && n <= sh::kMaxClients ? n : -1;
}

// Spectators tracking client entity `e` whose copy `bit` is on, here and in their own `sp`.
uint32_t spectator_copies(QcServer& s, int e, int bit) {
  const ge::console::Cvar* c = s.cvars.find("sv_specprint");
  if (c == nullptr || (c->integer() & bit) == 0) return 0;
  uint32_t to = 0;
  for (int i = 0; i < sh::kMaxClients; i++) {
    const ClientInfo& ci = s.clients[i];
    if (!ci.active || !ci.spectator || tracked_by(s, i) != e) continue;
    if ((static_cast<int>(pg::parse_stof(info_value(ci.userinfo, "sp").c_str())) & bit) == 0)
      continue;
    to |= 1u << i;
  }
  return to;
}

int sound_index(const QcServer& s, const char* sample) {
  for (size_t i = 1; i < s.sounds.size(); i++)
    if (s.sounds[i] == sample) return static_cast<int>(i);
  return -1;
}

// A client entity argument (1..32 in use); -1 after a fault with `what`.
int client_arg(pg::Vm& vm, int i, const char* what) {
  const int e = pg::entity_number(vm, pg::arg_int(vm, i));
  if (e < 1 || e > sh::kMaxClients) {
    pg::vm_error(vm, what);
    return -1;
  }
  return e;
}

void write(pg::Vm& vm, WriteKind kind) {
  QcServer& s = server_of(vm);
  const int dest = trunc_i32(pg::arg_float(vm, 0));
  float number = pg::arg_float(vm, 1);
  const char* text = "";
  if (kind == WriteKind::string) {
    text = pg::arg_string(vm, 1);
    if (vm.fault.set) return;
  } else if (kind == WriteKind::entity) {
    const int n = pg::entity_number(vm, pg::arg_int(vm, 1));
    number = static_cast<float>(n < 0 ? 0 : n);
  }
  std::vector<uint8_t>* buf = nullptr;
  switch (dest) {
    case 0:
      buf = &s.out.unreliable;
      break;
    case 1: {
      const int e = pg::entity_number(vm, pg::global_int(vm, pg::vm_layout(vm).g.msg_entity));
      if (e < 1 || e > sh::kMaxClients) {
        pg::vm_error(vm, "write to one client: msg_entity is not a client (1..32)");
        return;
      }
      buf = &s.out.one[e - 1];
      break;
    }
    case 2:
      buf = &s.out.all;
      break;
    case 3:
      if (!s.loading) {
        pg::vm_error(vm, "write to the signon (destination 3) only while the map loads");
        return;
      }
      buf = &s.out.init;
      break;
    case 4:
      if (pg::vm_layout(vm).single_player) break;  // no multicast in the single-player form
      buf = &s.out.multicast;
      break;
    default:
      break;
  }
  if (buf == nullptr) {
    const std::string m = "write: bad destination " + std::to_string(dest) +
                          (pg::vm_layout(vm).single_player ? " (0..3)" : " (0..4)");
    pg::vm_error(vm, m.c_str());
    return;
  }
  encode(*buf, kind, number, text);
}

}  // namespace

uint32_t all_clients(const QcServer& s) {
  uint32_t to = 0;
  for (int i = 0; i < sh::kMaxClients; i++)
    if (s.clients[i].active) to |= 1u << i;
  return to;
}

bool sees(const QcServer& s, sh::Vec3f from, sh::Vec3f to) {
  return s.vis == nullptr || s.vis(s.vis_ctx, from, to, true);
}

bool hears(const QcServer& s, sh::Vec3f from, sh::Vec3f to) {
  return s.vis == nullptr || s.vis(s.vis_ctx, from, to, false);
}

sh::Vec3f eye_of(const QcServer& s, int e) {
  return vm_v(s.q.vm, e, s.q.layout->f.origin) + vm_v(s.q.vm, e, s.q.layout->f.view_ofs);
}

uint32_t clients_reaching(const QcServer& s, sh::Vec3f where, bool see) {
  uint32_t to = 0;
  for (int i = 0; i < sh::kMaxClients; i++) {
    const ClientInfo& ci = s.clients[i];
    if (!ci.active) continue;
    int viewer = i + 1;
    if (ci.spectator) {
      const int t = tracked_by(s, i);
      if (t > 0) viewer = t;
    }
    const sh::Vec3f eye = eye_of(s, viewer);
    bool reach = false;
    if (see) {
      reach = sees(s, eye, where);
    } else {
      const sh::Vec3f d = eye - where;
      reach = hears(s, eye, where) || (d.x * d.x + d.y * d.y) + d.z * d.z <= kHearing * kHearing;
    }
    if (reach) to |= 1u << i;
  }
  return to;
}

SavedGlobals save_globals(const pg::Vm& vm) {
  return {vm.globals[pg::vm_layout(vm).g.self], vm.globals[pg::vm_layout(vm).g.other],
          vm.globals[pg::vm_layout(vm).g.time]};
}

void restore_globals(pg::Vm& vm, const SavedGlobals& g) {
  vm.globals[pg::vm_layout(vm).g.self] = g.self;
  vm.globals[pg::vm_layout(vm).g.other] = g.other;
  vm.globals[pg::vm_layout(vm).g.time] = g.time;
}

void bi_sound(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const int e = sim::entity_parm(vm, 0, "sound");
  if (e < 0) return;
  const int channel = trunc_i32(pg::arg_float(vm, 1));
  const char* sample = pg::arg_string(vm, 2);
  if (vm.fault.set) return;
  const float volume = pg::arg_float(vm, 3), attenuation = pg::arg_float(vm, 4);
  const int vol255 = trunc_i32(static_cast<double>(volume) * 255);
  if (vol255 < 0 || vol255 > 255) {
    pg::vm_error(vm, "sound: volume out of 0..1");
    return;
  }
  if (!(attenuation >= 0 && attenuation <= 4)) {
    pg::vm_error(vm, "sound: attenuation out of 0..4");
    return;
  }
  if (channel < 0 || channel > 15) {
    pg::vm_error(vm, "sound: channel out of 0..15");
    return;
  }
  const int index = sound_index(s, sample);
  if (index < 0) {
    console_print(s, std::string("no precache: ") + sample);
    return;
  }
  sh::Vec3f at = vm_v(vm, e, pg::vm_layout(vm).f.origin);
  const int solid = static_cast<int>(vm_f(vm, e, pg::vm_layout(vm).f.solid));
  const int modelindex = static_cast<int>(vm_f(vm, e, pg::vm_layout(vm).f.modelindex));
  if (solid == sh::kSolidBsp || (solid == sh::kSolidTrigger && modelindex == 0))
    at =
        at + (vm_v(vm, e, pg::vm_layout(vm).f.mins) + vm_v(vm, e, pg::vm_layout(vm).f.maxs)) * 0.5f;
  const bool everyone = (channel & 8) != 0 || !s.cvars.find("sv_phs")->on();
  NetEvent ev;
  ev.kind = EventKind::sound;
  ev.reliable = (channel & 8) != 0;
  ev.to = everyone ? all_clients(s) : clients_reaching(s, at, false);
  ev.ent = e;
  ev.level = channel & 7;
  ev.origin = at;
  ev.values[0] = static_cast<float>(vol255);
  ev.values[1] = attenuation;
  ev.text = sample;
  push(s, std::move(ev));
  sh::start_sound(s.sv, e, channel & 7, s.sounds[static_cast<size_t>(index)].c_str(), volume,
                  attenuation);
}

void bi_stuffcmd(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const int e = client_arg(vm, 0, "Parm 0 not a client");
  if (e < 0) return;
  const char* text = pg::arg_string(vm, 1);
  if (vm.fault.set) return;
  ClientInfo& c = s.clients[e - 1];
  if (std::strcmp(text, "disconnect\n") == 0) {
    c.drop = true;
    NetEvent ev;
    ev.kind = EventKind::drop;
    ev.to = 1u << (e - 1);
    ev.ent = e;
    push(s, std::move(ev));
    return;
  }
  if (c.stuff.size() + std::strlen(text) > kStuffBytes) {
    pg::vm_error(vm, "stuffcmd: the client's pending command text overflowed (256 bytes)");
    return;
  }
  c.stuff += text;
  if (c.stuff.find('\n') == std::string::npos) return;
  NetEvent ev;
  ev.kind = EventKind::stuffcmd;
  ev.to = 1u << (e - 1);
  ev.reliable = true;
  ev.ent = e;
  ev.text = c.stuff;
  c.stuff.clear();
  push(s, std::move(ev));
}

// A single-player program's prints carry no level: they are sent at the
// high level, which every client shows.
constexpr int kPrintHigh = 2;

void bi_bprint(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const bool solo = pg::vm_layout(vm).single_player;
  char buf[pg::kTempStringBytes];
  pg::join_args(vm, solo ? 0 : 1, buf, sizeof buf);
  if (vm.fault.set) return;
  NetEvent ev;
  ev.kind = EventKind::print;
  ev.to = all_clients(s);
  ev.reliable = true;
  ev.level = solo ? kPrintHigh : trunc_i32(pg::arg_float(vm, 0));
  ev.text = buf;
  push(s, std::move(ev));
}

void bi_sprint(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const int e = pg::entity_number(vm, pg::arg_int(vm, 0));
  const bool solo = pg::vm_layout(vm).single_player;
  char buf[pg::kTempStringBytes];
  pg::join_args(vm, solo ? 1 : 2, buf, sizeof buf);
  if (vm.fault.set) return;
  if (e < 1 || e > sh::kMaxClients || !s.clients[e - 1].active) {
    console_print(s, "tried to sprint to a non-client");
    return;
  }
  NetEvent ev;
  ev.kind = EventKind::print;
  ev.to = (1u << (e - 1)) | spectator_copies(s, e, 2);
  ev.reliable = true;
  ev.ent = e;
  ev.level = solo ? kPrintHigh : trunc_i32(pg::arg_float(vm, 1));
  ev.text = buf;
  push(s, std::move(ev));
}

void bi_dprint(pg::Vm& vm) {
  char buf[pg::kTempStringBytes];
  pg::join_args(vm, 0, buf, sizeof buf);
  if (vm.fault.set) return;
  console_print(server_of(vm), buf);
}

void bi_centerprint(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const int e = pg::entity_number(vm, pg::arg_int(vm, 0));
  char buf[pg::kTempStringBytes];
  pg::join_args(vm, 1, buf, sizeof buf);
  if (vm.fault.set) return;
  if (e < 1 || e > sh::kMaxClients || !s.clients[e - 1].active) {
    console_print(s, "tried to centerprint to a non-client");
    return;
  }
  NetEvent ev;
  ev.kind = EventKind::centerprint;
  ev.to = (1u << (e - 1)) | spectator_copies(s, e, 1);
  ev.reliable = true;
  ev.ent = e;
  ev.text = buf;
  push(s, std::move(ev));
}

void bi_lightstyle(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const int style = trunc_i32(pg::arg_float(vm, 0));
  const char* value = pg::arg_string(vm, 1);
  if (vm.fault.set) return;
  if (style < 0 || style >= kLightStyles) {
    pg::vm_error(vm, "lightstyle: style out of 0..63");
    return;
  }
  s.lightstyles[style] = value;
  if (s.loading) return;  // the signon carries the styles a loading map sets
  NetEvent ev;
  ev.kind = EventKind::lightstyle;
  ev.to = all_clients(s);
  ev.reliable = true;
  ev.level = style;
  ev.text = value;
  push(s, std::move(ev));
}

void bi_particle(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  float org[3], dir[3];
  pg::arg_vector(vm, 0, org);
  pg::arg_vector(vm, 1, dir);
  const float colour = pg::arg_float(vm, 2), count = pg::arg_float(vm, 3);
  std::string effect;
  float effect_count = 1;
  if (vm.argc >= 5) {
    effect = "effect " + std::to_string(trunc_i32(pg::arg_float(vm, 4)));
    if (vm.argc >= 6) effect_count = pg::arg_float(vm, 5);
  } else if (count == 255) {
    effect = "explosion";
  } else if (colour == 73) {
    effect = "blood";
  } else if (colour == 225) {
    effect = "lightning blood";
  } else {
    return;  // nothing the protocol can carry
  }
  NetEvent ev;
  ev.kind = EventKind::particle;
  ev.origin = {org[0], org[1], org[2]};
  ev.to = clients_reaching(s, ev.origin, true);
  ev.values[0] = colour;
  ev.values[1] = count;
  ev.values[2] = effect_count;
  ev.values[3] = dir[2];
  ev.text = effect;
  push(s, std::move(ev));
}

void bi_writebyte(pg::Vm& vm) { write(vm, WriteKind::byte_); }
void bi_writechar(pg::Vm& vm) { write(vm, WriteKind::char_); }
void bi_writeshort(pg::Vm& vm) { write(vm, WriteKind::short_); }
void bi_writelong(pg::Vm& vm) { write(vm, WriteKind::long_); }
void bi_writecoord(pg::Vm& vm) { write(vm, WriteKind::coord); }
void bi_writeangle(pg::Vm& vm) { write(vm, WriteKind::angle); }
void bi_writestring(pg::Vm& vm) { write(vm, WriteKind::string); }
void bi_writeentity(pg::Vm& vm) { write(vm, WriteKind::entity); }

void bi_ambientsound(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  float pos[3];
  pg::arg_vector(vm, 0, pos);
  const char* sample = pg::arg_string(vm, 1);
  if (vm.fault.set) return;
  if (sound_index(s, sample) < 0) {
    console_print(s, std::string("no precache: ") + sample);
    return;
  }
  NetEvent ev;
  ev.kind = EventKind::ambient;
  ev.to = all_clients(s);
  ev.reliable = true;
  ev.origin = {pos[0], pos[1], pos[2]};
  ev.values[0] = static_cast<float>(trunc_i32(static_cast<double>(pg::arg_float(vm, 2)) * 255));
  ev.values[1] = static_cast<float>(trunc_i32(static_cast<double>(pg::arg_float(vm, 3)) * 64));
  ev.text = sample;
  push(s, std::move(ev));
}

void bi_multicast(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  float where[3];
  pg::arg_vector(vm, 0, where);
  const int to = trunc_i32(pg::arg_float(vm, 1));
  if (to < 0 || to > 5) {
    pg::vm_error(vm, "multicast: bad destination (0..5)");
    return;
  }
  const sh::Vec3f at{where[0], where[1], where[2]};
  NetEvent ev;
  ev.kind = EventKind::multicast;
  ev.reliable = to >= 3;
  const int kind = to % 3;
  ev.to = kind == 0 ? all_clients(s) : clients_reaching(s, at, kind == 2);
  ev.level = to;
  ev.origin = at;
  ev.bytes = s.out.multicast;
  s.out.multicast.clear();
  push(s, std::move(ev));
}

}  // namespace shooters_edge::server
