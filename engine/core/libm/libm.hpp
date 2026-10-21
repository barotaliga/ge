// THE DETERMINISTIC LIBM. Every transcendental the simulation calls, returning the same bits
// on every compiler and OS this engine builds on (a platform libm differs in the last bit for
// a few percent of inputs). Simulation code calls these and never <cmath>'s transcendentals
// (tools/check-libm.sh holds that; AGENTS.md "BITS, NOT CLOSENESS").
//
// Where each one comes from:
//   sin cos tan asin acos atan atan2 exp expm1 log log2 cbrt tanh
//       fdlibm, vendored (fdlibm_trig.cpp, fdlibm_arc.cpp, fdlibm_exp_log.cpp).
//   pow glibc's pow, which is Arm's optimized-routines pow with FMA (pow.cpp).
// NaN in gives NaN out; pow's own special cases are stated in pow.cpp. Bits:
// tests/core_libm_test.cpp against tests/golden/libm_*.txt. Licences: engine/core/libm/LICENSE.md.
//
// ge:summary The deterministic libm: every transcendental, the same bits on every platform
// ge:keywords libm transcendental sin cos tan asin acos atan atan2 exp expm1 log log2 cbrt tanh pow
// ge:keywords fdlibm bits
// ge:skill determinism
// ge:test tests/core_libm_test.cpp
#pragma once

namespace ge::core::libm {

double sin(double x);
double cos(double x);
double tan(double x);
double asin(double x);
double acos(double x);
double atan(double x);
// atan2(y, x): note y first.
double atan2(double y, double x);
double exp(double x);
double expm1(double x);
double log(double x);
double log2(double x);
double cbrt(double x);
double tanh(double x);
// x to the power y.
double pow(double x, double y);

}  // namespace ge::core::libm
