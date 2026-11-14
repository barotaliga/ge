// A program of rules from its table: the services over the VM, the dispatch, the layout, and a
// rules package loaded and kept open.
//
// ge:summary A rules table laid out as a native program; the services over the VM; packages
// ge:keywords rules-package rules_progs load_rules_package dispatch services native-program
// ge:skill mod progs shooter
// ge:test tests/shooters_edge_rules_package_test.cpp
#include "shooters_edge/server/rules_package.hpp"

#include <cstdio>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>

#include "mod/loader.hpp"
#include "progs/layout.hpp"
#include "progs/native.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/rules/calls.hpp"

namespace shooters_edge::server {

namespace {

namespace pg = ge::progs;

static_assert(std::is_same_v<int, int32_t>, "the declaration's offsets are handed as int32");

pg::Vm& vm_of(void* ctx) { return *static_cast<pg::Vm*>(ctx); }

bool global_in_range(pg::Vm& vm, int32_t ofs, int words) {
  if (ofs >= 0 && static_cast<size_t>(ofs) + static_cast<size_t>(words) <= vm.globals.size())
    return true;
  char m[120];
  std::snprintf(m, sizeof m, "a rule read global %d, past the program's %zu", ofs,
                vm.globals.size());
  pg::vm_error(vm, m);
  return false;
}

int32_t s_failed(void* ctx) { return vm_of(ctx).fault.set ? 1 : 0; }
uint32_t s_global(void* ctx, int32_t ofs) {
  pg::Vm& vm = vm_of(ctx);
  return global_in_range(vm, ofs, 1) ? vm.globals[static_cast<size_t>(ofs)] : 0u;
}
void s_set_global(void* ctx, int32_t ofs, uint32_t word) {
  pg::Vm& vm = vm_of(ctx);
  if (global_in_range(vm, ofs, 1)) vm.globals[static_cast<size_t>(ofs)] = word;
}
void s_field(void* ctx, int32_t ent, int32_t field, int32_t words, uint32_t* out) {
  pg::field_words(vm_of(ctx), ent, field, words == 3 ? 3 : 1, out);
}
void s_set_field(void* ctx, int32_t ent, int32_t field, int32_t words, const uint32_t* in) {
  pg::set_field_words(vm_of(ctx), ent, field, words == 3 ? 3 : 1, in);
}
const char* s_string(void* ctx, int32_t s) { return pg::vm_string(vm_of(ctx), s); }
int32_t s_call_builtin(void* ctx, int32_t number, int32_t argc) {
  return pg::call_builtin(vm_of(ctx), number, argc) ? 1 : 0;
}
int32_t s_call_function(void* ctx, int32_t number) {
  return pg::call_function(vm_of(ctx), number) ? 1 : 0;
}

constexpr se_rules_services_v1 kServices = {
    sizeof(se_rules_services_v1),
    SE_RULES_API,
    &s_failed,
    &s_global,
    &s_set_global,
    &s_field,
    &s_set_field,
    &s_string,
    &s_call_builtin,
    &s_call_function,
};

// Every function of a program from a table runs here: the call made over the VM, the table's
// function run with it.
void dispatch(pg::Vm& vm, size_t index, const void* user) {
  const auto& rules = *static_cast<const se_rules_v1*>(user);
  const pg::NativeOffsets& o = pg::native_offsets(vm);
  const pg::Layout& form = pg::vm_layout(vm);
  const se_rules_call_v1 call = {&kServices,
                                 &vm,
                                 o.globals.data(),
                                 o.fields.data(),
                                 o.strings.data(),
                                 o.functions.data(),
                                 reinterpret_cast<const int32_t*>(&form.g),
                                 kFormGlobalCount,
                                 reinterpret_cast<const int32_t*>(&form.f),
                                 kFormFieldCount};
  rules.functions[index].run(&call);
}

bool known_type(uint32_t t) {
  return t == SE_RULES_TYPE_STRING || t == SE_RULES_TYPE_FLOAT || t == SE_RULES_TYPE_VECTOR ||
         t == SE_RULES_TYPE_ENTITY || t == SE_RULES_TYPE_FUNCTION;
}

}  // namespace

bool rules_progs(const se_rules_v1& rules, pg::Progs& out, std::string& error) {
  const std::string who = std::string("the rules ") + (rules.file != nullptr ? rules.file : "?");
  if (rules.size < sizeof(se_rules_v1) || rules.api != SE_RULES_API) {
    error = who + " were built against rules API " + std::to_string(rules.api) +
            " and this game runs " + std::to_string(SE_RULES_API) + "; rebuild them";
    return false;
  }
  if (rules.form != SE_RULES_FORM_MULTIPLAYER && rules.form != SE_RULES_FORM_SINGLE_PLAYER) {
    error = who + " name form " + std::to_string(rules.form) +
            ", which is none; use SE_RULES_FORM_MULTIPLAYER or SE_RULES_FORM_SINGLE_PLAYER";
    return false;
  }
  std::vector<pg::NativeDef> globals, fields;
  for (uint32_t k = 0; k < rules.global_count; k++) {
    const se_rules_def_v1& d = rules.globals[k];
    if (d.name == nullptr || !known_type(d.type)) {
      error = who + ": global " + std::to_string(k) + " has no name or no type a global may have";
      return false;
    }
    globals.push_back({d.name, static_cast<uint16_t>(d.type)});
  }
  for (uint32_t k = 0; k < rules.field_count; k++) {
    const se_rules_def_v1& d = rules.fields[k];
    if (d.name == nullptr || !known_type(d.type)) {
      error = who + ": field " + std::to_string(k) + " has no name or no type a field may have";
      return false;
    }
    fields.push_back({d.name, static_cast<uint16_t>(d.type)});
  }
  std::vector<const char*> strings(rules.strings, rules.strings + rules.string_count);
  for (const char* s : strings)
    if (s == nullptr) {
      error = who + ": a string is null; give each string its text";
      return false;
    }
  std::vector<pg::NativeFunction> functions;
  for (uint32_t k = 0; k < rules.function_count; k++) {
    const se_rules_function_v1& f = rules.functions[k];
    if (f.name == nullptr || f.run == nullptr || f.parm_count > 8 ||
        (f.parm_count > 0 && f.parms == nullptr)) {
      error = who + ": function " + std::to_string(k) + (f.name != nullptr ? f.name : "") +
              " has no name, no C++ or more than 8 parameters";
      return false;
    }
    functions.push_back({f.name, nullptr, {f.parms, f.parm_count}});
  }
  pg::NativeProgram p;
  p.form = rules.form == SE_RULES_FORM_SINGLE_PLAYER ? &pg::kSinglePlayerLayout
                                                     : &pg::kMultiplayerLayout;
  p.file = rules.file != nullptr ? rules.file : "rules";
  p.globals = globals;
  p.fields = fields;
  p.strings = strings;
  p.functions = functions;
  p.dispatch = &dispatch;
  p.user = &rules;
  pg::NativeOffsets offsets;
  if (!pg::build_native_progs(p, out, offsets, error)) {
    error = who + ": " + error;
    return false;
  }
  return true;
}

bool load_rules_package(const std::filesystem::path& dir, pg::Progs& out, std::string& error) {
  // Kept open for the process: a progs built from a package points at its table and its code.
  static std::mutex lock;
  static std::vector<std::unique_ptr<ge::mod::LoadedMod>> kept;
  auto loaded = std::make_unique<ge::mod::LoadedMod>();
  const ge::mod::LoadResult r = ge::mod::load_mod(dir, {SE_RULES_GAME, SE_RULES_API}, *loaded);
  if (!r.ok()) {
    error = "the rules package " + dir.generic_string() + " was refused: " + r.errors.front();
    return false;
  }
  const auto* rules = static_cast<const se_rules_v1*>(loaded->table->hooks);
  if (rules == nullptr) {
    error = "the rules package " + dir.generic_string() + " hands no rules table";
    return false;
  }
  if (!rules_progs(*rules, out, error)) return false;
  const std::lock_guard<std::mutex> hold(lock);
  kept.push_back(std::move(loaded));
  return true;
}

}  // namespace shooters_edge::server
