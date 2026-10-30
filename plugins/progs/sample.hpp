// THE SAMPLE PROGRAM: a small progs built with the assembler that runs every opcode, the tree's
// stand-in for a mod until our compiler (M18d) builds the test mod from QC. The `qc` lab profiles
// it, `ge qc` is tried on it (the lab writes it as previews/qc/sample.dat), and the tests hold its
// run to the bit.
//
// Its functions: `fib(n)` (recursion), `vectors(n)` (a loop over the vector and float arithmetic
// and comparisons), `entities()` (every field load, address and pointer store, the entity and
// function comparisons, STATE), `strings(x)` (the string comparisons and a temporary from a
// builtin), `bits(x)` (the logic and bit operations), `run(n)` (all of them, returning a
// checksum), `think()` (what STATE schedules), and `fault()` (calls the null function: a fault
// with its stack and lines). Its builtins are its own, by number (`install_sample_builtins`):
// 1 `ftos` (a float as text in a temporary), 2 `vlen` (a vector's length), 3 `sin` (through the
// engine's libm), 4 `spawn` (the next entity), 5 `print` (appends to `SampleOutput`); none of
// them is the classic catalogue (M18p).
//
// ge:summary The sample QC program the lab, ge qc and the tests run: every opcode, its builtins
// ge:keywords qc progs sample test-program test-mod fib vectors entities strings bits checksum
// ge:keywords build_sample install_sample_builtins sample-builtins ftos vlen spawn print
// ge:skill progs
// ge:test tests/progs_vm_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "progs/vm.hpp"

namespace ge::progs {

// The sample's bytes (a progs file) and its line table.
std::vector<uint8_t> build_sample(std::vector<int32_t>& lines);
// Loads the sample into `out` (with its line table); false with `error` only if the assembler
// broke.
bool load_sample(Progs& out, std::string& error);

// What the sample's `print` builtin wrote, kept by the caller and pointed at by `vm.user`.
struct SampleOutput {
  std::string text;
  int entities = 1;  // `spawn` hands out 1, 2, ... and raises vm.num_entities
};
// The sample's builtins (numbers 1..5) into the VM's table; `vm.user` must point at a
// SampleOutput for `print` and `spawn`.
void install_sample_builtins(Vm& vm);

}  // namespace ge::progs
