// A NATIVE PROGRAM: a program whose functions are C++, run by the VM and the game exactly where a
// compiled program's would be. It declares what a compiled program would hold, and the progs is
// built from that declaration:
//
// - its FORM (layout.hpp), whose engine-side globals and fields come with it;
// - its own GLOBALS and FIELDS by name and type, laid out after the form's as a compiler lays
//   them out; the globals are its whole state, so a snapshot of the VM's memory holds it;
// - its STRINGS, each a string value in the program's table (a classname, a model, a message);
// - its FUNCTIONS by name with their parameters' sizes: each is a function record whose first
//   statement is -(kNativeBase + i), so the game finds a hook or a spawn function by name and a
//   field stores it, and the VM runs `natives[i]` when it is called (from the game, from bytecode
//   or from another native function through `call_function`).
//
// `build_native_progs` lays the declaration out and checks it as any progs is checked;
// `bind_natives` points a VM made over that progs at the functions and the offsets they read.
// A native function reads its arguments from the parameter slots, as a builtin does, and writes
// its result to the return slot; the helpers below read and write words as the bytecode does
// (floats as 32-bit floats), so a function carried over statement for statement computes the same
// bits.
//
// ge:summary A native program: a progs built from a declaration whose functions are C++
// ge:keywords native native-program native-function build_native_progs bind_natives knativebase
// ge:keywords declaration globals fields strings functions native-rules no-bytecode
// ge:skill progs
// ge:test tests/progs_native_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "progs/file.hpp"
#include "progs/layout.hpp"
#include "progs/vm.hpp"

namespace ge::progs {

struct NativeDef {
  const char* name;
  uint16_t type;  // kTypeFloat, kTypeVector, kTypeString, kTypeEntity, kTypeFunction
};

struct NativeFunction {
  const char* name;
  Builtin run;
  std::span<const uint8_t> parms = {};  // each parameter's words (1, or 3 for a vector)
};

struct NativeProgram {
  const Layout* form = &kMultiplayerLayout;
  const char* file = "native";  // what a frame names as the function's file
  std::span<const NativeDef> globals;
  std::span<const NativeDef> fields;
  std::span<const char* const> strings;
  std::span<const NativeFunction> functions;
};

// Where the declaration landed, by its index in each list: a global's offset, a field's offset,
// a string's value and a function's number (what a field stores and `call_function` takes).
struct NativeOffsets {
  std::vector<int> globals, fields;
  std::vector<int32_t> strings, functions;
  std::vector<Builtin> runs;  // the functions' C++, by index (what `bind_natives` points at)
};

// `program` laid out as a progs in `out`, with where each declaration landed. False with `error`
// (a name declared twice, a type that is none, more than eight parameters, a field the form
// already has with another type).
bool build_native_progs(const NativeProgram& program, Progs& out, NativeOffsets& offsets,
                        std::string& error);

// A VM made over the built progs runs its native functions; `offsets` must outlive the VM's use.
void bind_natives(Vm& vm, const NativeOffsets& offsets);
inline const NativeOffsets& native_offsets(const Vm& vm) {
  return *static_cast<const NativeOffsets*>(vm.native_data);
}

// WORDS as the bytecode reads and writes them. An entity is its value (number x fields x 4);
// a field access on a value that names no entity in use faults the call and reads 0.
struct Vec3 {
  float x = 0, y = 0, z = 0;
};
Vec3 global_vec(const Vm& vm, int ofs);
void set_global_vec(Vm& vm, int ofs, Vec3 v);
float field_float(Vm& vm, int32_t ent, int field);
int32_t field_int(Vm& vm, int32_t ent, int field);  // an entity, string or function
Vec3 field_vec(Vm& vm, int32_t ent, int field);
void set_field_float(Vm& vm, int32_t ent, int field, float v);
void set_field_int(Vm& vm, int32_t ent, int field, int32_t v);
void set_field_vec(Vm& vm, int32_t ent, int field, Vec3 v);

// A CALL from native code: the arguments into the parameter slots, then the builtin (by number)
// or the function (by its number) run; the return slot read after.
inline void set_parm_float(Vm& vm, int i, float v) { set_global_float(vm, parm(i), v); }
inline void set_parm_int(Vm& vm, int i, int32_t v) { set_global_int(vm, parm(i), v); }
inline void set_parm_vec(Vm& vm, int i, Vec3 v) { set_global_vec(vm, parm(i), v); }
inline float parm_float(const Vm& vm, int i) { return global_float(vm, parm(i)); }
inline int32_t parm_int(const Vm& vm, int i) { return global_int(vm, parm(i)); }
inline Vec3 parm_vec(const Vm& vm, int i) { return global_vec(vm, parm(i)); }
inline float result_float(const Vm& vm) { return global_float(vm, kGlobalReturn); }
inline int32_t result_int(const Vm& vm) { return global_int(vm, kGlobalReturn); }
inline Vec3 result_vec(const Vm& vm) { return global_vec(vm, kGlobalReturn); }
inline void set_return_float(Vm& vm, float v) { set_global_float(vm, kGlobalReturn, v); }
inline void set_return_int(Vm& vm, int32_t v) { set_global_int(vm, kGlobalReturn, v); }
inline void set_return_vec(Vm& vm, Vec3 v) { set_global_vec(vm, kGlobalReturn, v); }

// `&` and `|` on floats as the bytecode does them: each side truncated toward zero to a 32-bit
// integer (out of range or NaN: -2147483648), the result back to a float.
float bit_and(float a, float b);
float bit_or(float a, float b);

}  // namespace ge::progs
