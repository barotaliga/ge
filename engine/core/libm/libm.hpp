// THE DETERMINISTIC LIBM. Every transcendental game5's engine calls, returning the exact bits
// game5's pinned runtime (Node 24, V8 13.6; game5's .nvmrc) returns, on every compiler and OS
// this engine builds on. Simulation code calls these and never <cmath>'s transcendentals
// (tools/check-libm.sh holds that; AGENTS.md "BITS, NOT CLOSENESS").
//
// Where each one comes from (measured: docs/research.md section 2):
//   sin cos tan asin acos atan atan2 exp expm1 log log2 cbrt tanh
//       V8's fdlibm port, src/base/ieee754.cc at V8 13.6 (fdlibm.cpp, fdlibm_trig.cpp).
//       Matches Node 24 on 1,000,000 inputs per function.
//   pow Node 24 runs Math.pow on the HOST libm. game5's reference is Linux x86-64 with glibc,
//       whose pow is Arm's optimized-routines pow with FMA (pow.cpp). Also covers `x ** y`.
// Every function takes and returns JS semantics (NaN in, NaN out; the ECMAScript special
// cases of Math.pow). Parity: tests/core_libm_test.cpp against tests/golden/libm_*.txt.
// Licences: engine/core/libm/LICENSE.md.
#pragma once

namespace ge::core::libm {

double sin(double x);
double cos(double x);
double tan(double x);
double asin(double x);
double acos(double x);
double atan(double x);
// JS Math.atan2(y, x): note y first.
double atan2(double y, double x);
double exp(double x);
double expm1(double x);
double log(double x);
double log2(double x);
double cbrt(double x);
double tanh(double x);
// JS Math.pow(x, y) and x ** y.
double pow(double x, double y);

}  // namespace ge::core::libm
