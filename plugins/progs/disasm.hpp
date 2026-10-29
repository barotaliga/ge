// THE DISASSEMBLER: a progs as text an agent reads (`ge qc disasm`). A statement prints as its
// index, opcode and operands, each global operand named by its definition when one starts there
// ("self", "origin") and shown with its initial value when it is an unnamed constant, a jump as
// its target ("-> 42"); a function prints as its header (name, file, parameters, locals, first
// statement or builtin number) and its statements up to the next function's first.
//
// ge:summary The QC disassembler: statements, operands named by definition, functions as text
// ge:keywords qc progs disasm disassemble disassembler listing statement operand describe
// ge:keywords statement_text function_text summary_text
// ge:skill progs
// ge:test tests/progs_tool_test.cpp
#pragma once

#include <string>

#include "progs/file.hpp"

namespace ge::progs {

// One statement: "  12  ADD_F  a:time b:#0.1 c:@95".
std::string statement_text(const Progs& p, int index);
// A function's header and body.
std::string function_text(const Progs& p, int fn);
// The file's counts: statements, functions (and builtins), globals, fields, strings, the CRC.
std::string summary_text(const Progs& p);
// The name of the global at `ofs` when a definition starts there, else "".
const char* global_name(const Progs& p, int ofs);
// The statements function `fn` owns: [first, end). Empty for a builtin.
void function_range(const Progs& p, int fn, int& first, int& end);

}  // namespace ge::progs
