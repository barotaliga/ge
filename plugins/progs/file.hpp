// A PROGS FILE, LOADED AND CHECKED: the compiled QC a mod ships, read from its bytes into plain
// arrays (`Progs`) and refused unless every bound and index in it is safe to run. The VM
// (vm.hpp) runs a `Progs` it never re-checks, so this is the one gate between a player's file and
// the interpreter.
//
// THE FILE (little endian): a header of fifteen i32 (version 6; the CRC of the engine-side layout,
// 54730 for the classic multiplayer one, layout.hpp; then offset and count of the statements, the
// global definitions, the field definitions, the functions, the strings (in bytes) and the
// globals (in words); then the entity size in words), and the six sections it points at:
//
//   statement   8 bytes: opcode u16, a b c i16 (global offsets, or a relative jump)
//   definition  8 bytes: type u16 (bit 15 saved, never on a field), offset u16 (words), name i32
//   function   36 bytes: first statement i32 (negative: builtin -first), first parameter's global,
//              locals (words from the first parameter's global), profile (ignored), name, file,
//              parameter count (i32), parameter sizes (8 u8)
//
// THE CHECKS (each refusal names the section, the record, the field and the value, and what to
// do): the version and the CRC; every section inside the file, no count negative; the strings
// start and end with a NUL byte; every opcode known and every operand of it that names a global
// inside the globals (three words for a vector), every jump inside the statements; every
// function's first statement inside the statements or a builtin, its parameter and local range
// inside the globals and its parameters' sizes inside that range, its parameter count 0..8 and
// each size 1 or 3; every name a string inside the table; every definition's type known, a global
// definition inside the globals and a field definition inside the entity; at least the
// engine-side globals and fields, and every definition the engine-side layout names at its
// offset with its type.
//
// A LINE TABLE (one source line per statement) is optional and comes beside the file
// (`set_lines`): a fault then names the line too (vm.hpp).
//
// ge:summary A progs file loaded and checked: header, statements, definitions, functions, strings
// ge:keywords qc progs file load loader check checker header statement definition def function
// ge:keywords strings globals entityfields crc version refuse validate bounds line-table
// ge:keywords load_progs find_function find_field find_global progs_string set_lines
// ge:skill progs
// ge:test tests/progs_file_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ge::progs {

struct Statement {
  uint16_t op;
  int16_t a, b, c;
};

struct Def {
  uint16_t type;    // with kTypeSaved in bit 15 for a saved global
  uint16_t offset;  // words: into the globals, or into an entity for a field
  int32_t name;     // a string offset
};

struct Function {
  int32_t first_statement;  // < 0: builtin number -first_statement
  int32_t parm_start;       // the first parameter's global
  int32_t locals;           // words saved on entry, from parm_start (parameters and locals)
  int32_t name, file;       // string offsets
  int32_t num_parms;        // 0..8
  uint8_t parm_size[8];     // 1 or 3 words each
};

struct Progs {
  int32_t version = 0;
  int32_t crc = 0;
  std::vector<Statement> statements;
  std::vector<Def> globaldefs, fielddefs;
  std::vector<Function> functions;  // [0] the null function
  std::vector<char> strings;        // [0] is NUL: offset 0 is the empty string
  std::vector<uint32_t> globals;    // the initial value of every global word
  int32_t entityfields = 0;         // words an entity holds
  std::vector<int32_t> lines;       // optional: the source line of each statement
};

struct LoadOptions {
  int32_t crc = 54730;       // the engine-side layout the game provides (layout.hpp's kClassicCrc)
  bool check_layout = true;  // hold the engine-side definitions to the classic layout
};

// Loads and checks `bytes`. False with `error` naming what is wrong (and `progs` unspecified).
bool load_progs(const uint8_t* bytes, size_t size, Progs& progs, std::string& error,
                const LoadOptions& options = {});

// The line table: one line per statement. False (and nothing set) when the counts differ.
bool set_lines(Progs& progs, std::vector<int32_t> lines, std::string& error);
// THE LINE TABLE AS A FILE, this engine's own sidecar `<progs>.lines` beside a progs (the file
// format carries none): text, one decimal line number a statement, separated by white space,
// '#' lines skipped. The assembler's table is written so (`lines_text`), and `ge qc` reads one
// when it is there.
std::string lines_text(const std::vector<int32_t>& lines);
bool parse_lines(const std::string& text, std::vector<int32_t>& lines, std::string& error);

// A string of the file's table (offset checked by the loader for names; "" when out of range).
const char* progs_string(const Progs& progs, int32_t offset);

// Lookups by name; the first match in file order wins. -1 (or nullptr) when there is none.
int find_function(const Progs& progs, const char* name);
const Def* find_field(const Progs& progs, const char* name);
const Def* find_global(const Progs& progs, const char* name);

}  // namespace ge::progs
