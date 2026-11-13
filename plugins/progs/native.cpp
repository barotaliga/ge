// A native program laid out by the assembler and run by the VM (native.hpp).
//
// ge:summary A native program's progs built from its declaration, its functions bound to a VM
// ge:keywords native build_native_progs bind_natives field_float set_field_vec native-offsets
// ge:skill progs
// ge:test tests/progs_native_test.cpp
#include "progs/native.hpp"

#include <climits>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <set>
#include <string_view>

#include "progs/assembler.hpp"

namespace ge::progs {

namespace {

bool known_type(uint16_t t) {
  return t == kTypeFloat || t == kTypeVector || t == kTypeString || t == kTypeEntity ||
         t == kTypeFunction;
}

// The entity a value names, or -1 after faulting the call.
int entity_of(Vm& vm, int32_t ent, int field, int words) {
  if (vm.fault.set) return -1;  // the call is over: nothing more is read or written, as bytecode
  const int n = entity_number(vm, ent);
  if (n < 0 || field < 0 || field + words > vm.entityfields) {
    char m[160];
    std::snprintf(m, sizeof m, "a native function read field %d of %d, which names no entity",
                  field, ent);
    vm_error(vm, m);
    return -1;
  }
  return n;
}

// Whether the assembler already holds a global named `name` (the form's own).
bool find_global_def(const Assembler& a, const char* name) {
  for (const Def& d : a.globaldefs)
    if (d.name >= 0 && static_cast<size_t>(d.name) < a.strings.size() &&
        std::string_view(a.strings.data() + d.name) == name)
      return true;
  return false;
}

// Whether a write to entity `n` is refused: the world's fields are the map's once it runs, as
// the bytecode's ADDRESS of a world field is.
bool world_refused(Vm& vm, int n) {
  if (n != 0 || !vm.world_locked) return false;
  vm_error(vm, "a native function wrote a field of the world after the map loaded");
  return true;
}

}  // namespace

bool build_native_progs(const NativeProgram& program, Progs& out, NativeOffsets& offsets,
                        std::string& error) {
  Assembler a(*program.form);
  std::set<std::string> names;
  const auto fresh = [&](const char* kind, const char* name) {
    if (names.insert(name).second) return true;
    error = std::string("native program: ") + kind + " " + name +
            " is declared twice; give each declaration its own name";
    return false;
  };
  for (const NativeDef& g : program.globals) {
    if (!fresh("global", g.name)) return false;
    if (!known_type(g.type) || find_global_def(a, g.name)) {
      error = std::string("native program: global ") + g.name +
              (known_type(g.type) ? " is the form's own; read it at the form's offset"
                                  : " has no type a global may have; use float, vector, string, "
                                    "entity or function");
      return false;
    }
    a.global(g.name, g.type);
  }
  for (const NativeDef& f : program.fields) {
    if (!fresh("field", f.name)) return false;
    if (!known_type(f.type)) {
      error = std::string("native program: field ") + f.name +
              " has no type a field may have; use float, vector, string, entity or function";
      return false;
    }
    a.field(f.name, f.type);
  }
  offsets.strings.clear();
  for (const char* s : program.strings) offsets.strings.push_back(a.string(s));
  offsets.functions.clear();
  offsets.runs.clear();
  for (size_t i = 0; i < program.functions.size(); i++) {
    const NativeFunction& fn = program.functions[i];
    if (!fresh("function", fn.name)) return false;
    if (fn.parms.size() > 8) {
      error = std::string("native program: function ") + fn.name +
              " takes more than 8 parameters, which no call can pass";
      return false;
    }
    const int32_t at = a.declare(fn.name);
    Function& rec = a.functions[static_cast<size_t>(at)];
    rec.first_statement = -(kNativeBase + static_cast<int32_t>(i));
    rec.file = a.string(program.file);
    rec.num_parms = static_cast<int32_t>(fn.parms.size());
    // Bounded by the record's slots as well (checked above), so the optimizer sees no overflow.
    for (size_t k = 0; k < fn.parms.size() && k < std::size(rec.parm_size); k++)
      rec.parm_size[k] = fn.parms[k];
    offsets.functions.push_back(at);
    offsets.runs.push_back(fn.run);
  }
  // The form's function globals (the hooks) hold the function of their name, as a compiler
  // initializes them.
  for (int k = 0; k < program.form->num_globals; k++) {
    const LayoutEntry& g = program.form->globals[k];
    if ((g.type & kTypeMask) != kTypeFunction) continue;
    for (size_t i = 0; i < program.functions.size(); i++)
      if (std::string_view(program.functions[i].name) == g.name)
        a.globals[static_cast<size_t>(g.offset)] = static_cast<uint32_t>(offsets.functions[i]);
  }
  if (!a.build(out, error)) {
    error = "native program: " + error;
    return false;
  }
  offsets.globals.clear();
  for (const NativeDef& g : program.globals)
    offsets.globals.push_back(find_global(out, g.name)->offset);
  offsets.fields.clear();
  for (const NativeDef& f : program.fields) {
    const Def* d = find_field(out, f.name);
    if (d == nullptr || (d->type & kTypeMask) != f.type) {
      error = std::string("native program: field ") + f.name +
              " is the form's own with another type; declare it as the form has it";
      return false;
    }
    offsets.fields.push_back(d->offset);
  }
  return true;
}

void bind_natives(Vm& vm, const NativeOffsets& offsets) {
  vm.natives = offsets.runs;
  vm.native_data = &offsets;
}

Vec3 global_vec(const Vm& vm, int ofs) {
  return {global_float(vm, ofs), global_float(vm, ofs + 1), global_float(vm, ofs + 2)};
}
void set_global_vec(Vm& vm, int ofs, Vec3 v) {
  set_global_float(vm, ofs, v.x);
  set_global_float(vm, ofs + 1, v.y);
  set_global_float(vm, ofs + 2, v.z);
}

float field_float(Vm& vm, int32_t ent, int field) {
  const int n = entity_of(vm, ent, field, 1);
  return n < 0 ? 0.0f : as_float(*entity_field(vm, n, field));
}
int32_t field_int(Vm& vm, int32_t ent, int field) {
  const int n = entity_of(vm, ent, field, 1);
  return n < 0 ? 0 : static_cast<int32_t>(*entity_field(vm, n, field));
}
Vec3 field_vec(Vm& vm, int32_t ent, int field) {
  const int n = entity_of(vm, ent, field, 3);
  if (n < 0) return {};
  const uint32_t* w = entity_field(vm, n, field);
  return {as_float(w[0]), as_float(w[1]), as_float(w[2])};
}
void set_field_float(Vm& vm, int32_t ent, int field, float v) {
  const int n = entity_of(vm, ent, field, 1);
  if (n >= 0 && !world_refused(vm, n)) *entity_field(vm, n, field) = as_word(v);
}
void set_field_int(Vm& vm, int32_t ent, int field, int32_t v) {
  const int n = entity_of(vm, ent, field, 1);
  if (n >= 0 && !world_refused(vm, n)) *entity_field(vm, n, field) = static_cast<uint32_t>(v);
}
void set_field_vec(Vm& vm, int32_t ent, int field, Vec3 v) {
  const int n = entity_of(vm, ent, field, 3);
  if (n < 0 || world_refused(vm, n)) return;
  uint32_t* w = entity_field(vm, n, field);
  w[0] = as_word(v.x);
  w[1] = as_word(v.y);
  w[2] = as_word(v.z);
}

namespace {

// Truncation toward zero to a 32-bit integer, out of range or NaN -2147483648, as BITAND and
// BITOR truncate.
int32_t word_int(float f) {
  if (!(f > -2147483904.0f && f < 2147483648.0f)) return INT32_MIN;
  return static_cast<int32_t>(f);
}

}  // namespace

float bit_and(float a, float b) { return static_cast<float>(word_int(a) & word_int(b)); }
float bit_or(float a, float b) { return static_cast<float>(word_int(a) | word_int(b)); }

}  // namespace ge::progs
