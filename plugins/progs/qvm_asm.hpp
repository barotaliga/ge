// A COMPILED MODULE WRITTEN BY HAND: the tests and the qvm lab build modules for the VM (qvm.hpp)
// instruction by instruction, with forward branches patched once their target is known, and
// write the file a server would load (the version-1 header, the code, the data words, the lit
// bytes, the bss length). `qvm_sample` is the lab's module: fib by recursion, a loop that sums
// through a system call, and a division whose divisor the caller picks.
//
// ge:summary A compiled module written by hand (the tests' and the qvm lab's), and the sample
// ge:keywords qvm assembler module-builder qvm_asm qvm_sample fib patch hand-assembled
// ge:skill progs
// ge:test tests/progs_qvm_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "progs/qvm.hpp"

namespace ge::progs {

struct QvmAsm {
  std::vector<uint8_t> code;
  int32_t count = 0;
  std::vector<int32_t> data;  // words at address 0
  std::string lit;            // bytes after the data
  int32_t bss = 0;
  // The index the next instruction gets.
  int32_t here() const { return count; }
  QvmAsm& op(QvmOp o);
  QvmAsm& op(QvmOp o, int32_t v);  // ARG keeps the low byte
  // Sets the 4-byte operand of instruction `i` (a branch written before its target).
  void patch(int32_t i, int32_t v);
  std::vector<uint8_t> file() const;
};

// The sample's exports: command 0 is fib(a0); 1 sums a0 numbers through system call 0 (which the
// host answers with its argument times ten); 2 is a0 / a1 (a1 = 0 faults).
enum : int32_t { kQvmSampleFib = 0, kQvmSampleSum = 1, kQvmSampleDiv = 2 };
std::vector<uint8_t> qvm_sample();

}  // namespace ge::progs
