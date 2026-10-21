// The following is adapted from fdlibm (http://www.netlib.org/fdlibm).
//
// ====================================================
// Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
//
// Developed at SunSoft, a Sun Microsystems, Inc. business.
// Permission to use, copy, modify, and distribute this
// software is freely granted, provided that this notice
// is preserved.
// ====================================================
//
// The original source code covered by the above license above has been
// modified significantly by Google Inc.
// Copyright 2016 the V8 project authors. All rights reserved.
//
// Internal to engine/core/libm: the word-access macros and helpers V8's fdlibm port
// (src/base/ieee754.cc, V8 13.6) is written against, so the vendored .cpp files stay line for
// line with upstream. Nothing outside engine/core/libm/*.cpp includes this.
#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

// NOLINTBEGIN
namespace ge::core::libm {

template <class To, class From>
inline To bit_cast(From from) {
  static_assert(sizeof(To) == sizeof(From));
  To to;
  std::memcpy(&to, &from, sizeof to);
  return to;
}

// V8's base::Divide: x / y without the UB of a divide by zero, IEEE 754 results.
template <class T>
inline T Divide(T x, T y) {
  if (y != 0) return x / y;
  if (x == 0 || x != x) return std::numeric_limits<T>::quiet_NaN();
  if ((x >= 0) == (std::signbit(y) == 0)) return std::numeric_limits<T>::infinity();
  return -std::numeric_limits<T>::infinity();
}

// V8's overflowing-math helpers: two's complement wraparound without signed-overflow UB.
template <class T>
inline T NegateWithWraparound(T a) {
  using U = std::make_unsigned_t<T>;
  return static_cast<T>(U(0) - static_cast<U>(a));
}
template <class T>
inline T SubWithWraparound(T a, T b) {
  using U = std::make_unsigned_t<T>;
  return static_cast<T>(static_cast<U>(a) - static_cast<U>(b));
}

}  // namespace ge::core::libm

// clang-format off
/*
 * The original fdlibm code used statements like:
 *  n0 = ((*(int*)&one)>>29)^1;   * index of high word *
 *  ix0 = *(n0+(int*)&x);     * high word of x *
 *  ix1 = *((1-n0)+(int*)&x);   * low word of x *
 * to dig two 32 bit words out of the 64 bit IEEE floating point
 * value.  That is non-ANSI, and, moreover, the gcc instruction
 * scheduler gets it wrong.  We instead use the following macros.
 * Unlike the original code, we determine the endianness at compile
 * time, not at run time; I don't see much benefit to selecting
 * endianness at run time.
 */

/* Get two 32 bit ints from a double.  */

#define EXTRACT_WORDS(ix0, ix1, d)               \
  do {                                           \
    uint64_t bits = ge::core::libm::bit_cast<uint64_t>(d); \
    (ix0) = bits >> 32;                          \
    (ix1) = bits & 0xFFFFFFFFu;                  \
  } while (false)

/* Get the more significant 32 bit int from a double.  */

#define GET_HIGH_WORD(i, d)                      \
  do {                                           \
    uint64_t bits = ge::core::libm::bit_cast<uint64_t>(d); \
    (i) = bits >> 32;                            \
  } while (false)

/* Get the less significant 32 bit int from a double.  */

#define GET_LOW_WORD(i, d)                       \
  do {                                           \
    uint64_t bits = ge::core::libm::bit_cast<uint64_t>(d); \
    (i) = bits & 0xFFFFFFFFu;                    \
  } while (false)

/* Set a double from two 32 bit ints.  */

#define INSERT_WORDS(d, ix0, ix1)             \
  do {                                        \
    uint64_t bits = 0;                        \
    bits |= static_cast<uint64_t>(ix0) << 32; \
    bits |= static_cast<uint32_t>(ix1);       \
    (d) = ge::core::libm::bit_cast<double>(bits);       \
  } while (false)

/* Set the more significant 32 bits of a double from an int.  */

#define SET_HIGH_WORD(d, v)                      \
  do {                                           \
    uint64_t bits = ge::core::libm::bit_cast<uint64_t>(d); \
    bits &= 0x0000'0000'FFFF'FFFF;               \
    bits |= static_cast<uint64_t>(v) << 32;      \
    (d) = ge::core::libm::bit_cast<double>(bits);          \
  } while (false)

/* Set the less significant 32 bits of a double from an int.  */

#define SET_LOW_WORD(d, v)                       \
  do {                                           \
    uint64_t bits = ge::core::libm::bit_cast<uint64_t>(d); \
    bits &= 0xFFFF'FFFF'0000'0000;               \
    bits |= static_cast<uint32_t>(v);            \
    (d) = ge::core::libm::bit_cast<double>(bits);          \
  } while (false)
// clang-format on
// NOLINTEND
