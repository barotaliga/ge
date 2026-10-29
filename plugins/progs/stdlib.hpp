// THE STANDARD BUILTINS THAT TOUCH ONLY THE VM: math, vectors, number text, strings, the program's
// word list and the seeded draws. Any game that runs QC offers them under the same numbers, so
// they live here; what reaches a world (entities, traces, messages, settings) is the game's.
// Each is a `Builtin` (vm.hpp) a game puts in its table (`set_builtin`, or an extension's row of
// its catalogue); `kStdlib` lists them by number.
//
// THE RULES (each the classic server's behaviour, with **ours** where it was unsafe):
//   normalize, vlen   the squares summed in float ((x x + y y) + z z), the root in double rounded
//                     to float, then each component times 1 / length; zero stays zero
//   vectoyaw          0 when x and y are 0; else atan2(y, x) in degrees in double, plus 360 when
//                     negative, rounded to float
//   vectoangles       (pitch, yaw, 0): x and y zero give yaw 0 and pitch 90 (z > 0) or 270; else
//                     the yaw as vectoyaw and the pitch atan2(z, the horizontal length) in degrees
//                     (the length in double), plus 360 when negative
//   random            k / 32767 with k uniform in 0..32767 from `vm.rng` (the game's seeded
//                     generator; 0 and 1 both possible); randomvec: k x (2 / 32767) - 1 per axis,
//                     drawn again until the squared length is below 1
//   rint              round half away from zero (the half added in double) through a 32-bit
//                     integer (out of range: -2147483648, as the VM's truncation); floor, ceil,
//                     fabs
//   sin, cos, sqrt, pow   in double through core/libm (sqrt exact), rounded to float
//   min, max          of two or more arguments (fewer: a fault); bound(lo, v, hi) = max(lo, min(v,
//   hi)) ftos, vtos, stof  text.hpp's rules; the text in the builtins' ring of 64 strlen, strcat
//   (2047 bytes at most), substr (start and length truncated; empty when start is
//                     at or past the end, the length is 0 or the string empty; **ours**: a negative
//                     start is 0 and a negative length runs to the end), strzone (the size argument
//                     ignored beyond the copy), strunzone (a string that is not a zone one faults;
//                     freeing twice is allowed)
//   tokenize, argc, argv   text.hpp's words in `vm.words` (argv out of range: empty)
//   precache_file     returns its argument
//
// ge:summary The standard builtins over the VM alone: math, vectors, text, strings, words, random
// ge:keywords qc progs builtins stdlib normalize vlen vectoyaw vectoangles random randomvec rint
// ge:keywords floor ceil fabs sin cos sqrt pow min max bound ftos vtos stof strlen strcat substr
// ge:keywords strzone strunzone tokenize argc argv precache_file arg_string arg_float kstdlib
// ge:skill progs
// ge:test tests/progs_stdlib_test.cpp
#pragma once

#include "progs/vm.hpp"

namespace ge::progs {

// Argument readers for any builtin: parameter `i` as a float, a vector, an entity value or text.
// `arg_string` faults (and returns "") when the value names no string.
float arg_float(const Vm& vm, int i);
void arg_vector(const Vm& vm, int i, float out[3]);
int32_t arg_int(const Vm& vm, int i);
const char* arg_string(Vm& vm, int i);
// The return slot.
void return_float(Vm& vm, float v);
void return_vector(Vm& vm, float x, float y, float z);
void return_int(Vm& vm, int32_t v);
// The arguments from `first` on joined into `out` (at most cap - 1 bytes): bprint, sprint, strcat.
void join_args(Vm& vm, int first, char* out, int cap);

struct StdBuiltin {
  int number;
  const char* name;
  Builtin fn;
};
// Every builtin of this file by its standard number.
extern const StdBuiltin kStdlib[];
extern const int kStdlibCount;
// The one called `name`, or nullptr.
const StdBuiltin* stdlib_builtin(const char* name);

}  // namespace ge::progs
