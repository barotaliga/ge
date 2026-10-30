// The interpreter (vm.hpp): the statement loop, the call convention, the guards, the strings and
// the faults. The loop is compiled twice, with and without the trace hooks, so a run nobody
// watches pays no branch for them.
//
// ge:summary The QC interpreter: the statement loop, calls, guards, strings, faults, profile
// ge:keywords qc progs vm interpreter execute statement-loop opcodes enter leave runaway fault
// ge:keywords bitand bitor truncation state address storep ieee
// ge:skill progs
// ge:test tests/progs_vm_test.cpp tests/progs_ops_test.cpp
#include "progs/vm.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "progs/opcodes.hpp"

namespace ge::progs {

namespace {

// Truncation toward zero to a 32-bit signed integer; out of range or NaN is -2147483648, the
// value the classic machines give, so a mod's flags read the same everywhere.
int32_t trunc_i32(float f) {
  if (!(f > -2147483904.0f && f < 2147483648.0f)) return INT32_MIN;
  return static_cast<int32_t>(f);
}

void raise(Vm& vm, const char* message) {
  if (vm.fault.set) return;  // the first fault is the one that names the cause
  vm.fault.set = true;
  std::snprintf(vm.fault.message, sizeof vm.fault.message, "%s", message);
  // The stack as it stands: the running statement, then each caller (innermost first).
  int n = 0;
  vm.fault.frames[n++] = {vm.function, vm.statement};
  for (int d = vm.depth - 1; d >= 0 && n <= kMaxCallDepth; d--) {
    if (vm.stack[d].function == 0) continue;  // the engine's entry
    vm.fault.frames[n++] = vm.stack[d];
  }
  vm.fault.depth = n;
}

void raisef(Vm& vm, const char* fmt, long long a, long long b = 0) {
  char m[200];
  std::snprintf(m, sizeof m, fmt, a, b);
  raise(vm, m);
}

// Enters function `fn` (not a builtin), called from statement `from` of the running function.
// The statement to run next, or -1 on a fault.
int32_t enter(Vm& vm, int32_t fn, int32_t from) {
  const Function& f = vm.progs->functions[static_cast<size_t>(fn)];
  if (vm.depth >= kMaxCallDepth) {
    raise(vm, "stack overflow (32 functions deep)");
    return -1;
  }
  if (vm.local_used + f.locals > kLocalStackWords) {
    raise(vm, "locals stack overflow (2048 words)");
    return -1;
  }
  vm.stack[vm.depth++] = {vm.function, from};
  uint32_t* g = vm.globals.data();
  std::memcpy(vm.local_stack.data() + vm.local_used, g + f.parm_start,
              static_cast<size_t>(f.locals) * 4);
  vm.local_used += f.locals;
  int32_t to = f.parm_start;
  for (int i = 0; i < f.num_parms; i++) {
    for (int j = 0; j < f.parm_size[i]; j++) g[to++] = g[parm(i) + j];
  }
  vm.function = fn;
  vm.profile[static_cast<size_t>(fn)].calls++;
  return f.first_statement;
}

// Leaves the running function: its saved words restored, the caller's statement returned.
int32_t leave(Vm& vm) {
  const Function& f = vm.progs->functions[static_cast<size_t>(vm.function)];
  vm.local_used -= f.locals;
  std::memcpy(vm.globals.data() + f.parm_start, vm.local_stack.data() + vm.local_used,
              static_cast<size_t>(f.locals) * 4);
  const Frame& back = vm.stack[--vm.depth];
  vm.function = back.function;
  return back.statement;
}

// After a fault: every frame above the engine call's entry popped, its locals restored.
void unwind(Vm& vm, int entry_depth) {
  while (vm.depth > entry_depth) {
    if (vm.function != 0 &&
        vm.progs->functions[static_cast<size_t>(vm.function)].first_statement >= 0)
      leave(vm);
    else
      vm.function = vm.stack[--vm.depth].function;
  }
}

// Runs builtin `number` with `argc` arguments. False on a fault.
bool run_builtin(Vm& vm, int32_t number, int argc) {
  if (number <= 0 || number >= static_cast<int32_t>(vm.builtins.size()) ||
      vm.builtins[static_cast<size_t>(number)].fn == nullptr) {
    raisef(vm, "builtin %lld is not provided by this game", number);
    return false;
  }
  vm.argc = argc;
  vm.builtin_calls[static_cast<size_t>(number)]++;
  vm.builtins[static_cast<size_t>(number)].fn(vm);
  return !vm.fault.set;
}

template <bool kTraced>
bool execute(Vm& vm, int32_t fn) {
  const Progs& p = *vm.progs;
  const Statement* const code = p.statements.data();
  const int32_t num_statements = static_cast<int32_t>(p.statements.size());
  const int32_t num_functions = static_cast<int32_t>(p.functions.size());
  uint32_t* const g = vm.globals.data();
  uint32_t* const ents = vm.entities.data();
  const int entry_depth = vm.depth;
  const TraceHooks* t = vm.trace;

  const auto F = [g](int ofs) { return as_float(g[ofs]); };
  // A NaN result is stored as the one quiet NaN 0x7fc00000: which NaN an operation hands back
  // (its sign, which operand's payload) differs between CPUs and with the operand order a
  // compiler picks, and the bits must not.
  const auto setF = [g](int ofs, float v) { g[ofs] = v != v ? 0x7fc00000u : as_word(v); };
  const auto I = [g](int ofs) { return static_cast<int32_t>(g[ofs]); };
  const auto truth = [](bool b) { return b ? 1.0f : 0.0f; };

  vm.statement = -1;
  int32_t st = enter(vm, fn, -1);
  if (st < 0) return false;
  if constexpr (kTraced) {
    if (t->enter != nullptr) t->enter(t->ctx, vm, fn, vm.depth);
  }
  int64_t* counter = &vm.profile[static_cast<size_t>(fn)].statements;
  int count = 0;

  // Entity A and field B checked; the word index of the field, or -1 after a fault.
  const auto field_word = [&](int32_t ev, int32_t field, int width) -> int64_t {
    const int number = entity_number(vm, ev);
    if (number < 0) {
      raisef(vm, "bad entity value %lld (not an entity in use)", ev);
      return -1;
    }
    if (field < 0 || field + width > vm.entityfields) {
      raisef(vm, "bad field %lld (the entity has %lld words)", field, vm.entityfields);
      return -1;
    }
    return static_cast<int64_t>(number) * vm.entityfields + field;
  };
  const auto string_of = [&](int32_t value) -> const char* {
    const char* s = vm_string(vm, value);
    if (s == nullptr) raisef(vm, "bad string value %lld", value);
    return s;
  };

  for (;;) {
    vm.statement = st;
    if (++count >= kRunawayLimit) {
      raise(vm, "runaway loop (100000 statements in one engine call)");
      break;
    }
    ++*counter;
    if constexpr (kTraced) {
      if (t->statement != nullptr) t->statement(t->ctx, vm, vm.function, st);
    }
    const Statement s = code[st];
    const int a = s.a, b = s.b, c = s.c;
    int32_t next = st + 1;
    switch (s.op) {
      case kOpDone:
      case kOpReturn: {
        g[kGlobalReturn] = g[a];
        g[kGlobalReturn + 1] = g[a + 1];
        g[kGlobalReturn + 2] = g[a + 2];
        if constexpr (kTraced) {
          if (t->leave != nullptr) t->leave(t->ctx, vm, vm.function, vm.depth);
        }
        next = leave(vm);
        if (vm.depth == entry_depth) {
          vm.statement = next;
          vm.statements_run += count;
          return true;
        }
        counter = &vm.profile[static_cast<size_t>(vm.function)].statements;
        next = next + 1;
        break;
      }
      case kOpMulF:
        setF(c, F(a) * F(b));
        break;
      case kOpMulV:
        setF(c, (F(a) * F(b) + F(a + 1) * F(b + 1)) + F(a + 2) * F(b + 2));
        break;
      case kOpMulFV: {
        const float k = F(a);
        const float x = k * F(b), y = k * F(b + 1), z = k * F(b + 2);
        setF(c, x);
        setF(c + 1, y);
        setF(c + 2, z);
        break;
      }
      case kOpMulVF: {
        const float k = F(b);
        const float x = F(a) * k, y = F(a + 1) * k, z = F(a + 2) * k;
        setF(c, x);
        setF(c + 1, y);
        setF(c + 2, z);
        break;
      }
      case kOpDivF:
        setF(c, F(a) / F(b));
        break;
      case kOpAddF:
        setF(c, F(a) + F(b));
        break;
      case kOpAddV: {
        const float x = F(a) + F(b), y = F(a + 1) + F(b + 1), z = F(a + 2) + F(b + 2);
        setF(c, x);
        setF(c + 1, y);
        setF(c + 2, z);
        break;
      }
      case kOpSubF:
        setF(c, F(a) - F(b));
        break;
      case kOpSubV: {
        const float x = F(a) - F(b), y = F(a + 1) - F(b + 1), z = F(a + 2) - F(b + 2);
        setF(c, x);
        setF(c + 1, y);
        setF(c + 2, z);
        break;
      }
      case kOpEqF:
        setF(c, truth(F(a) == F(b)));
        break;
      case kOpEqV:
        setF(c, truth(F(a) == F(b) && F(a + 1) == F(b + 1) && F(a + 2) == F(b + 2)));
        break;
      case kOpEqS:
      case kOpNeS: {
        const char* x = string_of(I(a));
        const char* y = x != nullptr ? string_of(I(b)) : nullptr;
        if (y == nullptr) break;
        const bool same = std::strcmp(x, y) == 0;
        setF(c, truth(s.op == kOpEqS ? same : !same));
        break;
      }
      case kOpEqE:
      case kOpEqFnc:
        setF(c, truth(g[a] == g[b]));
        break;
      case kOpNeF:
        setF(c, truth(F(a) != F(b)));
        break;
      case kOpNeV:
        setF(c, truth(F(a) != F(b) || F(a + 1) != F(b + 1) || F(a + 2) != F(b + 2)));
        break;
      case kOpNeE:
      case kOpNeFnc:
        setF(c, truth(g[a] != g[b]));
        break;
      case kOpLe:
        setF(c, truth(F(a) <= F(b)));
        break;
      case kOpGe:
        setF(c, truth(F(a) >= F(b)));
        break;
      case kOpLt:
        setF(c, truth(F(a) < F(b)));
        break;
      case kOpGt:
        setF(c, truth(F(a) > F(b)));
        break;
      case kOpLoadF:
      case kOpLoadS:
      case kOpLoadEnt:
      case kOpLoadFld:
      case kOpLoadFnc: {
        if (I(b) < 0) {  // a negative field reads 0 (a client's command can produce one)
          if (entity_number(vm, I(a)) < 0) {
            raisef(vm, "bad entity value %lld (not an entity in use)", I(a));
            break;
          }
          g[c] = 0;
          break;
        }
        const int64_t w = field_word(I(a), I(b), 1);
        if (w >= 0) g[c] = ents[w];
        break;
      }
      case kOpLoadV: {
        const int64_t w = field_word(I(a), I(b), 3);
        if (w < 0) break;
        const uint32_t x = ents[w], y = ents[w + 1], z = ents[w + 2];
        g[c] = x;
        g[c + 1] = y;
        g[c + 2] = z;
        break;
      }
      case kOpAddress: {
        const int64_t w = field_word(I(a), I(b), 1);
        if (w < 0) break;
        if (vm.world_locked && I(a) == 0) {
          raise(vm, "assignment to world entity");
          break;
        }
        g[c] = static_cast<uint32_t>(I(a) + I(b) * 4);
        break;
      }
      case kOpStoreF:
      case kOpStoreS:
      case kOpStoreEnt:
      case kOpStoreFld:
      case kOpStoreFnc:
        g[b] = g[a];
        break;
      case kOpStoreV: {
        const uint32_t x = g[a], y = g[a + 1], z = g[a + 2];
        g[b] = x;
        g[b + 1] = y;
        g[b + 2] = z;
        break;
      }
      case kOpStorePF:
      case kOpStorePS:
      case kOpStorePEnt:
      case kOpStorePFld:
      case kOpStorePFnc:
      case kOpStorePV: {
        const int width = s.op == kOpStorePV ? 3 : 1;
        const int32_t ptr = I(b);
        const int64_t words = static_cast<int64_t>(vm.num_entities) * vm.entityfields;
        if (ptr < 0 || (ptr & 3) != 0 || ptr / 4 + width > words) {
          raisef(vm, "bad pointer %lld (outside the %lld words of entities in use)", ptr, words);
          break;
        }
        for (int k = 0; k < width; k++) ents[ptr / 4 + k] = g[a + k];
        break;
      }
      case kOpNotF:
        setF(c, truth(F(a) == 0.0f));
        break;
      case kOpNotV:
        setF(c, truth(F(a) == 0.0f && F(a + 1) == 0.0f && F(a + 2) == 0.0f));
        break;
      case kOpNotS: {
        const char* x = string_of(I(a));
        if (x != nullptr) setF(c, truth(x[0] == 0));
        break;
      }
      case kOpNotEnt:
      case kOpNotFnc:
        setF(c, truth(g[a] == 0));
        break;
      case kOpIf:
        if (g[a] != 0) next = st + b;
        break;
      case kOpIfNot:
        if (g[a] == 0) next = st + b;
        break;
      case kOpCall0:
      case kOpCall1:
      case kOpCall2:
      case kOpCall3:
      case kOpCall4:
      case kOpCall5:
      case kOpCall6:
      case kOpCall7:
      case kOpCall8: {
        const int32_t callee = I(a);
        if (callee == 0) {
          raise(vm, "NULL function");
          break;
        }
        if (callee < 0 || callee >= num_functions) {
          raisef(vm, "bad function %lld (the progs has %lld)", callee, num_functions);
          break;
        }
        const int argc = s.op - kOpCall0;
        const int32_t first = p.functions[static_cast<size_t>(callee)].first_statement;
        if (first < 0) {
          if (!run_builtin(vm, -first, argc)) break;
          if constexpr (kTraced) {
            if (t->builtin != nullptr) t->builtin(t->ctx, vm, -first, argc, vm.depth);
          }
          vm.statement = st;
          break;
        }
        next = enter(vm, callee, st);
        if (next < 0) break;
        if constexpr (kTraced) {
          if (t->enter != nullptr) t->enter(t->ctx, vm, callee, vm.depth);
        }
        counter = &vm.profile[static_cast<size_t>(callee)].statements;
        break;
      }
      case kOpState: {
        const int self = entity_number(vm, I(vm.slots.self));
        if (self < 0) {
          raisef(vm, "STATE: self %lld is not an entity in use", I(vm.slots.self));
          break;
        }
        const float time = F(vm.slots.time);
        *entity_field(vm, self, vm.slots.nextthink) =
            as_word(static_cast<float>(static_cast<double>(time) + 0.1));
        *entity_field(vm, self, vm.slots.frame) = g[a];
        *entity_field(vm, self, vm.slots.think) = g[b];
        break;
      }
      case kOpGoto:
        next = st + a;
        break;
      case kOpAnd:
        setF(c, truth(F(a) != 0.0f && F(b) != 0.0f));
        break;
      case kOpOr:
        setF(c, truth(F(a) != 0.0f || F(b) != 0.0f));
        break;
      case kOpBitAnd:
        setF(c, static_cast<float>(trunc_i32(F(a)) & trunc_i32(F(b))));
        break;
      case kOpBitOr:
        setF(c, static_cast<float>(trunc_i32(F(a)) | trunc_i32(F(b))));
        break;
      default:
        raisef(vm, "bad opcode %lld", s.op);
        break;
    }
    if (vm.fault.set) break;
    if (next < 0 || next >= num_statements) {
      raisef(vm, "statement %lld is past the program's %lld statements", next, num_statements);
      break;
    }
    st = next;
  }
  vm.statements_run += count;
  unwind(vm, entry_depth);
  return false;
}

}  // namespace

void create_vm(Vm& vm, const Progs& progs, const VmLimits& limits) {
  vm.progs = &progs;
  vm.globals = progs.globals;
  vm.entityfields = progs.entityfields;
  vm.entity_bytes = progs.entityfields * 4;
  vm.max_entities = limits.max_entities;
  vm.entities.assign(
      static_cast<size_t>(limits.max_entities) * static_cast<size_t>(progs.entityfields), 0u);
  vm.num_entities = 1;
  vm.world_locked = false;
  vm.builtins.assign(static_cast<size_t>(limits.max_builtins), BuiltinSlot{});
  vm.builtin_calls.assign(vm.builtins.size(), 0);
  vm.depth = 0;
  vm.local_stack.assign(kLocalStackWords, 0u);
  vm.local_used = 0;
  vm.function = 0;
  vm.statement = 0;
  vm.engine_text.assign(static_cast<size_t>(limits.engine_string_bytes), 0);
  vm.engine_offsets.assign(static_cast<size_t>(limits.engine_strings), 0);
  vm.engine_count = 0;
  vm.engine_used = 0;
  vm.temp_next = 0;
  vm.builtin_temps.assign(static_cast<size_t>(kBuiltinTempStrings) * kTempStringBytes, 0);
  vm.builtin_temp_next = 0;
  vm.zone.assign(kZoneStrings, std::string());
  vm.zone_used.assign(kZoneStrings, 0);
  vm.words = Words{};
  vm.profile.assign(progs.functions.size(), FunctionProfile{});
  vm.statements_run = 0;
  vm.fault = Fault{};
}

void reset_globals(Vm& vm) { vm.globals = vm.progs->globals; }

void clear_entities(Vm& vm) { std::fill(vm.entities.begin(), vm.entities.end(), 0u); }

void clear_entity(Vm& vm, int number) {
  std::fill_n(entity_field(vm, number, 0), static_cast<size_t>(vm.entityfields), 0u);
}

void set_builtin(Vm& vm, int number, Builtin fn, const char* name) {
  if (number <= 0 || number >= static_cast<int>(vm.builtins.size())) return;
  vm.builtins[static_cast<size_t>(number)] = {fn, name};
}

bool call_function(Vm& vm, int32_t fn) {
  if (vm.fault.set) return false;
  const int32_t saved_statement = vm.statement;
  if (fn <= 0 || fn >= static_cast<int32_t>(vm.progs->functions.size())) {
    raise(vm, "NULL function");
    return false;
  }
  const Function& f = vm.progs->functions[static_cast<size_t>(fn)];
  bool ok = false;
  if (f.first_statement < 0) {
    ok = run_builtin(vm, -f.first_statement, f.num_parms);
  } else {
    ok = vm.trace != nullptr ? execute<true>(vm, fn) : execute<false>(vm, fn);
  }
  vm.statement = saved_statement;
  return ok;
}

void vm_error(Vm& vm, const char* message) { raise(vm, message); }

void clear_fault(Vm& vm) { vm.fault = Fault{}; }

std::string frame_text(const Vm& vm, const Frame& f) {
  const Progs& p = *vm.progs;
  std::string s;
  if (f.function <= 0 || f.function >= static_cast<int32_t>(p.functions.size())) {
    s = "(the engine)";
  } else {
    const Function& fn = p.functions[static_cast<size_t>(f.function)];
    s = std::string(progs_string(p, fn.name)) + " (" + progs_string(p, fn.file) + ")";
  }
  if (f.statement >= 0) {
    s += " statement " + std::to_string(f.statement);
    if (static_cast<size_t>(f.statement) < p.lines.size())
      s += ", line " + std::to_string(p.lines[static_cast<size_t>(f.statement)]);
  }
  return s;
}

std::string fault_text(const Vm& vm) {
  if (!vm.fault.set) return "";
  std::string s = std::string("QC fault: ") + vm.fault.message;
  for (int i = 0; i < vm.fault.depth; i++)
    s += std::string(i == 0 ? "\n  at " : "\n  from ") + frame_text(vm, vm.fault.frames[i]);
  s += "\n  next: run `ge qc trace` on the program to see the calls before it; skill: progs";
  return s;
}

void clear_profile(Vm& vm) {
  std::fill(vm.profile.begin(), vm.profile.end(), FunctionProfile{});
  std::fill(vm.builtin_calls.begin(), vm.builtin_calls.end(), 0);
}

int entity_number(const Vm& vm, int32_t value) {
  if (vm.entity_bytes <= 0 || value < 0 || value % vm.entity_bytes != 0) return -1;
  const int32_t n = value / vm.entity_bytes;
  return n < vm.num_entities ? n : -1;
}

const char* vm_string(const Vm& vm, int32_t value) {
  if (value >= 0) {
    return static_cast<size_t>(value) < vm.progs->strings.size() ? vm.progs->strings.data() + value
                                                                 : nullptr;
  }
  if (value > -kTempBase) {
    const int32_t k = -value - 1;
    return k < vm.engine_count ? vm.engine_text.data() + vm.engine_offsets[static_cast<size_t>(k)]
                               : nullptr;
  }
  if (value <= -kZoneBase) {
    const int64_t z = -static_cast<int64_t>(value) - kZoneBase;
    if (z >= kZoneStrings) return nullptr;
    // A freed slot reads as empty: a program may keep a value it unzoned, and reading it must
    // not fault.
    return vm.zone_used[static_cast<size_t>(z)] != 0 ? vm.zone[static_cast<size_t>(z)].c_str() : "";
  }
  const int64_t slot = -static_cast<int64_t>(value) - kTempBase;
  if (slot < kTempStrings) return vm.temps[slot];
  const int64_t b = -static_cast<int64_t>(value) - kBuiltinTempBase;
  if (b >= 0 && b < kBuiltinTempStrings)
    return vm.builtin_temps.data() + static_cast<size_t>(b) * kTempStringBytes;
  return nullptr;
}

int32_t engine_string(Vm& vm, const char* text) {
  const size_t n = std::strlen(text) + 1;
  if (vm.engine_count >= static_cast<int>(vm.engine_offsets.size()) ||
      static_cast<size_t>(vm.engine_used) + n > vm.engine_text.size()) {
    raise(vm, "the engine's string store is full (VmLimits::engine_strings, engine_string_bytes)");
    return 0;
  }
  std::memcpy(vm.engine_text.data() + vm.engine_used, text, n);
  vm.engine_offsets[static_cast<size_t>(vm.engine_count)] = vm.engine_used;
  vm.engine_used += static_cast<int>(n);
  return -(vm.engine_count++) - 1;
}

void clear_engine_strings(Vm& vm) {
  vm.engine_count = 0;
  vm.engine_used = 0;
}

int32_t temp_string(Vm& vm, const char* text) {
  const int slot = vm.temp_next;
  vm.temp_next = (vm.temp_next + 1) % kTempStrings;
  std::snprintf(vm.temps[slot], kTempStringBytes, "%s", text);
  return -(kTempBase + slot);
}

int32_t builtin_temp_string(Vm& vm, const char* text) {
  const int slot = vm.builtin_temp_next;
  vm.builtin_temp_next = (vm.builtin_temp_next + 1) % kBuiltinTempStrings;
  std::snprintf(vm.builtin_temps.data() + static_cast<size_t>(slot) * kTempStringBytes,
                kTempStringBytes, "%s", text);
  return -(kBuiltinTempBase + slot);
}

int32_t zone_string(Vm& vm, const char* text) {
  for (int i = 0; i < kZoneStrings; i++) {
    if (vm.zone_used[static_cast<size_t>(i)] != 0) continue;
    vm.zone_used[static_cast<size_t>(i)] = 1;
    vm.zone[static_cast<size_t>(i)].assign(text);
    return -(kZoneBase + i);
  }
  raise(vm, "out of string memory (all 1024 zone strings are in use; strunzone what you keep)");
  return 0;
}

bool unzone_string(Vm& vm, int32_t value) {
  if (value > -kZoneBase) return false;
  const int64_t z = -static_cast<int64_t>(value) - kZoneBase;
  if (z >= kZoneStrings) return false;
  vm.zone_used[static_cast<size_t>(z)] = 0;
  return true;
}

void clear_zone_strings(Vm& vm) { std::fill(vm.zone_used.begin(), vm.zone_used.end(), uint8_t{0}); }

}  // namespace ge::progs
