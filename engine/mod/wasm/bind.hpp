// NATIVE FUNCTIONS BOUND INTO A MODULE, AND A MODULE'S FUNCTIONS CALLED FROM C++ (docs/mods.md §
// The store form): how a mod's tables cross into the interpreter (interp.hpp). A game's services
// and the host table are native C function pointers (mod/abi.hpp); `bind_native` puts one in the
// module's table and answers the slot, the number a module reads as that function pointer. The
// conversion of each argument is worked out from the C++ type, once, at compile time:
//
//   void*             the host's own context (the module's number for it is ignored)
//   int32_t, uint32_t an i32
//   float, double     an f32, an f64
//   const char*       a NUL-terminated string in the module's memory, copied out (at most 1 KiB)
//   const float*      three floats in the module's memory, copied out before the call
//   float*            three floats written back into the module's memory after the call
//
// so a game binds each member of its services table in one line, and the same table serves the
// native form and the portable one. `call_slot` is the other way: a module's function pointer
// (a table slot) called with C++ arguments, its type checked against theirs.
//
// ge:summary Native functions bound into a WebAssembly module's table by their C++ types; calls out
// ge:keywords wasm bind bind_native call_slot thunk function-pointer table-slot services host-table
// ge:skill mod
// ge:test tests/mod_wasm_mod_test.cpp
#pragma once

#include <bit>
#include <cstdint>
#include <deque>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "mod/wasm/interp.hpp"

namespace ge::mod::wasm {

// A bound native function: its pointer (cast back by the thunk that knows its type) and the
// host's context. Kept in a `Bindings`, which must outlive the instance's calls.
struct Bound {
  void (*fp)() = nullptr;
  void* ctx = nullptr;
};
using Bindings = std::deque<Bound>;

namespace detail {

template <typename T>
constexpr ValType wasm_type() {
  if constexpr (std::is_same_v<T, float>)
    return ValType::f32;
  else if constexpr (std::is_same_v<T, double>)
    return ValType::f64;
  else if constexpr (std::is_same_v<T, int64_t> || std::is_same_v<T, uint64_t>)
    return ValType::i64;
  else
    return ValType::i32;  // int32_t, uint32_t, a pointer
}

template <typename R, typename... A>
FuncType type_of() {
  FuncType t;
  (t.params.push_back(wasm_type<A>()), ...);
  if constexpr (!std::is_void_v<R>) t.results.push_back(wasm_type<R>());
  return t;
}

template <typename T>
uint64_t to_bits(T v) {
  if constexpr (std::is_same_v<T, float>)
    return std::bit_cast<uint32_t>(v);
  else if constexpr (std::is_same_v<T, double>)
    return std::bit_cast<uint64_t>(v);
  else if constexpr (std::is_pointer_v<T>)
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(v));
  else
    return static_cast<uint64_t>(static_cast<std::make_unsigned_t<T>>(v));
}

struct ArgState {
  Instance& in;
  void* ctx;
  const uint64_t* args;
  size_t next = 0;
  bool ok = true;
  uint64_t take() { return args[next++]; }
  void fail(const std::string& why) {
    if (ok) in.trap_message = why;
    ok = false;
  }
};

// One argument, converted from its bits (and, for an out-pointer, written back).
template <typename T>
struct Arg {
  T value{};
  explicit Arg(ArgState& s) {
    const uint64_t b = s.take();
    if constexpr (std::is_same_v<T, float>)
      value = std::bit_cast<float>(static_cast<uint32_t>(b));
    else if constexpr (std::is_same_v<T, double>)
      value = std::bit_cast<double>(b);
    else if constexpr (std::is_same_v<T, int64_t> || std::is_same_v<T, uint64_t>)
      value = static_cast<T>(b);
    else
      value = static_cast<T>(static_cast<uint32_t>(b));
  }
  T get() { return value; }
  void finish(ArgState&) {}
};

template <>
struct Arg<void*> {
  void* value;
  explicit Arg(ArgState& s) : value(s.ctx) { (void)s.take(); }
  void* get() { return value; }
  void finish(ArgState&) {}
};

template <>
struct Arg<const char*> {
  std::string value;
  explicit Arg(ArgState& s) {
    if (!read_string(s.in, static_cast<uint32_t>(s.take()), 1024, value))
      s.fail("a string argument past the module's memory");
  }
  const char* get() { return value.c_str(); }
  void finish(ArgState&) {}
};

template <>
struct Arg<const float*> {
  float value[3] = {0, 0, 0};
  explicit Arg(ArgState& s) {
    if (!read_memory(s.in, static_cast<uint32_t>(s.take()), value, sizeof value))
      s.fail("a vector argument past the module's memory");
  }
  const float* get() { return value; }
  void finish(ArgState&) {}
};

template <>
struct Arg<float*> {
  float value[3] = {0, 0, 0};
  uint32_t at = 0;
  explicit Arg(ArgState& s) : at(static_cast<uint32_t>(s.take())) {
    if (!read_memory(s.in, at, value, sizeof value))
      s.fail("a vector result past the module's memory");
  }
  float* get() { return value; }
  void finish(ArgState& s) {
    if (!write_memory(s.in, at, value, sizeof value))
      s.fail("a vector result past the module's memory");
  }
};

template <typename R, typename... A>
bool thunk(Instance& in, void* user, const uint64_t* args, uint64_t* results) {
  const Bound& b = *static_cast<const Bound*>(user);
  auto* fp = reinterpret_cast<R (*)(A...)>(b.fp);
  ArgState s{in, b.ctx, args};
  // A braced list converts the arguments left to right.
  std::tuple<Arg<A>...> held{Arg<A>(s)...};
  if (!s.ok) return false;
  if constexpr (std::is_void_v<R>) {
    std::apply([&](auto&... a) { fp(a.get()...); }, held);
  } else {
    const R r = std::apply([&](auto&... a) { return fp(a.get()...); }, held);
    results[0] = to_bits(r);
  }
  std::apply([&](auto&... a) { (a.finish(s), ...); }, held);
  return s.ok;
}

}  // namespace detail

// `fp` put in `in`'s table with `ctx` as its context: the slot, which the module reads as a
// function pointer. 0 (the module's null pointer) for a null `fp`; kNullFunc when the table is
// full or the module has none.
template <typename R, typename... A>
uint32_t bind_native(Instance& in, Bindings& keep, R (*fp)(A...), void* ctx) {
  if (fp == nullptr) return 0;
  keep.push_back({reinterpret_cast<void (*)()>(fp), ctx});
  return add_host_function(in, detail::type_of<R, A...>(), &detail::thunk<R, A...>, &keep.back());
}

// The module's function at table slot `slot` called with `args`: Trap::indirect when the slot
// holds no function or one of another type than `R(A...)`. Its result, if any, in `*result`.
template <typename R, typename... A>
Trap call_slot(Instance& in, uint32_t slot, uint64_t fuel, R* result, A... args) {
  if (slot >= in.table.size() || in.table[slot] == kNullFunc) {
    in.trap_message = "a call through table slot " + std::to_string(slot) + ", which is empty";
    return Trap::indirect;
  }
  const uint32_t fn = in.table[slot];
  const Module& m = *in.module;
  if (fn >= m.func_count() || !(m.func_type(fn) == detail::type_of<R, A...>())) {
    in.trap_message = "a call through table slot " + std::to_string(slot) +
                      " to a function of another type than the host expects";
    return Trap::indirect;
  }
  const uint64_t bits[sizeof...(A) + 1] = {detail::to_bits(args)..., 0};
  uint64_t out[1] = {0};
  const Trap t = call(in, fn, bits, out, fuel);
  if constexpr (!std::is_void_v<R>) {
    if (t == Trap::none && result != nullptr) {
      if constexpr (std::is_same_v<R, float>)
        *result = std::bit_cast<float>(static_cast<uint32_t>(out[0]));
      else if constexpr (std::is_same_v<R, double>)
        *result = std::bit_cast<double>(out[0]);
      else
        *result = static_cast<R>(out[0]);
    }
  }
  return t;
}

}  // namespace ge::mod::wasm
