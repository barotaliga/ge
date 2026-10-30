// THE QC INSTRUCTION SET: every opcode a progs statement may carry, its name, and what each of its
// three operands is (a global of one word, a global of three, a relative jump, or unused). The
// checker (file.hpp) reads the operand kinds to hold every global operand inside the globals and
// every jump inside the statements; the interpreter (vm.cpp) and the disassembler (disasm.hpp)
// read the same table, so an opcode's shape is stated once here.
//
// Numbers 0..65; anything else is the error "bad opcode N" (the checker refuses it at load).
//
// ge:summary The QC instruction set: opcode numbers, names and the kind of each operand
// ge:keywords qc progs opcode opcodes instruction statement operand jump op_name op_shape
// ge:keywords bytecode instruction-set call store load address state goto
// ge:skill progs
// ge:test tests/progs_vm_test.cpp
#pragma once

#include <cstdint>

namespace ge::progs {

enum Op : uint16_t {
  kOpDone = 0,
  kOpMulF,
  kOpMulV,
  kOpMulFV,
  kOpMulVF,
  kOpDivF,
  kOpAddF,
  kOpAddV,
  kOpSubF,
  kOpSubV,
  kOpEqF,  // 10
  kOpEqV,
  kOpEqS,
  kOpEqE,
  kOpEqFnc,
  kOpNeF,
  kOpNeV,
  kOpNeS,
  kOpNeE,
  kOpNeFnc,
  kOpLe,  // 20
  kOpGe,
  kOpLt,
  kOpGt,
  kOpLoadF,
  kOpLoadV,
  kOpLoadS,
  kOpLoadEnt,
  kOpLoadFld,
  kOpLoadFnc,
  kOpAddress,  // 30
  kOpStoreF,
  kOpStoreV,
  kOpStoreS,
  kOpStoreEnt,
  kOpStoreFld,
  kOpStoreFnc,
  kOpStorePF,
  kOpStorePV,
  kOpStorePS,
  kOpStorePEnt,  // 40
  kOpStorePFld,
  kOpStorePFnc,
  kOpReturn,
  kOpNotF,
  kOpNotV,
  kOpNotS,
  kOpNotEnt,
  kOpNotFnc,
  kOpIf,
  kOpIfNot,  // 50
  kOpCall0,
  kOpCall1,
  kOpCall2,
  kOpCall3,
  kOpCall4,
  kOpCall5,
  kOpCall6,
  kOpCall7,
  kOpCall8,
  kOpState,  // 60
  kOpGoto,
  kOpAnd,
  kOpOr,
  kOpBitAnd,
  kOpBitOr,
  kOpCount,  // 66: the first number that is not an opcode
};

// What one operand of a statement is.
enum class Operand : uint8_t {
  none,    // unused (its value is not read and not checked)
  word,    // a global of one word
  vector,  // a global of three words
  jump,    // a relative jump in statements (from this statement)
};

struct OpShape {
  const char* name;  // upper case, as the disassembler prints it ("STOREP_V")
  Operand a, b, c;
};

// The shape of opcode `op` (< kOpCount).
const OpShape& op_shape(uint16_t op);
inline const char* op_name(uint16_t op) { return op < kOpCount ? op_shape(op).name : "BAD"; }

}  // namespace ge::progs
