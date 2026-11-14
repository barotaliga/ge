// The WebAssembly interpreter's instances and its run loop (interp.hpp).
//
// ge:summary The WebAssembly interpreter's run loop: every instruction, calls, traps, fuel
// ge:keywords wasm interpreter run-loop instantiate call trap fuel canonical-nan trunc nearest
// ge:skill mod
// ge:test tests/mod_wasm_test.cpp
#include "mod/wasm/interp.hpp"

#include <bit>
#include <cmath>
#include <cstring>
#include <limits>

namespace ge::mod::wasm {

static_assert(std::endian::native == std::endian::little,
              "the interpreter keeps the module's memory in the host's byte order");

const char* trap_name(Trap t) {
  switch (t) {
    case Trap::none:
      return "none";
    case Trap::unreachable:
      return "unreachable";
    case Trap::memory:
      return "memory out of bounds";
    case Trap::divide_by_zero:
      return "integer divide by zero";
    case Trap::overflow:
      return "integer overflow";
    case Trap::conversion:
      return "invalid conversion to integer";
    case Trap::indirect:
      return "bad indirect call";
    case Trap::stack:
      return "stack exhausted";
    case Trap::fuel:
      return "out of fuel";
    case Trap::host:
      return "stopped by the host";
  }
  return "?";
}

uint32_t Instance::host_index(size_t k) const {
  const size_t imports = module->imports.size();
  return k < imports ? static_cast<uint32_t>(k)
                     : module->func_count() + static_cast<uint32_t>(k - imports);
}

bool read_memory(const Instance& in, uint32_t at, void* out, uint32_t n) {
  if (static_cast<uint64_t>(at) + n > in.memory.size()) return false;
  if (n > 0) std::memcpy(out, in.memory.data() + at, n);
  return true;
}

bool write_memory(Instance& in, uint32_t at, const void* src, uint32_t n) {
  if (static_cast<uint64_t>(at) + n > in.memory.size()) return false;
  if (n > 0) std::memcpy(in.memory.data() + at, src, n);
  return true;
}

bool read_string(const Instance& in, uint32_t at, uint32_t max, std::string& out) {
  out.clear();
  for (uint32_t k = 0; k < max; k++) {
    if (static_cast<uint64_t>(at) + k >= in.memory.size()) return false;
    const char c = static_cast<char>(in.memory[at + k]);
    if (c == '\0') return true;
    out.push_back(c);
  }
  return true;  // cut at `max`
}

namespace {

// Floats from and to their bits; every NaN an operation makes is the canonical one.
inline float f32(uint64_t v) { return std::bit_cast<float>(static_cast<uint32_t>(v)); }
inline double f64(uint64_t v) { return std::bit_cast<double>(v); }
inline uint64_t b32(float x) {
  return x != x ? uint64_t{0x7fc00000u} : uint64_t{std::bit_cast<uint32_t>(x)};
}
inline uint64_t b64(double x) {
  return x != x ? 0x7ff8000000000000ull : std::bit_cast<uint64_t>(x);
}
inline uint32_t u32(uint64_t v) { return static_cast<uint32_t>(v); }
inline int32_t s32(uint64_t v) { return static_cast<int32_t>(static_cast<uint32_t>(v)); }
inline uint64_t w32(uint32_t v) { return v; }
inline uint64_t w32s(int32_t v) { return static_cast<uint32_t>(v); }

template <typename F>
F wmin(F a, F b) {
  if (a != a || b != b) return std::numeric_limits<F>::quiet_NaN();
  if (a == 0 && b == 0) return std::signbit(a) ? a : b;
  return a < b ? a : b;
}
template <typename F>
F wmax(F a, F b) {
  if (a != a || b != b) return std::numeric_limits<F>::quiet_NaN();
  if (a == 0 && b == 0) return std::signbit(a) ? b : a;
  return a > b ? a : b;
}
// Round to the nearest integer, halves to even.
template <typename F>
F nearest(F x) {
  if (x != x || std::isinf(x)) return x;
  const F t = std::trunc(x);
  const F d = x - t;  // exact: |d| < 1
  F r = t;
  if (d > F(0.5) || (d == F(0.5) && std::fmod(t, F(2)) != 0))
    r = t + F(1);
  else if (d < F(-0.5) || (d == F(-0.5) && std::fmod(t, F(2)) != 0))
    r = t - F(1);
  return r == 0 ? std::copysign(F(0), x) : r;
}

// An unsigned 64-bit integer to a float, rounded once (half to even) on every compiler.
template <typename F>
F from_u64(uint64_t v) {
  if (v < (uint64_t{1} << 63)) return static_cast<F>(static_cast<int64_t>(v));
  const uint64_t half = (v >> 1) | (v & 1);  // the low bit kept as a sticky bit
  const F h = static_cast<F>(static_cast<int64_t>(half));
  return h + h;
}

// A float to an integer type, trapping (`sat` false) or saturating; `lo` and `hi` the exclusive
// bounds the truncated value must sit between.
template <typename I, typename F>
bool trunc_to(F x, double lo, double hi, bool sat, I& out, Trap& trap) {
  if (x != x) {
    if (sat) return out = 0, true;
    trap = Trap::conversion;
    return false;
  }
  const double d = static_cast<double>(x);
  if (!(d > lo)) {
    if (sat) return out = std::numeric_limits<I>::min(), true;
    trap = Trap::overflow;
    return false;
  }
  if (!(d < hi)) {
    if (sat) return out = std::numeric_limits<I>::max(), true;
    trap = Trap::overflow;
    return false;
  }
  out = static_cast<I>(std::trunc(x));
  return true;
}

constexpr double k2p31 = 2147483648.0, k2p32 = 4294967296.0;
constexpr double k2p63 = 9223372036854775808.0, k2p64 = 18446744073709551616.0;

// The function `fn` run with its arguments at stack[base ...); its results left there.
Trap run(Instance& in, uint32_t fn, size_t base);

Trap invoke(Instance& in, uint32_t fn, size_t base) {
  const Module& m = *in.module;
  const uint32_t imports = static_cast<uint32_t>(m.imports.size());
  if (fn >= imports && fn < m.func_count()) return run(in, fn, base);
  const size_t k = fn < imports ? fn : imports + (fn - m.func_count());
  if (k >= in.host.size()) {
    in.trap_message = "a call to function " + std::to_string(fn) + ", which does not exist";
    return Trap::indirect;
  }
  const HostSlot& h = in.host[k];
  uint64_t results[8] = {};
  if (h.type.results.size() > 8) {
    in.trap_message = "a host function with more than 8 results";
    return Trap::host;
  }
  if (base + 8 > in.stack.size()) return Trap::stack;
  if (!h.fn(in, h.user, in.stack.data() + base, results)) {
    if (in.trap_message.empty()) in.trap_message = "a host function stopped the call";
    return Trap::host;
  }
  for (size_t r = 0; r < h.type.results.size(); r++) in.stack[base + r] = results[r];
  return Trap::none;
}

#define TRAP(kind, what)                                                           \
  do {                                                                             \
    in.fuel = fuel;                                                                \
    if (in.trap_message.empty())                                                   \
      in.trap_message = std::string(what) + " in function " + std::to_string(fn) + \
                        " at instruction " + std::to_string(pc - 1);               \
    in.depth--;                                                                    \
    return kind;                                                                   \
  } while (0)

#define MEM(size)                                                    \
  const uint64_t ea = static_cast<uint64_t>(u32(S[sp - 1])) + ins.a; \
  if (ea + (size) > mem_size) TRAP(Trap::memory, "a memory access past the end")

Trap run(Instance& in, uint32_t fn, size_t base) {
  const Module& m = *in.module;
  const Func& f = m.funcs[fn - m.imports.size()];
  const FuncType& type = m.types[f.type];
  if (in.depth >= kMaxDepth || base + f.locals.size() + f.max_height + 8 > in.stack.size()) {
    in.trap_message = "the calls nest past " + std::to_string(kMaxDepth) +
                      " or their values pass the stack, entering function " + std::to_string(fn);
    return Trap::stack;
  }
  in.depth++;
  uint64_t* S = in.stack.data();
  for (size_t k = type.params.size(); k < f.locals.size(); k++) S[base + k] = 0;
  const size_t opbase = base + f.locals.size();
  size_t sp = opbase;
  uint32_t pc = 0;
  uint64_t fuel = in.fuel;
  const Inst* code = f.code.data();
  uint8_t* mem = in.memory.data();
  uint64_t mem_size = in.memory.size();
  for (;;) {
    if (fuel == 0) TRAP(Trap::fuel, "the instruction budget spent");
    fuel--;
    const Inst& ins = code[pc++];
    switch (ins.op) {
      case 0x00:
        TRAP(Trap::unreachable, "the unreachable instruction");
      case kOpJump:
        pc = ins.a;
        break;
      case kOpJumpIfFalse:
        if (u32(S[--sp]) == 0) pc = ins.a;
        break;
      case kOpBrIf:
        if (u32(S[--sp]) == 0) break;
        [[fallthrough]];
      case kOpBr: {
        const size_t keep = ins.b >> 32, dst = opbase + (ins.b & 0xffffffffu);
        if (dst + keep != sp) std::memmove(S + dst, S + sp - keep, keep * sizeof(uint64_t));
        sp = dst + keep;
        pc = ins.a;
        break;
      }
      case kOpBrTable: {
        const uint32_t i = u32(S[--sp]);
        const Target& t = f.table[ins.a + (i < ins.b ? i : ins.b)];
        const size_t dst = opbase + t.height;
        if (dst + t.keep != sp) std::memmove(S + dst, S + sp - t.keep, t.keep * sizeof(uint64_t));
        sp = dst + t.keep;
        pc = t.pc;
        break;
      }
      case kOpReturn: {
        if (ins.a > 0) std::memmove(S + base, S + sp - ins.a, ins.a * sizeof(uint64_t));
        in.fuel = fuel;
        in.depth--;
        return Trap::none;
      }
      case 0x10:
      case 0x11: {
        uint32_t callee = ins.a;
        if (ins.op == 0x11) {
          const uint32_t i = u32(S[--sp]);
          if (i >= in.table.size() || in.table[i] == kNullFunc)
            TRAP(Trap::indirect,
                 "a call through table slot " + std::to_string(i) + ", which holds no function");
          callee = in.table[i];
          const uint32_t imports = static_cast<uint32_t>(m.imports.size());
          const FuncType* have = nullptr;
          if (callee >= imports && callee < m.func_count())
            have = &m.func_type(callee);
          else if (const size_t k = callee < imports ? callee : imports + (callee - m.func_count());
                   k < in.host.size())
            have = &in.host[k].type;
          if (have == nullptr || !(*have == m.types[ins.a]))
            TRAP(Trap::indirect, "a call through table slot " + std::to_string(i) +
                                     " to a function of another type");
        }
        const FuncType& ct = ins.op == 0x11 ? m.types[ins.a] : m.func_type(callee);
        const size_t cbase = sp - ct.params.size();
        in.fuel = fuel;
        const Trap t = invoke(in, callee, cbase);
        fuel = in.fuel;
        if (t != Trap::none) {
          in.depth--;
          return t;
        }
        sp = cbase + ct.results.size();
        mem = in.memory.data();  // a host function may grow the memory
        mem_size = in.memory.size();
        break;
      }
      case 0x1a:
        sp--;
        break;
      case 0x1b: {
        const uint32_t c = u32(S[--sp]);
        sp--;
        if (c == 0) S[sp - 1] = S[sp];
        break;
      }
      case 0x20:
        S[sp++] = S[base + ins.a];
        break;
      case 0x21:
        S[base + ins.a] = S[--sp];
        break;
      case 0x22:
        S[base + ins.a] = S[sp - 1];
        break;
      case 0x23:
        S[sp++] = in.globals[ins.a];
        break;
      case 0x24:
        in.globals[ins.a] = S[--sp];
        break;

      // Loads: the address on top, replaced by the value.
      case 0x28: {
        MEM(4);
        uint32_t v;
        std::memcpy(&v, mem + ea, 4);
        S[sp - 1] = v;
        break;
      }
      case 0x29: {
        MEM(8);
        uint64_t v;
        std::memcpy(&v, mem + ea, 8);
        S[sp - 1] = v;
        break;
      }
      case 0x2a: {
        MEM(4);
        uint32_t v;
        std::memcpy(&v, mem + ea, 4);
        S[sp - 1] = v;
        break;
      }
      case 0x2b: {
        MEM(8);
        uint64_t v;
        std::memcpy(&v, mem + ea, 8);
        S[sp - 1] = v;
        break;
      }
      case 0x2c: {
        MEM(1);
        S[sp - 1] = w32s(static_cast<int8_t>(mem[ea]));
        break;
      }
      case 0x2d: {
        MEM(1);
        S[sp - 1] = mem[ea];
        break;
      }
      case 0x2e: {
        MEM(2);
        int16_t v;
        std::memcpy(&v, mem + ea, 2);
        S[sp - 1] = w32s(v);
        break;
      }
      case 0x2f: {
        MEM(2);
        uint16_t v;
        std::memcpy(&v, mem + ea, 2);
        S[sp - 1] = v;
        break;
      }
      case 0x30: {
        MEM(1);
        S[sp - 1] = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int8_t>(mem[ea])));
        break;
      }
      case 0x31: {
        MEM(1);
        S[sp - 1] = mem[ea];
        break;
      }
      case 0x32: {
        MEM(2);
        int16_t v;
        std::memcpy(&v, mem + ea, 2);
        S[sp - 1] = static_cast<uint64_t>(static_cast<int64_t>(v));
        break;
      }
      case 0x33: {
        MEM(2);
        uint16_t v;
        std::memcpy(&v, mem + ea, 2);
        S[sp - 1] = v;
        break;
      }
      case 0x34: {
        MEM(4);
        int32_t v;
        std::memcpy(&v, mem + ea, 4);
        S[sp - 1] = static_cast<uint64_t>(static_cast<int64_t>(v));
        break;
      }
      case 0x35: {
        MEM(4);
        uint32_t v;
        std::memcpy(&v, mem + ea, 4);
        S[sp - 1] = v;
        break;
      }

      // Stores: the address under the value.
      case 0x36:
      case 0x38:
      case 0x3e: {
        const uint32_t v = u32(S[--sp]);
        MEM(4);
        std::memcpy(mem + ea, &v, 4);
        sp--;
        break;
      }
      case 0x37:
      case 0x39: {
        const uint64_t v = S[--sp];
        MEM(8);
        std::memcpy(mem + ea, &v, 8);
        sp--;
        break;
      }
      case 0x3a:
      case 0x3c: {
        const uint8_t v = static_cast<uint8_t>(S[--sp]);
        MEM(1);
        mem[ea] = v;
        sp--;
        break;
      }
      case 0x3b:
      case 0x3d: {
        const uint16_t v = static_cast<uint16_t>(S[--sp]);
        MEM(2);
        std::memcpy(mem + ea, &v, 2);
        sp--;
        break;
      }
      case 0x3f:
        S[sp++] = mem_size / kPageBytes;
        break;
      case 0x40: {
        const uint32_t pages = static_cast<uint32_t>(mem_size / kPageBytes);
        const uint64_t want = static_cast<uint64_t>(pages) + u32(S[sp - 1]);
        if (want > in.memory_max) {
          S[sp - 1] = w32s(-1);
        } else {
          in.memory.resize(want * kPageBytes, 0);
          mem = in.memory.data();
          mem_size = in.memory.size();
          S[sp - 1] = pages;
        }
        break;
      }
      case 0x41:
      case 0x42:
      case 0x43:
      case 0x44:
        S[sp++] = ins.b;
        break;

#define CMP32(op, expr)                \
  case op: {                           \
    const uint32_t b = u32(S[--sp]);   \
    const uint32_t a = u32(S[sp - 1]); \
    (void)a, (void)b;                  \
    S[sp - 1] = (expr) ? 1 : 0;        \
    break;                             \
  }
#define CMP64(op, expr)           \
  case op: {                      \
    const uint64_t b = S[--sp];   \
    const uint64_t a = S[sp - 1]; \
    S[sp - 1] = (expr) ? 1 : 0;   \
    break;                        \
  }
#define CMPF32(op, expr)            \
  case op: {                        \
    const float b = f32(S[--sp]);   \
    const float a = f32(S[sp - 1]); \
    S[sp - 1] = (expr) ? 1 : 0;     \
    break;                          \
  }
#define CMPF64(op, expr)             \
  case op: {                         \
    const double b = f64(S[--sp]);   \
    const double a = f64(S[sp - 1]); \
    S[sp - 1] = (expr) ? 1 : 0;      \
    break;                           \
  }
      case 0x45:
        S[sp - 1] = u32(S[sp - 1]) == 0 ? 1 : 0;
        break;
        CMP32(0x46, a == b)
        CMP32(0x47, a != b)
        CMP32(0x48, static_cast<int32_t>(a) < static_cast<int32_t>(b))
        CMP32(0x49, a < b)
        CMP32(0x4a, static_cast<int32_t>(a) > static_cast<int32_t>(b))
        CMP32(0x4b, a > b)
        CMP32(0x4c, static_cast<int32_t>(a) <= static_cast<int32_t>(b))
        CMP32(0x4d, a <= b)
        CMP32(0x4e, static_cast<int32_t>(a) >= static_cast<int32_t>(b))
        CMP32(0x4f, a >= b)
      case 0x50:
        S[sp - 1] = S[sp - 1] == 0 ? 1 : 0;
        break;
        CMP64(0x51, a == b)
        CMP64(0x52, a != b)
        CMP64(0x53, static_cast<int64_t>(a) < static_cast<int64_t>(b))
        CMP64(0x54, a < b)
        CMP64(0x55, static_cast<int64_t>(a) > static_cast<int64_t>(b))
        CMP64(0x56, a > b)
        CMP64(0x57, static_cast<int64_t>(a) <= static_cast<int64_t>(b))
        CMP64(0x58, a <= b)
        CMP64(0x59, static_cast<int64_t>(a) >= static_cast<int64_t>(b))
        CMP64(0x5a, a >= b)
        CMPF32(0x5b, a == b)
        CMPF32(0x5c, a != b)
        CMPF32(0x5d, a < b)
        CMPF32(0x5e, a > b)
        CMPF32(0x5f, a <= b)
        CMPF32(0x60, a >= b)
        CMPF64(0x61, a == b)
        CMPF64(0x62, a != b)
        CMPF64(0x63, a < b)
        CMPF64(0x64, a > b)
        CMPF64(0x65, a <= b)
        CMPF64(0x66, a >= b)

      // i32 arithmetic.
      case 0x67:
        S[sp - 1] = static_cast<uint32_t>(std::countl_zero(u32(S[sp - 1])));
        break;
      case 0x68:
        S[sp - 1] = static_cast<uint32_t>(std::countr_zero(u32(S[sp - 1])));
        break;
      case 0x69:
        S[sp - 1] = static_cast<uint32_t>(std::popcount(u32(S[sp - 1])));
        break;
#define BIN32(op, expr)                \
  case op: {                           \
    const uint32_t b = u32(S[--sp]);   \
    const uint32_t a = u32(S[sp - 1]); \
    S[sp - 1] = w32(expr);             \
    break;                             \
  }
        BIN32(0x6a, a + b)
        BIN32(0x6b, a - b)
        BIN32(0x6c, a * b)
      case 0x6d:
      case 0x6f: {
        const int32_t b = s32(S[--sp]);
        const int32_t a = s32(S[sp - 1]);
        if (b == 0) TRAP(Trap::divide_by_zero, "an i32 division by zero");
        if (a == std::numeric_limits<int32_t>::min() && b == -1) {
          if (ins.op == 0x6d) TRAP(Trap::overflow, "an i32 division that overflows");
          S[sp - 1] = 0;
        } else {
          S[sp - 1] = w32s(ins.op == 0x6d ? a / b : a % b);
        }
        break;
      }
      case 0x6e:
      case 0x70: {
        const uint32_t b = u32(S[--sp]);
        const uint32_t a = u32(S[sp - 1]);
        if (b == 0) TRAP(Trap::divide_by_zero, "an i32 division by zero");
        S[sp - 1] = ins.op == 0x6e ? a / b : a % b;
        break;
      }
        BIN32(0x71, a & b)
        BIN32(0x72, a | b)
        BIN32(0x73, a ^ b)
        BIN32(0x74, a << (b & 31))
        BIN32(0x75, static_cast<uint32_t>(static_cast<int32_t>(a) >> (b & 31)))
        BIN32(0x76, a >> (b & 31))
        BIN32(0x77, std::rotl(a, static_cast<int>(b & 31)))
        BIN32(0x78, std::rotr(a, static_cast<int>(b & 31)))

      // i64 arithmetic.
      case 0x79:
        S[sp - 1] = static_cast<uint64_t>(std::countl_zero(S[sp - 1]));
        break;
      case 0x7a:
        S[sp - 1] = static_cast<uint64_t>(std::countr_zero(S[sp - 1]));
        break;
      case 0x7b:
        S[sp - 1] = static_cast<uint64_t>(std::popcount(S[sp - 1]));
        break;
#define BIN64(op, expr)           \
  case op: {                      \
    const uint64_t b = S[--sp];   \
    const uint64_t a = S[sp - 1]; \
    S[sp - 1] = (expr);           \
    break;                        \
  }
        BIN64(0x7c, a + b)
        BIN64(0x7d, a - b)
        BIN64(0x7e, a * b)
      case 0x7f:
      case 0x81: {
        const int64_t b = static_cast<int64_t>(S[--sp]);
        const int64_t a = static_cast<int64_t>(S[sp - 1]);
        if (b == 0) TRAP(Trap::divide_by_zero, "an i64 division by zero");
        if (a == std::numeric_limits<int64_t>::min() && b == -1) {
          if (ins.op == 0x7f) TRAP(Trap::overflow, "an i64 division that overflows");
          S[sp - 1] = 0;
        } else {
          S[sp - 1] = static_cast<uint64_t>(ins.op == 0x7f ? a / b : a % b);
        }
        break;
      }
      case 0x80:
      case 0x82: {
        const uint64_t b = S[--sp];
        const uint64_t a = S[sp - 1];
        if (b == 0) TRAP(Trap::divide_by_zero, "an i64 division by zero");
        S[sp - 1] = ins.op == 0x80 ? a / b : a % b;
        break;
      }
        BIN64(0x83, a & b)
        BIN64(0x84, a | b)
        BIN64(0x85, a ^ b)
        BIN64(0x86, a << (b & 63))
        BIN64(0x87, static_cast<uint64_t>(static_cast<int64_t>(a) >> (b & 63)))
        BIN64(0x88, a >> (b & 63))
        BIN64(0x89, std::rotl(a, static_cast<int>(b & 63)))
        BIN64(0x8a, std::rotr(a, static_cast<int>(b & 63)))

      // f32 arithmetic: abs, neg and copysign touch only the sign bit.
      case 0x8b:
        S[sp - 1] = u32(S[sp - 1]) & 0x7fffffffu;
        break;
      case 0x8c:
        S[sp - 1] = u32(S[sp - 1]) ^ 0x80000000u;
        break;
      case 0x8d:
        S[sp - 1] = b32(std::ceil(f32(S[sp - 1])));
        break;
      case 0x8e:
        S[sp - 1] = b32(std::floor(f32(S[sp - 1])));
        break;
      case 0x8f:
        S[sp - 1] = b32(std::trunc(f32(S[sp - 1])));
        break;
      case 0x90:
        S[sp - 1] = b32(nearest(f32(S[sp - 1])));
        break;
      case 0x91:
        S[sp - 1] = b32(std::sqrt(f32(S[sp - 1])));
        break;
#define BINF32(op, expr)            \
  case op: {                        \
    const float b = f32(S[--sp]);   \
    const float a = f32(S[sp - 1]); \
    S[sp - 1] = b32(expr);          \
    break;                          \
  }
        BINF32(0x92, a + b)
        BINF32(0x93, a - b)
        BINF32(0x94, a * b)
        BINF32(0x95, a / b)
        BINF32(0x96, wmin(a, b))
        BINF32(0x97, wmax(a, b))
      case 0x98: {
        const uint32_t b = u32(S[--sp]);
        S[sp - 1] = (u32(S[sp - 1]) & 0x7fffffffu) | (b & 0x80000000u);
        break;
      }

      // f64 arithmetic.
      case 0x99:
        S[sp - 1] &= 0x7fffffffffffffffull;
        break;
      case 0x9a:
        S[sp - 1] ^= 0x8000000000000000ull;
        break;
      case 0x9b:
        S[sp - 1] = b64(std::ceil(f64(S[sp - 1])));
        break;
      case 0x9c:
        S[sp - 1] = b64(std::floor(f64(S[sp - 1])));
        break;
      case 0x9d:
        S[sp - 1] = b64(std::trunc(f64(S[sp - 1])));
        break;
      case 0x9e:
        S[sp - 1] = b64(nearest(f64(S[sp - 1])));
        break;
      case 0x9f:
        S[sp - 1] = b64(std::sqrt(f64(S[sp - 1])));
        break;
#define BINF64(op, expr)             \
  case op: {                         \
    const double b = f64(S[--sp]);   \
    const double a = f64(S[sp - 1]); \
    S[sp - 1] = b64(expr);           \
    break;                           \
  }
        BINF64(0xa0, a + b)
        BINF64(0xa1, a - b)
        BINF64(0xa2, a * b)
        BINF64(0xa3, a / b)
        BINF64(0xa4, wmin(a, b))
        BINF64(0xa5, wmax(a, b))
      case 0xa6: {
        const uint64_t b = S[--sp];
        S[sp - 1] = (S[sp - 1] & 0x7fffffffffffffffull) | (b & 0x8000000000000000ull);
        break;
      }

      // Conversions.
      case 0xa7:
        S[sp - 1] = u32(S[sp - 1]);
        break;
#define TRUNC(op, I, F, from, lo, hi, sat, store)                         \
  case op: {                                                              \
    I v{};                                                                \
    Trap t = Trap::none;                                                  \
    if (!trunc_to<I>(from(S[sp - 1]), lo, hi, sat, v, t))                 \
      TRAP(t, t == Trap::conversion ? "a NaN converted to an integer"     \
                                    : "a float too big for its integer"); \
    S[sp - 1] = store;                                                    \
    break;                                                                \
  }
        TRUNC(0xa8, int32_t, float, f32, -k2p31 - 1, k2p31, false, w32s(v))
        TRUNC(0xa9, uint32_t, float, f32, -1.0, k2p32, false, w32(v))
        TRUNC(0xaa, int32_t, double, f64, -k2p31 - 1, k2p31, false, w32s(v))
        TRUNC(0xab, uint32_t, double, f64, -1.0, k2p32, false, w32(v))
      case 0xac:
        S[sp - 1] = static_cast<uint64_t>(static_cast<int64_t>(s32(S[sp - 1])));
        break;
      case 0xad:
        S[sp - 1] = u32(S[sp - 1]);
        break;
        TRUNC(0xae, int64_t, float, f32, -k2p63 * (1 + 0x1p-52), k2p63, false,
              static_cast<uint64_t>(v))
        TRUNC(0xaf, uint64_t, float, f32, -1.0, k2p64, false, v)
        TRUNC(0xb0, int64_t, double, f64, -k2p63 * (1 + 0x1p-52), k2p63, false,
              static_cast<uint64_t>(v))
        TRUNC(0xb1, uint64_t, double, f64, -1.0, k2p64, false, v)
      case 0xb2:
        S[sp - 1] = b32(static_cast<float>(s32(S[sp - 1])));
        break;
      case 0xb3:
        S[sp - 1] = b32(static_cast<float>(u32(S[sp - 1])));
        break;
      case 0xb4:
        S[sp - 1] = b32(static_cast<float>(static_cast<int64_t>(S[sp - 1])));
        break;
      case 0xb5:
        S[sp - 1] = b32(from_u64<float>(S[sp - 1]));
        break;
      case 0xb6:
        S[sp - 1] = b32(static_cast<float>(f64(S[sp - 1])));
        break;
      case 0xb7:
        S[sp - 1] = b64(static_cast<double>(s32(S[sp - 1])));
        break;
      case 0xb8:
        S[sp - 1] = b64(static_cast<double>(u32(S[sp - 1])));
        break;
      case 0xb9:
        S[sp - 1] = b64(static_cast<double>(static_cast<int64_t>(S[sp - 1])));
        break;
      case 0xba:
        S[sp - 1] = b64(from_u64<double>(S[sp - 1]));
        break;
      case 0xbb:
        S[sp - 1] = b64(static_cast<double>(f32(S[sp - 1])));
        break;
      case 0xbc:
      case 0xbd:
      case 0xbe:
      case 0xbf:
        break;  // the bits are the value
      case 0xc0:
        S[sp - 1] = w32s(static_cast<int8_t>(S[sp - 1]));
        break;
      case 0xc1:
        S[sp - 1] = w32s(static_cast<int16_t>(S[sp - 1]));
        break;
      case 0xc2:
        S[sp - 1] = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int8_t>(S[sp - 1])));
        break;
      case 0xc3:
        S[sp - 1] = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(S[sp - 1])));
        break;
      case 0xc4:
        S[sp - 1] = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(S[sp - 1])));
        break;
        TRUNC(kPrefixFC + 0, int32_t, float, f32, -k2p31 - 1, k2p31, true, w32s(v))
        TRUNC(kPrefixFC + 1, uint32_t, float, f32, -1.0, k2p32, true, w32(v))
        TRUNC(kPrefixFC + 2, int32_t, double, f64, -k2p31 - 1, k2p31, true, w32s(v))
        TRUNC(kPrefixFC + 3, uint32_t, double, f64, -1.0, k2p32, true, w32(v))
        TRUNC(kPrefixFC + 4, int64_t, float, f32, -k2p63 * (1 + 0x1p-52), k2p63, true,
              static_cast<uint64_t>(v))
        TRUNC(kPrefixFC + 5, uint64_t, float, f32, -1.0, k2p64, true, v)
        TRUNC(kPrefixFC + 6, int64_t, double, f64, -k2p63 * (1 + 0x1p-52), k2p63, true,
              static_cast<uint64_t>(v))
        TRUNC(kPrefixFC + 7, uint64_t, double, f64, -1.0, k2p64, true, v)
      case kPrefixFC + 10: {
        const uint64_t n = u32(S[--sp]);
        const uint64_t src = u32(S[--sp]);
        const uint64_t dst = u32(S[--sp]);
        if (src + n > mem_size || dst + n > mem_size)
          TRAP(Trap::memory, "a memory.copy past the end");
        if (n > 0) std::memmove(mem + dst, mem + src, n);
        break;
      }
      case kPrefixFC + 11: {
        const uint64_t n = u32(S[--sp]);
        const uint8_t v = static_cast<uint8_t>(S[--sp]);
        const uint64_t dst = u32(S[--sp]);
        if (dst + n > mem_size) TRAP(Trap::memory, "a memory.fill past the end");
        if (n > 0) std::memset(mem + dst, v, n);
        break;
      }
      default:
        TRAP(Trap::unreachable,
             "an instruction the validator let through but the interpreter "
             "does not run (a bug: report it)");
    }
  }
}

}  // namespace

uint32_t add_host_function(Instance& in, const FuncType& type, HostFn fn, void* user) {
  const Module& m = *in.module;
  if (!m.has_table || in.table.size() >= m.table_max) return kNullFunc;
  in.host.push_back({type, fn, user});
  const uint32_t index = in.host_index(in.host.size() - 1);
  in.table.push_back(index);
  return static_cast<uint32_t>(in.table.size() - 1);
}

bool instantiate(const Module& m, const std::vector<HostImport>& imports, uint32_t memory_cap,
                 uint64_t fuel, Instance& out, std::string& error) {
  out = Instance{};
  out.module = &m;
  for (const Import& im : m.imports) {
    const HostImport* found = nullptr;
    for (const HostImport& h : imports)
      if (h.module == im.module && h.name == im.name) found = &h;
    if (found == nullptr) {
      error = "the module imports " + im.module + "." + im.name +
              ", which the host does not offer; next: call only the host table's functions "
              "(mod/abi.hpp), and link with the engine's template";
      return false;
    }
    if (!(found->type == m.types[im.type])) {
      error = "the module imports " + im.module + "." + im.name +
              " with another type than the host's; next: build against this engine's mod/abi.hpp";
      return false;
    }
    out.host.push_back({found->type, found->fn, found->user});
  }
  if (m.has_memory) {
    out.memory_max = std::min(m.memory_max, memory_cap);
    if (m.memory_min > out.memory_max) {
      error = "the module asks for " + std::to_string(m.memory_min) + " pages of memory, past " +
              "the " + std::to_string(out.memory_max) + " the host allows; next: give the mod " +
              "less memory (-Wl,--initial-memory)";
      return false;
    }
    out.memory.assign(static_cast<size_t>(m.memory_min) * kPageBytes, 0);
  }
  for (const Global& g : m.globals) out.globals.push_back(g.init);
  if (m.has_table) out.table.assign(m.table_min, kNullFunc);
  for (const Segment& s : m.elements) {
    if (static_cast<uint64_t>(s.offset) + s.funcs.size() > out.table.size()) {
      error = "an element segment past the table's end; next: rebuild the mod";
      return false;
    }
    for (size_t k = 0; k < s.funcs.size(); k++) out.table[s.offset + k] = s.funcs[k];
  }
  for (const Segment& s : m.data) {
    if (!write_memory(out, s.offset, s.bytes.data(), static_cast<uint32_t>(s.bytes.size()))) {
      error = "a data segment past the memory's end; next: rebuild the mod";
      return false;
    }
  }
  out.stack.assign(kStackSlots, 0);
  if (m.start != kNoStart) {
    const Trap t = call(out, m.start, nullptr, nullptr, fuel);
    if (t != Trap::none) {
      error = std::string("the module's start function trapped (") + trap_name(t) +
              "): " + out.trap_message + "; next: fix the mod's static initialisers";
      return false;
    }
  }
  return true;
}

Trap call(Instance& in, uint32_t func, const uint64_t* args, uint64_t* results, uint64_t fuel) {
  const Module& m = *in.module;
  in.trap_message.clear();
  in.depth = 0;
  in.fuel = fuel;
  if (func >= m.func_count()) {
    in.trap_message = "a call to function " + std::to_string(func) + ", past the module's";
    return Trap::indirect;
  }
  const FuncType& t = m.func_type(func);
  const size_t np = t.params.size();
  if (np > 0) {
    if (args == nullptr) {
      in.trap_message = "a call to function " + std::to_string(func) + " with no arguments";
      return Trap::host;
    }
    std::memcpy(in.stack.data(), args, np * sizeof(uint64_t));
  }
  const Trap trap = invoke(in, func, 0);
  if (trap == Trap::none && results != nullptr)
    for (size_t k = 0; k < t.results.size(); k++) results[k] = in.stack[k];
  return trap;
}

}  // namespace ge::mod::wasm
