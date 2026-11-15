// A WEBASSEMBLY MODULE, READ AND CHECKED (docs/mods.md § The store form, M52c): the portable form
// of a mod, the same C++ source compiled for `wasm32`, which the engine's own interpreter
// (interp.hpp) runs where native code may not load (iOS, Android, a store build).
//
// WHAT IS READ: the MVP's binary format, version 1, with the few later features clang emits by
// default (sign extension, the saturating float-to-int conversions, multiple values from a block,
// mutable globals) and `memory.copy` / `memory.fill`. Sections: types, imports (functions only),
// functions, one table of function references, one memory, globals, exports, the start function,
// element segments (active, into table 0, at a constant offset), code, data segments (active, at a
// constant offset) and the data count; custom sections (names, producers) are skipped. Anything
// else (an import of a memory, a table or a global; a second memory; passive segments; threads;
// SIMD; reference types beyond the one table) is refused with the section, the byte offset and
// what to build instead.
//
// THE CHECK: every function body is validated as the format's own rules say (every operand's
// type, every block's results, every branch's label, every index in range), and compiled while it
// is checked into `Inst`s: an instruction a slot, its immediates decoded, each branch resolved to
// the instruction it goes to, how many values it keeps and the stack height it cuts back to. So
// the interpreter keeps no control stack and checks no type: a module that passed cannot make it
// read past its stack or run an instruction with the wrong operands.
//
// ge:summary A WebAssembly module read, validated and compiled to the interpreter's instructions
// ge:keywords wasm webassembly wasm32 module read_module validate store-form portable-mod
// ge:keywords func-type import export table memory global element data inst branch
// ge:skill mod
// ge:test tests/mod_wasm_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ge::mod::wasm {

enum class ValType : uint8_t { i32 = 0x7f, i64 = 0x7e, f32 = 0x7d, f64 = 0x7c };

const char* type_name(ValType t);

struct FuncType {
  std::vector<ValType> params, results;
  bool operator==(const FuncType&) const = default;
};

// The interpreter's own opcodes past the format's (a format opcode keeps its byte; 0xFC n is
// kPrefixFC + n).
enum : uint16_t {
  kPrefixFC = 0x100,
  kOpJump = 0x200,         // a = target
  kOpJumpIfFalse = 0x201,  // pops an i32; a = target when it is 0
  kOpBr = 0x202,           // a = target, b = keep << 32 | height
  kOpBrIf = 0x203,         // pops an i32; the branch when it is not 0
  kOpBrTable = 0x204,      // pops an index; a = first of `count` + 1 targets in `Func::table`
  kOpReturn = 0x205,       // a = how many results to keep
};

// One compiled instruction: the opcode, then its immediates (an index, a memory offset, a
// branch's target; a constant's bits or a branch's keep and height).
struct Inst {
  uint16_t op = 0;
  uint32_t a = 0;
  uint64_t b = 0;
};

// A branch target of a br_table: the instruction, the values kept, the height cut back to.
struct Target {
  uint32_t pc = 0;
  uint32_t keep = 0;
  uint32_t height = 0;
};

struct Func {
  uint32_t type = 0;            // its FuncType
  std::vector<ValType> locals;  // params first, then the declared locals
  std::vector<Inst> code;       // ends with a kOpReturn
  std::vector<Target> table;    // the br_tables' targets
  uint32_t max_height = 0;      // the most operands on its stack at once
};

struct Import {
  std::string module, name;
  uint32_t type = 0;  // a function's FuncType (only functions are imported)
};

enum class ExternKind : uint8_t { func = 0, table = 1, memory = 2, global = 3 };

struct Export {
  std::string name;
  ExternKind kind = ExternKind::func;
  uint32_t index = 0;
};

struct Global {
  ValType type = ValType::i32;
  bool mut = false;
  uint64_t init = 0;  // the constant's bits
};

struct Segment {
  uint32_t offset = 0;
  std::vector<uint32_t> funcs;  // an element segment's
  std::vector<uint8_t> bytes;   // a data segment's
};

constexpr uint32_t kPageBytes = 65536;
constexpr uint32_t kNoStart = 0xffffffffu;

struct Module {
  std::vector<FuncType> types;
  std::vector<Import> imports;  // function indices 0 .. imports.size() - 1
  std::vector<Func> funcs;      // then these
  bool has_table = false;
  uint32_t table_min = 0, table_max = 0xffffffffu;
  bool has_memory = false;
  uint32_t memory_min = 0, memory_max = 0x10000;  // pages
  std::vector<Global> globals;
  std::vector<Export> exports;
  uint32_t start = kNoStart;
  std::vector<Segment> elements, data;

  uint32_t func_count() const { return static_cast<uint32_t>(imports.size() + funcs.size()); }
  // A function's type by its index (an import's or a defined one's).
  const FuncType& func_type(uint32_t index) const;
  // The export called `name` of `kind`, or nullptr.
  const Export* find_export(std::string_view name, ExternKind kind) const;
};

// `bytes` read and checked into `out`; false with `error` naming where and what to do.
bool read_module(const uint8_t* bytes, size_t size, Module& out, std::string& error);

}  // namespace ge::mod::wasm
