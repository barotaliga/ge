// THE ASSEMBLER: builds a progs in memory, statement by statement, so the tests, the `qc` lab and
// the sample program have bytecode without a compiler (none is in the tree until M18d, and none
// from outside may be used). It writes exactly the file the loader reads (file.hpp): a new
// assembler already holds the reserved globals, one program form's engine-side globals and fields
// with their definitions (layout.hpp: the multiplayer form unless it is made with another), the
// null function and statement 0, so what it builds loads with that form's crc.
//
// HOW CODE IS LAID OUT. A function's parameters and locals are one contiguous range of globals
// (`begin_function` reserves it); everything else (constants, a program's own globals, the
// globals holding a field's offset or a function's index) is allocated outside every range.
// Calls follow the convention: arguments stored into the parameter slots (global 4 + 3i), then
// CALLn on a global holding the function's index (`call`). Jumps are relative; `jump` emits one
// to a label and `place` binds the label wherever it lands.
//
// `line` sets the source line of the statements emitted after it; `lines()` is the table a fault
// reads (file.hpp `set_lines`).
//
// ge:summary The tests' and the lab's QC assembler: a progs built in memory, statement by statement
// ge:keywords qc progs assembler assemble bytecode emit label jump call constant field function
// ge:keywords builtin begin_function local test-program golden
// ge:skill progs
// ge:test tests/progs_vm_test.cpp tests/progs_ops_test.cpp
#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

#include "progs/file.hpp"
#include "progs/layout.hpp"
#include "progs/opcodes.hpp"

namespace ge::progs {

struct Assembler {
  explicit Assembler(const Layout& layout = kMultiplayerLayout);

  // A string in the table (deduplicated); its offset.
  int32_t string(const char* text);

  // A named global of `type` with its initial words (zeros when fewer); its offset.
  int global(const char* name, uint16_t type, std::initializer_list<uint32_t> init = {});
  // An unnamed global of `words` words (a temporary or a constant); its offset.
  int temp(int words = 1);
  // Constants: a global holding the value.
  int constant_f(float v);
  int constant_v(float x, float y, float z);
  int constant_i(int32_t v);          // a raw word (an entity, function or field value)
  int constant_s(const char* text);   // a string's offset
  int constant_fn(int32_t function);  // a function's index
  // A field of this program (after the engine-side ones): its definition, and a global holding
  // its offset, which is what LOAD and ADDRESS take as `b`. For an engine-side field the global
  // holds its layout offset.
  int field(const char* name, uint16_t type);

  // Functions. `declare` names one (its index, for a call before its body); `builtin` is a
  // builtin of the game's table; `begin_function` starts the body of a declared or new function
  // with its parameters' sizes and `local_words` more words, and `end_function` closes it.
  int32_t declare(const char* name);
  int32_t builtin(const char* name, int number, std::initializer_list<uint8_t> parm_sizes = {});
  int32_t begin_function(const char* name, std::initializer_list<uint8_t> parm_sizes,
                         int local_words, const char* file = "test.qc");
  // Parameter i's global and local k's (words from the end of the parameters) in the open body.
  int param(int i) const;
  int local(int k) const;
  void end_function();

  // Statements. `emit` writes one and returns its index.
  int emit(uint16_t op, int a = 0, int b = 0, int c = 0);
  // Stores each argument (a global of `words` words) into its parameter slot, then CALLn on the
  // global holding the callee's index.
  void call(int32_t function, std::initializer_list<std::pair<int, int>> args = {});
  // Labels for relative jumps: `label` makes one, `jump` emits IF / IFNOT (on `cond`) or GOTO to
  // it, `place` binds it to the next statement (patching every jump to it).
  int label();
  void jump(uint16_t op, int label_id, int cond = 0);
  void place(int label_id);

  void line(int32_t n) { line_ = n; }

  // The file's bytes (every label placed, no function open) and the line table.
  std::vector<uint8_t> bytes() const;
  const std::vector<int32_t>& lines() const { return lines_; }
  // Loaded and checked, with its line table: a builder's mistake fails here with the checker's
  // message.
  bool build(Progs& out, std::string& error) const;

  // What the assembler has laid out (read by the tests).
  std::vector<Statement> statements;
  std::vector<Def> globaldefs, fielddefs;
  std::vector<Function> functions;
  std::vector<char> strings;
  std::vector<uint32_t> globals;
  int32_t entityfields = kEngineFields;
  const Layout* layout;  // the form it lays out and stamps

 private:
  int alloc(int words);
  std::vector<int32_t> lines_;
  int32_t line_ = 0;
  int open_ = -1;  // the function whose body is open
  int open_parm_words_ = 0;
  struct Label {
    int at = -1;
    std::vector<int> uses;
  };
  std::vector<Label> labels_;
};

}  // namespace ge::progs
