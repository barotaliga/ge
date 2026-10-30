// The disassembler (disasm.hpp).
//
// ge:summary The QC disassembler's definitions: operand names, constants, function ranges
// ge:keywords qc progs disasm operand constant function-range
// ge:skill progs
// ge:test tests/progs_tool_test.cpp
#include "progs/disasm.hpp"

#include <bit>
#include <cstdio>

#include "progs/layout.hpp"
#include "progs/opcodes.hpp"

namespace ge::progs {

const char* global_name(const Progs& p, int ofs) {
  for (const Def& d : p.globaldefs) {
    if (d.offset == ofs) return progs_string(p, d.name);
  }
  return "";
}

namespace {

std::string operand(const Progs& p, int index, int v, Operand kind) {
  char b[96];
  if (kind == Operand::jump) {
    std::snprintf(b, sizeof b, "-> %d", index + v);
    return b;
  }
  const char* name = global_name(p, v);
  if (name[0] != 0) return name;
  if (v >= kEngineGlobalsEnd && static_cast<size_t>(v) < p.globals.size()) {
    const uint32_t w = p.globals[static_cast<size_t>(v)];
    if (w != 0) {
      std::snprintf(b, sizeof b, "@%d(#%.9g|%d)", v, static_cast<double>(std::bit_cast<float>(w)),
                    static_cast<int32_t>(w));
      return b;
    }
  }
  std::snprintf(b, sizeof b, "@%d", v);
  return b;
}

}  // namespace

std::string statement_text(const Progs& p, int index) {
  const Statement& s = p.statements[static_cast<size_t>(index)];
  char head[48];
  std::snprintf(head, sizeof head, "%6d  %-10s", index, op_name(s.op));
  std::string out = head;
  if (s.op >= kOpCount) return out;
  const OpShape& sh = op_shape(s.op);
  const Operand kinds[3] = {sh.a, sh.b, sh.c};
  const int values[3] = {s.a, s.b, s.c};
  const char tags[3] = {'a', 'b', 'c'};
  for (int k = 0; k < 3; k++) {
    if (kinds[k] == Operand::none) continue;
    out += ' ';
    out += tags[k];
    out += ':';
    out += operand(p, index, values[k], kinds[k]);
  }
  if (static_cast<size_t>(index) < p.lines.size()) {
    out += "  ; line " + std::to_string(p.lines[static_cast<size_t>(index)]);
  }
  return out;
}

void function_range(const Progs& p, int fn, int& first, int& end) {
  const Function& f = p.functions[static_cast<size_t>(fn)];
  first = end = 0;
  if (f.first_statement <= 0) return;
  first = f.first_statement;
  end = static_cast<int>(p.statements.size());
  for (const Function& g : p.functions) {
    if (g.first_statement > first && g.first_statement < end) end = g.first_statement;
  }
}

std::string function_text(const Progs& p, int fn) {
  const Function& f = p.functions[static_cast<size_t>(fn)];
  std::string out = "function " + std::to_string(fn) + " " + progs_string(p, f.name) + " (" +
                    progs_string(p, f.file) + ")";
  if (f.first_statement < 0) {
    return out + ": builtin " + std::to_string(-f.first_statement) + ", " +
           std::to_string(f.num_parms) + " parameters\n";
  }
  out += ": " + std::to_string(f.num_parms) + " parameters (";
  for (int i = 0; i < f.num_parms; i++)
    out += (i ? " " : "") + std::to_string(static_cast<int>(f.parm_size[i]));
  out += "), locals " + std::to_string(f.locals) + " at @" + std::to_string(f.parm_start) +
         ", first statement " + std::to_string(f.first_statement) + "\n";
  int first = 0, end = 0;
  function_range(p, fn, first, end);
  for (int i = first; i < end; i++) out += statement_text(p, i) + "\n";
  return out;
}

std::string summary_text(const Progs& p) {
  int builtins = 0;
  for (const Function& f : p.functions) builtins += f.first_statement < 0 ? 1 : 0;
  return "progs version " + std::to_string(p.version) + ", crc " + std::to_string(p.crc) + ": " +
         std::to_string(p.statements.size()) + " statements, " +
         std::to_string(p.functions.size()) + " functions (" + std::to_string(builtins) +
         " builtins), " + std::to_string(p.globals.size()) + " global words, " +
         std::to_string(p.globaldefs.size()) + " global definitions, " +
         std::to_string(p.fielddefs.size()) + " fields in " + std::to_string(p.entityfields) +
         " words, " + std::to_string(p.strings.size()) + " bytes of strings" +
         (p.lines.empty() ? "" : ", a line table");
}

}  // namespace ge::progs
