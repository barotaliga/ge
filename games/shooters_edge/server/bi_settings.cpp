// The builtins over the server's settings, lists and flow (builtins.hpp; catalogue.hpp states each
// rule): errors that stop the map, precaching, cvars, info keys, the console's command buffer, the
// map change, the frag log, the extension query, the date, and pausing.
//
// ge:summary Builtins over settings and flow: errors, precache, cvars, infokey, logfrag, pause
// ge:keywords error objerror precache_sound precache_model precache_vwep_model cvar cvar_set
// ge:keywords cvar_string registercvar localcmd changelevel logfrag infokey checkextension
// ge:keywords calltimeofday timeofday setpause pr_checkextension frag-log long-frag-log
// ge:skill progs shooter
// ge:test tests/shooters_edge_qc_builtins_test.cpp
#include <cstring>

#include "progs/stdlib.hpp"
#include "shooters_edge/server/builtins.hpp"
#include "shooters_edge/server/catalogue.hpp"

namespace shooters_edge::server {

namespace {

bool iequal(const char* a, const char* b) {
  for (; *a != 0 && *b != 0; a++, b++) {
    char x = *a, y = *b;
    if (x >= 'A' && x <= 'Z') x = static_cast<char>(x + 32);
    if (y >= 'A' && y <= 'Z') y = static_cast<char>(y + 32);
    if (x != y) return false;
  }
  return *a == *b;
}

// The error builtins' message: the running function's name, `self`, and the text.
void stop_map(pg::Vm& vm, const char* what, bool free_self) {
  QcServer& s = server_of(vm);
  char text[pg::kTempStringBytes];
  pg::join_args(vm, 0, text, sizeof text);
  const pg::Progs& p = *vm.progs;
  const char* fn = vm.function > 0
                       ? pg::progs_string(p, p.functions[static_cast<size_t>(vm.function)].name)
                       : "(the engine)";
  const int self = pg::entity_number(vm, pg::global_int(vm, pg::vm_layout(vm).g.self));
  const std::string msg =
      std::string(what) + " in " + fn + " (self " + std::to_string(self) + "): " + text;
  console_print(s, msg);
  if (free_self && self > 0) sim::qc_free_entity(sim::qc_of(vm), self);
  s.map_stopped = true;
  s.stop_reason = msg;
  pg::vm_error(vm, msg.c_str());
}

// A precache name: not empty and not starting with a character at or below a space.
bool good_name(const char* n) { return n[0] != 0 && static_cast<unsigned char>(n[0]) > ' '; }

void precache(pg::Vm& vm, std::vector<std::string>& list, int cap, const char* which) {
  QcServer& s = server_of(vm);
  const char* name = pg::arg_string(vm, 0);
  if (vm.fault.set) return;
  if (!s.loading) {
    const std::string m = std::string(which) + ": only while the map loads (in spawn functions)";
    pg::vm_error(vm, m.c_str());
    return;
  }
  if (!good_name(name)) {
    pg::vm_error(vm, "Bad string");
    return;
  }
  pg::return_int(vm, pg::arg_int(vm, 0));
  for (const std::string& have : list)
    if (have == name) return;
  if (static_cast<int>(list.size()) >= cap) {
    const std::string m = std::string(which) + ": the list is full (" + std::to_string(cap) + ")";
    pg::vm_error(vm, m.c_str());
    return;
  }
  list.emplace_back(name);
}

std::string lower(const char* s) {
  std::string out(s);
  for (char& c : out)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
  return out;
}

}  // namespace

void bi_error(pg::Vm& vm) { stop_map(vm, "error", false); }
void bi_objerror(pg::Vm& vm) { stop_map(vm, "objerror", true); }

void bi_precache_sound(pg::Vm& vm) {
  precache(vm, server_of(vm).sounds, kMaxSounds, "precache_sound");
}
void bi_precache_model(pg::Vm& vm) {
  precache(vm, server_of(vm).models, kMaxModels, "precache_model");
}

void bi_cvar(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const char* name = pg::arg_string(vm, 0);
  if (iequal(name, "pr_checkextension")) {
    pg::return_float(vm, 1);
    return;
  }
  const ge::console::Cvar* c = s.cvars.find(name);
  pg::return_float(vm, c == nullptr ? 0.0f : static_cast<float>(c->number));
}

void bi_cvar_set(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const char* name = pg::arg_string(vm, 0);
  const char* value = pg::arg_string(vm, 1);
  if (vm.fault.set) return;
  if (s.cvars.find(name) == nullptr) {
    console_print(s, std::string("cvar_set: variable ") + name + " not found");
    return;
  }
  s.cvars.set(name, value, ge::console::Source::code);
  apply_settings(s);
}

void bi_cvar_string(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const ge::console::Cvar* c = s.cvars.find(pg::arg_string(vm, 0));
  pg::return_int(vm, c == nullptr ? 0 : pg::builtin_temp_string(vm, c->value.c_str()));
}

void bi_registercvar(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const std::string name = lower(pg::arg_string(vm, 0));
  if (vm.fault.set) return;
  if (s.cvars.find(name) != nullptr) {
    pg::return_float(vm, 0);
    return;
  }
  // Made with its NAME as its value (the value argument is not read), as a mod expects.
  const ge::console::SetResult r = s.cvars.set(name, name, ge::console::Source::code, true, 0);
  pg::return_float(vm, r == ge::console::SetResult::changed ? 1.0f : 0.0f);
}

void bi_localcmd(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  s.command_buffer += pg::arg_string(vm, 0);
}

void bi_changelevel(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const char* map = pg::arg_string(vm, 0);
  if (vm.fault.set || s.changelevel_queued) return;
  s.changelevel_queued = true;
  s.command_buffer += std::string("map ") + map + "\n";
}

void bi_logfrag(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const int killer = client_slot_of(s, pg::entity_number(vm, pg::arg_int(vm, 0)));
  const int killed = client_slot_of(s, pg::entity_number(vm, pg::arg_int(vm, 1)));
  if (killer < 0 || killed < 0) return;
  const std::string a = info_value(s.clients[killer].userinfo, "name");
  const std::string b = info_value(s.clients[killed].userinfo, "name");
  const ge::console::Cvar* longlog = s.cvars.find("sv_longfraglog");
  if (longlog != nullptr && longlog->on()) {
    const DateTime d = s.opt.clock(s.opt.clock_ctx, s.sv.time);
    s.frag_log.push_back(
        "\\frag\\" + a + "\\" + b + "\\" + info_value(s.clients[killer].userinfo, "team") + "\\" +
        info_value(s.clients[killed].userinfo, "team") + "\\" + date_text(d) + "\\\n");
  } else {
    s.frag_log.push_back("\\" + a + "\\" + b + "\\\n");
  }
}

void bi_infokey(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const int e = pg::entity_number(vm, pg::arg_int(vm, 0));
  const char* key = pg::arg_string(vm, 1);
  if (vm.fault.set) return;
  std::string value;
  if (e == 0) {
    value = info_value(s.serverinfo, key);
    if (value.empty()) value = info_value(s.localinfo, key);
  } else if (const int slot = client_slot_of(s, e); slot >= 0) {
    const ClientInfo& c = s.clients[slot];
    if (std::strcmp(key, "ip") == 0 || std::strcmp(key, "realip") == 0)
      value = c.address;
    else if (std::strcmp(key, "download") == 0)
      value = "-1";
    else if (std::strcmp(key, "ping") == 0)
      value = std::to_string(c.ping_ms);
    else if (std::strcmp(key, "*userid") == 0)
      value = std::to_string(c.userid);
    else if (std::strcmp(key, "login") == 0)
      value = "";
    else
      value = info_value(c.userinfo, key);
  }
  pg::return_int(vm, pg::builtin_temp_string(vm, value.c_str()));
}

void bi_checkextension(pg::Vm& vm) {
  pg::return_float(vm, offers_extension(server_of(vm), pg::arg_string(vm, 0)) ? 1.0f : 0.0f);
}

void bi_calltimeofday(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  if (s.named.timeofday <= 0) return;
  const DateTime d = s.opt.clock(s.opt.clock_ctx, s.sv.time);
  const float parts[6] = {static_cast<float>(d.second), static_cast<float>(d.minute),
                          static_cast<float>(d.hour),   static_cast<float>(d.day),
                          static_cast<float>(d.month),  static_cast<float>(d.year)};
  for (int i = 0; i < 6; i++) pg::set_global_float(vm, pg::parm(i), parts[i]);
  pg::set_global_int(vm, pg::parm(6), pg::temp_string(vm, date_text(d).c_str()));
  note_hook(s, HookId::timeofday, 0);
  pg::call_function(vm, s.named.timeofday);  // a fault here stops the calling QC too
}

void bi_setpause(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const bool want = pg::arg_float(vm, 0) != 0;
  if (want == s.paused) return;
  s.paused = want;
  if (want) s.paused_seconds = 0;
  NetEvent ev;
  ev.kind = EventKind::pause;
  ev.to = all_clients(s);
  ev.reliable = true;
  ev.time = s.sv.time;
  ev.level = want ? 1 : 0;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

void bi_precache_vwep_model(pg::Vm& vm) {
  QcServer& s = server_of(vm);
  const char* name = pg::arg_string(vm, 0);
  if (vm.fault.set) return;
  if (!s.loading) {
    pg::vm_error(vm, "precache_vwep_model: only while the map loads");
    return;
  }
  bool bad = !good_name(name);
  for (const char* c = name; *c != 0 && !bad; c++)
    if (*c == '"' || *c == ';' || *c == '\n' || *c == '\t' || *c == ' ') bad = true;
  if (bad) {
    pg::vm_error(vm, "precache_vwep_model: bad name (empty, or a quote, ';', space or newline)");
    return;
  }
  if (static_cast<int>(s.vwep.size()) >= kMaxVwep) {
    pg::vm_error(vm, "precache_vwep_model: the list is full (32)");
    return;
  }
  s.vwep.emplace_back(name);
  pg::return_float(vm, static_cast<float>(s.vwep.size() - 1));
}

}  // namespace shooters_edge::server
