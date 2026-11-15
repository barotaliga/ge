// Every function body of a WebAssembly module checked by the format's typing rules and compiled
// to `Inst`s as it is checked (module.hpp § The check): the operand stack's types, the control
// frames, each branch resolved to its target, the values it keeps and the height it cuts back to.
//
// ge:summary The WebAssembly validator: each body type-checked and compiled to Insts
// ge:keywords wasm validate typecheck control-frame branch br_table block loop if compile
// ge:skill mod
// ge:test tests/mod_wasm_test.cpp
#include <algorithm>

#include "mod/wasm/module.hpp"
#include "mod/wasm/reader.hpp"

namespace ge::mod::wasm {

namespace {

constexpr uint8_t kAny = 0;  // an operand of unknown type (after unreachable code)
constexpr uint32_t kTablePatch = 0x80000000u;

enum class Kind : uint8_t { func, block, loop, if_ };

struct Ctl {
  Kind kind = Kind::block;
  std::vector<ValType> params, results;
  uint32_t height = 0;
  bool unreachable = false;
  uint32_t start = 0;              // a loop's first instruction
  uint32_t if_jump = 0xffffffffu;  // an if's JumpIfFalse, until else or end patches it
  bool has_else = false;
  std::vector<uint32_t> patches;  // branches to this frame's end
};

class Checker {
 public:
  Checker(Reader& r, const Module& m, Func& f) : r_(r), m_(m), f_(f) {}

  bool run() {
    const FuncType& t = m_.types[f_.type];
    Ctl outer;
    outer.kind = Kind::func;
    outer.results = t.results;
    ctl_.push_back(std::move(outer));
    while (r_.ok() && !ctl_.empty()) step();
    if (r_.ok() && !r_.done()) r_.fail("bytes after the function's end");
    return r_.ok();
  }

 private:
  // The operand stack.
  void push(ValType t) {
    ops_.push_back(static_cast<uint8_t>(t));
    f_.max_height = std::max(f_.max_height, static_cast<uint32_t>(ops_.size()));
  }
  void push_any() { ops_.push_back(kAny); }
  uint8_t pop() {
    Ctl& c = ctl_.back();
    if (ops_.size() == c.height) {
      if (!c.unreachable) r_.fail("an instruction pops a value its block does not have");
      return kAny;
    }
    const uint8_t t = ops_.back();
    ops_.pop_back();
    return t;
  }
  uint8_t pop(ValType want) {
    const uint8_t t = pop();
    if (t != kAny && t != static_cast<uint8_t>(want))
      r_.fail(std::string("an operand of ") + type_name(static_cast<ValType>(t)) + " where " +
              type_name(want) + " is wanted");
    return t;
  }
  void pop_all(const std::vector<ValType>& ts) {
    for (size_t k = ts.size(); k-- > 0;) pop(ts[k]);
  }
  void push_all(const std::vector<ValType>& ts) {
    for (ValType t : ts) push(t);
  }
  void unreachable() {
    Ctl& c = ctl_.back();
    ops_.resize(c.height);
    c.unreachable = true;
  }

  uint32_t emit(uint16_t op, uint32_t a = 0, uint64_t b = 0) {
    f_.code.push_back({op, a, b});
    return static_cast<uint32_t>(f_.code.size() - 1);
  }
  uint32_t here() const { return static_cast<uint32_t>(f_.code.size()); }

  // A label `depth` frames out: its types, and the instruction a branch to it records.
  const std::vector<ValType>& label_types(const Ctl& c) const {
    return c.kind == Kind::loop ? c.params : c.results;
  }
  Ctl* label(uint32_t depth) {
    if (depth >= ctl_.size()) {
      r_.fail("a branch to label " + std::to_string(depth) + " past the enclosing blocks");
      return nullptr;
    }
    return &ctl_[ctl_.size() - 1 - depth];
  }
  // A branch to `c` from instruction `at` (or br_table slot `slot`): a loop's start now, any other
  // frame's end when it closes.
  void bind(Ctl& c, uint32_t patch, uint32_t& target) {
    if (c.kind == Kind::loop)
      target = c.start;
    else
      c.patches.push_back(patch);
  }

  bool block_type(std::vector<ValType>& params, std::vector<ValType>& results) {
    const int64_t bt = r_.s33();
    if (!r_.ok()) return false;
    if (bt == -64) return true;  // 0x40: no values
    if (bt < 0) {
      const uint8_t b = static_cast<uint8_t>(bt & 0x7f);
      if (b != 0x7f && b != 0x7e && b != 0x7d && b != 0x7c)
        return r_.fail("a block type " + hex_byte(b));
      results.push_back(static_cast<ValType>(b));
      return true;
    }
    if (static_cast<uint64_t>(bt) >= m_.types.size())
      return r_.fail("a block of type " + std::to_string(bt) + " past the types");
    params = m_.types[bt].params;
    results = m_.types[bt].results;
    return true;
  }

  void open(Kind kind) {
    Ctl c;
    c.kind = kind;
    if (!block_type(c.params, c.results)) return;
    if (kind == Kind::if_) {
      pop(ValType::i32);
      c.if_jump = emit(kOpJumpIfFalse);
    }
    pop_all(c.params);
    c.height = static_cast<uint32_t>(ops_.size());
    c.start = here();
    push_all(c.params);
    ctl_.push_back(std::move(c));
  }

  void patch_all(Ctl& c, uint32_t target) {
    for (uint32_t p : c.patches) {
      if (p & kTablePatch)
        f_.table[p & ~kTablePatch].pc = target;
      else
        f_.code[p].a = target;
    }
  }

  void end_types(Ctl& c) {
    pop_all(c.results);
    if (ops_.size() != c.height) r_.fail("a block ends with values it does not return");
  }

  void op_else() {
    Ctl& c = ctl_.back();
    if (c.kind != Kind::if_ || c.has_else) {
      r_.fail("an else outside an if");
      return;
    }
    end_types(c);
    c.patches.push_back(emit(kOpJump));
    f_.code[c.if_jump].a = here();
    c.if_jump = 0xffffffffu;
    c.has_else = true;
    c.unreachable = false;
    ops_.resize(c.height);
    push_all(c.params);
  }

  void op_end() {
    Ctl& c = ctl_.back();
    end_types(c);
    if (c.kind == Kind::if_ && !c.has_else) {
      if (c.params != c.results) r_.fail("an if with no else whose results differ from its params");
      f_.code[c.if_jump].a = here();
    }
    if (c.kind == Kind::func) {
      const uint32_t ret = emit(kOpReturn, static_cast<uint32_t>(c.results.size()));
      patch_all(c, ret);
      ctl_.pop_back();
      return;
    }
    patch_all(c, here());
    std::vector<ValType> results = c.results;
    ops_.resize(c.height);
    ctl_.pop_back();
    push_all(results);
  }

  void op_br(uint16_t op) {
    const uint32_t depth = r_.u32();
    if (op == kOpBrIf) pop(ValType::i32);
    Ctl* c = label(depth);
    if (c == nullptr) return;
    const std::vector<ValType> types = label_types(*c);
    pop_all(types);
    const uint32_t at = emit(op, 0, (static_cast<uint64_t>(types.size()) << 32) | c->height);
    bind(*c, at, f_.code[at].a);
    if (op == kOpBr)
      unreachable();
    else
      push_all(types);
  }

  void op_br_table() {
    const uint32_t n = r_.u32();
    if (n > 65536) {
      r_.fail("a br_table of " + std::to_string(n) + " labels");
      return;
    }
    std::vector<uint32_t> depths(n + 1);
    for (uint32_t k = 0; k <= n && r_.ok(); k++) depths[k] = r_.u32();
    pop(ValType::i32);
    Ctl* def = label(depths[n]);
    if (def == nullptr) return;
    const std::vector<ValType> types = label_types(*def);
    const uint32_t first = static_cast<uint32_t>(f_.table.size());
    for (uint32_t k = 0; k <= n && r_.ok(); k++) {
      Ctl* c = label(depths[k]);
      if (c == nullptr) return;
      if (label_types(*c) != types) {
        r_.fail("a br_table whose labels take different values");
        return;
      }
      f_.table.push_back({0, static_cast<uint32_t>(types.size()), c->height});
      const uint32_t slot = first + k;
      bind(*c, slot | kTablePatch, f_.table[slot].pc);
    }
    pop_all(types);
    emit(kOpBrTable, first, n);
    unreachable();
  }

  void op_memarg(uint16_t op, int natural_log2, ValType addr_then, bool store, ValType value) {
    const uint32_t align = r_.u32();
    const uint32_t offset = r_.u32();
    if (!m_.has_memory) {
      r_.fail("a load or store with no memory");
      return;
    }
    if (align > static_cast<uint32_t>(natural_log2)) {
      r_.fail("an alignment past the access's natural one");
      return;
    }
    (void)addr_then;
    if (store) {
      pop(value);
      pop(ValType::i32);
    } else {
      pop(ValType::i32);
      push(value);
    }
    emit(op, offset);
  }

  void unary(uint16_t op, ValType in, ValType out) {
    pop(in);
    push(out);
    emit(op);
  }
  void binary(uint16_t op, ValType in, ValType out) {
    pop(in);
    pop(in);
    push(out);
    emit(op);
  }

  bool numeric(uint8_t op) {
    using V = ValType;
    if (op == 0x45) return unary(op, V::i32, V::i32), true;
    if (op >= 0x46 && op <= 0x4f) return binary(op, V::i32, V::i32), true;
    if (op == 0x50) return unary(op, V::i64, V::i32), true;
    if (op >= 0x51 && op <= 0x5a) return binary(op, V::i64, V::i32), true;
    if (op >= 0x5b && op <= 0x60) return binary(op, V::f32, V::i32), true;
    if (op >= 0x61 && op <= 0x66) return binary(op, V::f64, V::i32), true;
    if (op >= 0x67 && op <= 0x69) return unary(op, V::i32, V::i32), true;
    if (op >= 0x6a && op <= 0x78) return binary(op, V::i32, V::i32), true;
    if (op >= 0x79 && op <= 0x7b) return unary(op, V::i64, V::i64), true;
    if (op >= 0x7c && op <= 0x8a) return binary(op, V::i64, V::i64), true;
    if (op >= 0x8b && op <= 0x91) return unary(op, V::f32, V::f32), true;
    if (op >= 0x92 && op <= 0x98) return binary(op, V::f32, V::f32), true;
    if (op >= 0x99 && op <= 0x9f) return unary(op, V::f64, V::f64), true;
    if (op >= 0xa0 && op <= 0xa6) return binary(op, V::f64, V::f64), true;
    switch (op) {
      case 0xa7:
        return unary(op, V::i64, V::i32), true;
      case 0xa8:
      case 0xa9:
        return unary(op, V::f32, V::i32), true;
      case 0xaa:
      case 0xab:
        return unary(op, V::f64, V::i32), true;
      case 0xac:
      case 0xad:
        return unary(op, V::i32, V::i64), true;
      case 0xae:
      case 0xaf:
        return unary(op, V::f32, V::i64), true;
      case 0xb0:
      case 0xb1:
        return unary(op, V::f64, V::i64), true;
      case 0xb2:
      case 0xb3:
        return unary(op, V::i32, V::f32), true;
      case 0xb4:
      case 0xb5:
        return unary(op, V::i64, V::f32), true;
      case 0xb6:
        return unary(op, V::f64, V::f32), true;
      case 0xb7:
      case 0xb8:
        return unary(op, V::i32, V::f64), true;
      case 0xb9:
      case 0xba:
        return unary(op, V::i64, V::f64), true;
      case 0xbb:
        return unary(op, V::f32, V::f64), true;
      case 0xbc:
        return unary(op, V::f32, V::i32), true;
      case 0xbd:
        return unary(op, V::f64, V::i64), true;
      case 0xbe:
        return unary(op, V::i32, V::f32), true;
      case 0xbf:
        return unary(op, V::i64, V::f64), true;
      case 0xc0:
      case 0xc1:
        return unary(op, V::i32, V::i32), true;
      case 0xc2:
      case 0xc3:
      case 0xc4:
        return unary(op, V::i64, V::i64), true;
      default:
        return false;
    }
  }

  void prefix_fc() {
    using V = ValType;
    const uint32_t sub = r_.u32();
    const uint16_t op = static_cast<uint16_t>(kPrefixFC + sub);
    switch (sub) {
      case 0:
      case 1:
        unary(op, V::f32, V::i32);
        return;
      case 2:
      case 3:
        unary(op, V::f64, V::i32);
        return;
      case 4:
      case 5:
        unary(op, V::f32, V::i64);
        return;
      case 6:
      case 7:
        unary(op, V::f64, V::i64);
        return;
      case 10:
        if (r_.byte() != 0 || r_.byte() != 0) {
          r_.fail("a memory.copy between memories other than 0");
          return;
        }
        if (!m_.has_memory) {
          r_.fail("a memory.copy with no memory");
          return;
        }
        pop(V::i32), pop(V::i32), pop(V::i32);
        emit(op);
        return;
      case 11:
        if (r_.byte() != 0) {
          r_.fail("a memory.fill of a memory other than 0");
          return;
        }
        if (!m_.has_memory) {
          r_.fail("a memory.fill with no memory");
          return;
        }
        pop(V::i32), pop(V::i32), pop(V::i32);
        emit(op);
        return;
      default:
        r_.fail("the instruction 0xfc " + std::to_string(sub) +
                " (memory.init, data.drop and the table instructions are not run)");
    }
  }

  void call(uint32_t fn) {
    if (fn >= m_.func_count()) {
      r_.fail("a call to function " + std::to_string(fn) + " past the functions");
      return;
    }
    const FuncType& t = m_.func_type(fn);
    pop_all(t.params);
    push_all(t.results);
    emit(0x10, fn);
  }

  void step() {
    const uint8_t op = r_.byte();
    if (!r_.ok()) return;
    switch (op) {
      case 0x00:
        emit(op);
        unreachable();
        return;
      case 0x01:
        return;
      case 0x02:
        open(Kind::block);
        return;
      case 0x03:
        open(Kind::loop);
        return;
      case 0x04:
        open(Kind::if_);
        return;
      case 0x05:
        op_else();
        return;
      case 0x0b:
        op_end();
        return;
      case 0x0c:
        op_br(kOpBr);
        return;
      case 0x0d:
        op_br(kOpBrIf);
        return;
      case 0x0e:
        op_br_table();
        return;
      case 0x0f: {
        const std::vector<ValType>& results = ctl_.front().results;
        pop_all(results);
        emit(kOpReturn, static_cast<uint32_t>(results.size()));
        unreachable();
        return;
      }
      case 0x10:
        call(r_.u32());
        return;
      case 0x11: {
        const uint32_t type = r_.u32();
        if (r_.byte() != 0 || !m_.has_table) {
          r_.fail("a call_indirect through a table other than 0");
          return;
        }
        if (type >= m_.types.size()) {
          r_.fail("a call_indirect of type " + std::to_string(type) + " past the types");
          return;
        }
        pop(ValType::i32);
        pop_all(m_.types[type].params);
        push_all(m_.types[type].results);
        emit(op, type);
        return;
      }
      case 0x1a:
        pop();
        emit(op);
        return;
      case 0x1b:
      case 0x1c: {
        if (op == 0x1c) {
          const uint32_t n = r_.u32();
          ValType t = ValType::i32;
          if (n != 1) {
            r_.fail("a typed select of " + std::to_string(n) + " types");
            return;
          }
          const uint8_t b = r_.byte();
          if (b != 0x7f && b != 0x7e && b != 0x7d && b != 0x7c) {
            r_.fail("a typed select of " + hex_byte(b));
            return;
          }
          t = static_cast<ValType>(b);
          pop(ValType::i32);
          pop(t);
          pop(t);
          push(t);
        } else {
          pop(ValType::i32);
          const uint8_t a = pop();
          const uint8_t b = pop();
          if (a != kAny && b != kAny && a != b) {
            r_.fail("a select of two different types");
            return;
          }
          if (a == kAny && b == kAny)
            push_any();
          else
            push(static_cast<ValType>(a == kAny ? b : a));
        }
        emit(0x1b);
        return;
      }
      case 0x20:
      case 0x21:
      case 0x22: {
        const uint32_t i = r_.u32();
        if (i >= f_.locals.size()) {
          r_.fail("local " + std::to_string(i) + " past the function's locals");
          return;
        }
        const ValType t = f_.locals[i];
        if (op == 0x20) {
          push(t);
        } else {
          pop(t);
          if (op == 0x22) push(t);
        }
        emit(op, i);
        return;
      }
      case 0x23:
      case 0x24: {
        const uint32_t i = r_.u32();
        if (i >= m_.globals.size()) {
          r_.fail("global " + std::to_string(i) + " past the globals");
          return;
        }
        const Global& g = m_.globals[i];
        if (op == 0x23) {
          push(g.type);
        } else {
          if (!g.mut) {
            r_.fail("a global.set of the immutable global " + std::to_string(i));
            return;
          }
          pop(g.type);
        }
        emit(op, i);
        return;
      }
      case 0x28:
        op_memarg(op, 2, ValType::i32, false, ValType::i32);
        return;
      case 0x29:
        op_memarg(op, 3, ValType::i32, false, ValType::i64);
        return;
      case 0x2a:
        op_memarg(op, 2, ValType::i32, false, ValType::f32);
        return;
      case 0x2b:
        op_memarg(op, 3, ValType::i32, false, ValType::f64);
        return;
      case 0x2c:
      case 0x2d:
        op_memarg(op, 0, ValType::i32, false, ValType::i32);
        return;
      case 0x2e:
      case 0x2f:
        op_memarg(op, 1, ValType::i32, false, ValType::i32);
        return;
      case 0x30:
      case 0x31:
        op_memarg(op, 0, ValType::i32, false, ValType::i64);
        return;
      case 0x32:
      case 0x33:
        op_memarg(op, 1, ValType::i32, false, ValType::i64);
        return;
      case 0x34:
      case 0x35:
        op_memarg(op, 2, ValType::i32, false, ValType::i64);
        return;
      case 0x36:
        op_memarg(op, 2, ValType::i32, true, ValType::i32);
        return;
      case 0x37:
        op_memarg(op, 3, ValType::i32, true, ValType::i64);
        return;
      case 0x38:
        op_memarg(op, 2, ValType::i32, true, ValType::f32);
        return;
      case 0x39:
        op_memarg(op, 3, ValType::i32, true, ValType::f64);
        return;
      case 0x3a:
        op_memarg(op, 0, ValType::i32, true, ValType::i32);
        return;
      case 0x3b:
        op_memarg(op, 1, ValType::i32, true, ValType::i32);
        return;
      case 0x3c:
        op_memarg(op, 0, ValType::i32, true, ValType::i64);
        return;
      case 0x3d:
        op_memarg(op, 1, ValType::i32, true, ValType::i64);
        return;
      case 0x3e:
        op_memarg(op, 2, ValType::i32, true, ValType::i64);
        return;
      case 0x3f:
      case 0x40:
        if (r_.byte() != 0 || !m_.has_memory) {
          r_.fail("memory.size or memory.grow of a memory other than 0");
          return;
        }
        if (op == 0x40) pop(ValType::i32);
        push(ValType::i32);
        emit(op);
        return;
      case 0x41:
        push(ValType::i32);
        emit(op, 0, static_cast<uint32_t>(r_.s32()));
        return;
      case 0x42:
        push(ValType::i64);
        emit(op, 0, static_cast<uint64_t>(r_.s64()));
        return;
      case 0x43:
        push(ValType::f32);
        emit(op, 0, r_.fixed(4));
        return;
      case 0x44:
        push(ValType::f64);
        emit(op, 0, r_.fixed(8));
        return;
      case 0xfc:
        prefix_fc();
        return;
      default:
        if (!numeric(op))
          r_.fail("the instruction " + hex_byte(op) +
                  " (not in the subset the engine runs: the MVP, sign extension, saturating "
                  "conversions, memory.copy and memory.fill)");
    }
  }

  Reader& r_;
  const Module& m_;
  Func& f_;
  std::vector<uint8_t> ops_;
  std::vector<Ctl> ctl_;
};

}  // namespace

bool validate_code(Reader& r, Module& m) {
  const uint32_t n = r.u32();
  if (n != m.funcs.size())
    return r.fail("a code section of " + std::to_string(n) + " bodies for " +
                  std::to_string(m.funcs.size()) + " functions");
  for (uint32_t i = 0; i < n && r.ok(); i++) {
    const uint32_t size = r.u32();
    const size_t at = r.pos();
    const uint8_t* body = r.take(size);
    if (body == nullptr) return false;
    Reader b(body, size, at);
    Func& f = m.funcs[i];
    f.locals = m.types[f.type].params;
    const uint32_t groups = b.u32();
    uint64_t total = f.locals.size();
    for (uint32_t g = 0; g < groups && b.ok(); g++) {
      const uint32_t count = b.u32();
      const uint8_t t = b.byte();
      if (t != 0x7f && t != 0x7e && t != 0x7d && t != 0x7c)
        return b.fail("a local of type " + hex_byte(t)), r.adopt(b);
      total += count;
      if (total > 50000) return b.fail("a function with more than 50000 locals"), r.adopt(b);
      f.locals.insert(f.locals.end(), count, static_cast<ValType>(t));
    }
    Checker c(b, m, f);
    if (!b.ok() || !c.run()) return r.adopt(b);
  }
  return r.ok();
}

}  // namespace ge::mod::wasm
