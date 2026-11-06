// The compiled-module VM: the header and segment checks, the decode, the load-time rules that
// measure each function's operand depth, the memory image, the interpreter loop with its checked
// loads and stores, system calls and re-entry, and the fault with its frames (qvm.hpp says the
// contract).
//
// ge:summary The compiled-module VM's loader, verifier and interpreter loop (qvm.hpp's contract)
// ge:keywords qvm load_qvm verifier static-depth interpreter syscall reentry crc16 fault frames
// ge:skill progs
// ge:test tests/progs_qvm_test.cpp
#include "progs/qvm.hpp"

#include <array>
#include <cstdio>
#include <cstring>

namespace ge::progs {

namespace {

constexpr int32_t kMaxMemory = 1 << 28;  // 256 MiB: the largest block a module may ask for
constexpr int kStaticLimit = 30;         // words the static depth must stay under
constexpr int kFrameLimit = 65536;       // bytes an ENTER may take

const char* const kNames[kQvmOps] = {
    "UNDEF",  "IGNORE", "BREAK",  "ENTER", "LEAVE",      "CALL", "PUSH",  "POP",   "CONST", "LOCAL",
    "JUMP",   "EQ",     "NE",     "LTI",   "LEI",        "GTI",  "GEI",   "LTU",   "LEU",   "GTU",
    "GEU",    "EQF",    "NEF",    "LTF",   "LEF",        "GTF",  "GEF",   "LOAD1", "LOAD2", "LOAD4",
    "STORE1", "STORE2", "STORE4", "ARG",   "BLOCK_COPY", "SEX8", "SEX16", "NEGI",  "ADD",   "SUB",
    "DIVI",   "DIVU",   "MODI",   "MODU",  "MULI",       "MULU", "BAND",  "BOR",   "BXOR",  "BCOM",
    "LSH",    "RSHI",   "RSHU",   "NEGF",  "ADDF",       "SUBF", "DIVF",  "MULF",  "CVIF",  "CVFI",
};

// Bytes of operand an opcode carries.
int operand_bytes(int op) {
  switch (op) {
    case kOpEnter:
    case kOpLeave:
    case kOpConst:
    case kOpLocal:
    case kOpBlockCopy:
      return 4;
    case kOpArg:
      return 1;
    default:
      return (op >= kOpEq && op <= kOpGef) ? 4 : 0;
  }
}

// The opcode's static effect on the operand stack, words.
int static_effect(int op) {
  if (op >= kOpEq && op <= kOpGef) return -2;
  switch (op) {
    case kOpLeave:
    case kOpPop:
    case kOpJump:
    case kOpArg:
      return -1;
    case kOpPush:
    case kOpConst:
    case kOpLocal:
      return 1;
    case kOpStore1:
    case kOpStore2:
    case kOpStore4:
    case kOpBlockCopy:
      return -2;
    case kOpAdd:
    case kOpSub:
    case kOpDivi:
    case kOpDivu:
    case kOpModi:
    case kOpModu:
    case kOpMuli:
    case kOpMulu:
    case kOpBand:
    case kOpBor:
    case kOpBxor:
    case kOpLsh:
    case kOpRshi:
    case kOpRshu:
    case kOpAddf:
    case kOpSubf:
    case kOpDivf:
    case kOpMulf:
      return -1;
    default:
      return 0;
  }
}

int32_t read_le(const uint8_t* p) {
  return static_cast<int32_t>(uint32_t{p[0]} | uint32_t{p[1]} << 8 | uint32_t{p[2]} << 16 |
                              uint32_t{p[3]} << 24);
}

uint16_t crc16(std::span<const uint8_t> bytes) {
  uint16_t crc = 0xFFFF;
  for (uint8_t b : bytes) {
    crc = static_cast<uint16_t>(crc ^ (b << 8));
    for (int i = 0; i < 8; ++i)
      crc = static_cast<uint16_t>((crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1);
  }
  return crc;
}

std::string at(int i, const char* rule, long long value) {
  char buf[200];
  std::snprintf(buf, sizeof buf, "instruction %d: %s (value %lld)", i, rule, value);
  return buf;
}

struct Header {
  int32_t magic, count, code_ofs, code_len, data_ofs, data_len, lit_len, bss_len, jt_len;
};

bool read_header(std::span<const uint8_t> file, Header& h, std::string& error) {
  if (file.size() < 32) return error = "truncated image header (under 32 bytes)", false;
  int32_t w[9] = {};
  for (size_t i = 0; i < 8; ++i) w[i] = read_le(file.data() + 4 * i);
  h = {w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7], 0};
  if (h.magic != static_cast<int32_t>(kQvmMagic1) && h.magic != static_cast<int32_t>(kQvmMagic2))
    return error = "bad file magic (not a compiled module)", false;
  if (h.magic == static_cast<int32_t>(kQvmMagic2)) {
    if (file.size() < 36) return error = "truncated image header (version 2 needs 36 bytes)", false;
    h.jt_len = read_le(file.data() + 32);
  }
  const long long size = static_cast<long long>(file.size());
  if (h.count <= 0 || h.code_ofs < 0 || h.code_len <= 0 || h.data_ofs < 0 || h.data_len < 0 ||
      h.lit_len < 0 || h.bss_len < 0 || h.jt_len < 0)
    return error = "bad header: a negative or empty segment", false;
  if (h.code_ofs >= size || static_cast<long long>(h.code_ofs) + h.code_len > size)
    return error = "bad code segment: it runs past the file's end", false;
  if (static_cast<long long>(h.code_ofs) + h.code_len != h.data_ofs)
    return error = "bad data segment: it does not start where the code ends", false;
  if (static_cast<long long>(h.data_ofs) + h.data_len + h.lit_len + h.jt_len != size)
    return error = "bad lit segment length: the segments do not end at the file's end", false;
  if (h.data_len % 4 != 0) return error = "bad data segment: not a whole number of words", false;
  return true;
}

// Decodes the code into `vm.code`.
bool decode(std::span<const uint8_t> code, int32_t count, Qvm& vm, std::string& error) {
  vm.code.assign(static_cast<size_t>(count), QvmInstr{});
  size_t p = 0;
  for (int32_t i = 0; i < count; ++i) {
    if (p >= code.size()) return error = at(i, "the code runs past its segment", count), false;
    const int op = code[p++];
    if (op >= kQvmOps) return error = at(i, "bad opcode", op), false;
    const int n = operand_bytes(op);
    if (p + n > code.size()) return error = at(i, "an operand runs past the code", op), false;
    QvmInstr& in = vm.code[static_cast<size_t>(i)];
    in.op = static_cast<uint8_t>(op);
    if (n == 4) in.v = read_le(code.data() + p);
    if (n == 1) in.v = code[p];
    p += n;
  }
  return true;
}

// The load-time rules (qvm.hpp's header). Fills each instruction's function and static depth, and
// each ENTER's deepest depth.
bool verify(Qvm& vm, std::string& error) {
  const int32_t count = static_cast<int32_t>(vm.code.size());
  std::vector<int32_t> fn_end(vm.code.size(), -1);
  // Pass 1: functions, static depths, the rules that need only the instruction itself.
  int depth = 0;
  int32_t fn = -1, frame = 0;
  for (int32_t i = 0; i < count; ++i) {
    QvmInstr& in = vm.code[static_cast<size_t>(i)];
    if (in.op == kOpEnter) {
      if (fn >= 0) return error = at(i, "ENTER inside another function", fn), false;
      if (depth != 0) return error = at(i, "ENTER at static depth other than 0", depth), false;
      if (in.v < 0 || in.v >= kFrameLimit || in.v % 4 != 0)
        return error = at(i, "ENTER's frame is not a multiple of 4 under 64 KiB", in.v), false;
      fn = i;
      frame = in.v;
    }
    if (fn < 0) return error = at(i, "an instruction outside every function", in.op), false;
    in.fn = fn;
    in.aux = depth;
    switch (in.op) {
      case kOpLeave:
        if (in.v != frame)
          return error = at(i, "LEAVE's frame differs from its ENTER's", in.v), false;
        if (depth != 1)
          return error = at(i, "LEAVE at static depth other than 1 word", depth), false;
        break;
      case kOpJump:
        if (depth != 1)
          return error = at(i, "JUMP at static depth other than 1 word", depth), false;
        break;
      case kOpCall:
        if (depth < 1) return error = at(i, "CALL with no target on the stack", depth), false;
        break;
      case kOpArg:
        if (in.v < 8 || in.v % 4 != 0 || in.v > frame - 4)
          return error = at(i, "ARG outside the frame's outgoing slots", in.v), false;
        break;
      case kOpLocal:
        if (i + 1 < count) {
          const int next = vm.code[static_cast<size_t>(i) + 1].op;
          if ((next == kOpLoad1 || next == kOpLoad2 || next == kOpLoad4 || next == kOpArg) &&
              (in.v < 8 || in.v >= frame + 256))
            return error = at(i, "LOCAL read outside the frame and its parameters", in.v), false;
        }
        break;
      case kOpBlockCopy:
        if (static_cast<uint32_t>(in.v) >= vm.exact)
          return error = at(i, "BLOCK_COPY of more bytes than the image holds", in.v), false;
        break;
      case kOpLoad1:
      case kOpLoad2:
      case kOpLoad4:
        if (i > 0 && vm.code[static_cast<size_t>(i) - 1].op == kOpConst) {
          const uint32_t size = in.op == kOpLoad1 ? 1 : in.op == kOpLoad2 ? 2 : 4;
          const auto addr = static_cast<uint32_t>(vm.code[static_cast<size_t>(i) - 1].v);
          if (addr > vm.exact || vm.exact - addr < size)
            return error = at(i, "a constant load outside the image", static_cast<int32_t>(addr)),
                   false;
        }
        break;
      default:
        if (in.op >= kOpEq && in.op <= kOpGef && depth != 2)
          return error = at(i, "a compare at static depth other than 2 words", depth), false;
    }
    depth += static_effect(in.op);
    if (depth < 0) return error = at(i, "the static operand depth goes below 0", depth), false;
    if (depth >= kStaticLimit)
      return error = at(i, "the static operand depth reaches 30 words", depth), false;
    if (vm.code[static_cast<size_t>(fn)].aux < depth) vm.code[static_cast<size_t>(fn)].aux = depth;
    if (in.op == kOpLeave && i > 0 && vm.code[static_cast<size_t>(i) - 1].op == kOpPush) {
      for (int32_t k = fn; k <= i; ++k) fn_end[static_cast<size_t>(k)] = i;
      fn = -1;
    }
  }
  if (fn >= 0) return error = at(fn, "a function with no PUSH LEAVE end", fn), false;
  if (vm.code.back().op != kOpLeave)
    return error = at(count - 1, "the last instruction is not LEAVE", vm.code.back().op), false;
  if (vm.code[0].op != kOpEnter)
    return error = at(0, "instruction 0 (the entry) is not ENTER", vm.code[0].op), false;
  // Pass 2: targets.
  auto inside = [&](int32_t i, int32_t t) {
    const int32_t f = vm.code[static_cast<size_t>(i)].fn;
    return t >= f && t <= fn_end[static_cast<size_t>(f)];
  };
  for (int32_t i = 0; i < count; ++i) {
    const QvmInstr& in = vm.code[static_cast<size_t>(i)];
    if (in.op >= kOpEq && in.op <= kOpGef) {
      if (!inside(i, in.v) || vm.code[static_cast<size_t>(in.v)].aux != 0)
        return error = at(i, "a branch target outside its function or not at depth 0", in.v), false;
    }
    if (i == 0 || vm.code[static_cast<size_t>(i) - 1].op != kOpConst) continue;
    const int32_t t = vm.code[static_cast<size_t>(i) - 1].v;
    if (in.op == kOpJump) {
      if (!inside(i, t) || vm.code[static_cast<size_t>(t)].aux != 0 ||
          vm.code[static_cast<size_t>(t)].op == kOpEnter || t == i - 1)
        return error = at(i, "a JUMP target outside its function, not at depth 0, or itself", t),
               false;
    }
    if (in.op == kOpCall && t >= 0) {
      if (t >= count || vm.code[static_cast<size_t>(t)].op != kOpEnter || t == 0)
        return error = at(i, "a CALL target that is not a function's ENTER (or is the entry)", t),
               false;
    }
  }
  return true;
}

// The fault, placed at `pc` with the frames above it.
void fail(Qvm& vm, int32_t pc, int32_t ps, const std::string& why) {
  if (vm.fault_pc >= 0) return;  // placed already: the first fault names the cause
  const auto count = static_cast<int32_t>(vm.code.size());
  std::string text = vm.fault.empty() ? why : vm.fault;
  if (pc >= 0 && pc < count) {
    const QvmInstr& in = vm.code[static_cast<size_t>(pc)];
    char buf[160];
    std::snprintf(buf, sizeof buf, " at instruction %d (%s) in function %d; frames:", pc,
                  qvm_op_name(in.op), in.fn);
    text += buf;
    int32_t f = in.fn;
    const size_t bare = text.size();
    for (int n = 0; n < 16; ++n) {
      const int32_t frame = vm.code[static_cast<size_t>(f)].v;
      const int64_t slot = static_cast<int64_t>(ps) + frame;
      if (slot < 0 || slot + 4 > static_cast<int64_t>(vm.mem.size())) break;
      const int32_t ret = qvm_word(vm, static_cast<int32_t>(slot));
      if (ret < 0 || ret >= count) break;
      f = vm.code[static_cast<size_t>(ret)].fn;
      ps = static_cast<int32_t>(slot);
      std::snprintf(buf, sizeof buf, " %d (returning to %d)", f, ret);
      text += buf;
    }
    if (text.size() == bare) text += " none (the host called it)";
  }
  vm.fault = text;
  vm.fault_pc = pc < 0 ? 0 : pc;
}

}  // namespace

const char* qvm_op_name(int op) { return op >= 0 && op < kQvmOps ? kNames[op] : "?"; }

bool reset_qvm(std::span<const uint8_t> file, Qvm& vm, std::string& error) {
  Header h{};
  if (!read_header(file, h, error)) return false;
  const int64_t exact = int64_t{h.data_len} + h.lit_len + h.bss_len;
  int64_t size = 1;
  while (size < exact + kQvmMargin) size <<= 1;
  if (size > kMaxMemory) return error = "the module asks for more than 256 MiB of memory", false;
  vm.exact = static_cast<uint32_t>(exact);
  vm.data_len = static_cast<uint32_t>(h.data_len);
  vm.lit_len = static_cast<uint32_t>(h.lit_len);
  vm.bss_len = static_cast<uint32_t>(h.bss_len);
  vm.mem.assign(static_cast<size_t>(size), 0);
  // The data words are little endian, as is every host the engine builds for.
  std::memcpy(vm.mem.data(), file.data() + h.data_ofs, static_cast<size_t>(h.data_len) + h.lit_len);
  vm.start_stack = static_cast<int32_t>(size);
  vm.ps = vm.start_stack;
  vm.depth = 0;
  vm.fault.clear();
  vm.fault_pc = -1;
  return true;
}

bool load_qvm(std::span<const uint8_t> file, Qvm& vm, std::string& error) {
  Header h{};
  if (!read_header(file, h, error)) return false;
  vm.version = h.magic == static_cast<int32_t>(kQvmMagic2) ? 2 : 1;
  if (!reset_qvm(file, vm, error)) return false;
  if (!decode(file.subspan(static_cast<size_t>(h.code_ofs), static_cast<size_t>(h.code_len)),
              h.count, vm, error) ||
      !verify(vm, error)) {
    vm.code.clear();
    return false;
  }
  vm.crc = crc16(file);
  vm.instructions = 0;
  return true;
}

void qvm_raise(Qvm& vm, const std::string& why) {
  if (vm.fault.empty()) vm.fault = why;
}

void qvm_clear_fault(Qvm& vm) {
  vm.fault.clear();
  vm.fault_pc = -1;
}

int32_t qvm_word(const Qvm& vm, int32_t addr) {
  int32_t w;
  std::memcpy(&w, vm.mem.data() + static_cast<uint32_t>(addr), 4);
  return w;
}

void qvm_set_word(Qvm& vm, int32_t addr, int32_t value) {
  std::memcpy(vm.mem.data() + static_cast<uint32_t>(addr), &value, 4);
}

uint8_t* qvm_ptr(Qvm& vm, int32_t addr, uint32_t size, const char* what) {
  const auto a = static_cast<uint32_t>(addr);
  if (a > vm.mem.size() || vm.mem.size() - a < size) {
    char buf[200];
    std::snprintf(buf, sizeof buf, "%s: %u bytes at address %d are outside the module's %zu bytes",
                  what, size, addr, vm.mem.size());
    qvm_raise(vm, buf);
    return nullptr;
  }
  return vm.mem.data() + a;
}

std::string_view qvm_string(Qvm& vm, int32_t addr, const char* what) {
  if (addr == 0) return {};
  const auto a = static_cast<uint32_t>(addr);
  if (a < vm.mem.size()) {
    const void* end = std::memchr(vm.mem.data() + a, 0, vm.mem.size() - a);
    if (end != nullptr)
      return {reinterpret_cast<const char*>(vm.mem.data() + a),
              static_cast<size_t>(static_cast<const uint8_t*>(end) - (vm.mem.data() + a))};
  }
  char buf[200];
  std::snprintf(buf, sizeof buf, "%s: the text at address %d runs off the module's memory", what,
                addr);
  qvm_raise(vm, buf);
  return {};
}

bool qvm_put_string(Qvm& vm, int32_t addr, int32_t size, std::string_view text, const char* what) {
  if (size <= 0) return true;
  uint8_t* p = qvm_ptr(vm, addr, static_cast<uint32_t>(size), what);
  if (p == nullptr) return false;
  const size_t n =
      text.size() < static_cast<size_t>(size) - 1 ? text.size() : static_cast<size_t>(size) - 1;
  std::memcpy(p, text.data(), n);
  p[n] = 0;
  return true;
}

int32_t qvm_call(Qvm& vm, int32_t command, int32_t a0, int32_t a1, int32_t a2) {
  if (!vm.fault.empty() || vm.code.empty()) return 0;
  const auto memsize = static_cast<int64_t>(vm.mem.size());
  const int64_t bottom = memsize - kQvmProgramStack - kQvmMargin;
  const int32_t saved_start = vm.start_stack;
  int32_t ps = vm.start_stack - 24;
  if (ps <= 0 || ps <= bottom) {
    fail(vm, -1, ps, "program stack overflow (a host call nested too deep)");
    return 0;
  }
  const int32_t block[6] = {-1, 0, command, a0, a1, a2};
  std::memcpy(vm.mem.data() + ps, block, sizeof block);
  ++vm.depth;

  std::array<int32_t, kQvmOperandStack + 4> stack;  // the 4 spare words: PUSH LEAVE's slack
  int sp = 0;                                       // words on the operand stack
  int32_t* const s = stack.data();
  uint8_t* const mem = vm.mem.data();
  const auto mem_size = static_cast<uint32_t>(vm.mem.size());
  const QvmInstr* const code = vm.code.data();
  const auto count = static_cast<int32_t>(vm.code.size());
  int32_t pc = 0;
  uint64_t ran = 0;
  int32_t result = 0;

  // A checked address: false (and the fault) when `size` bytes at `addr` leave the memory.
  auto check = [&](int32_t addr, uint32_t size, int32_t at_pc) {
    const auto a = static_cast<uint32_t>(addr);
    if (a <= mem_size && mem_size - a >= size) return true;
    char buf[160];
    std::snprintf(buf, sizeof buf, "%u bytes at address %d are outside the module's memory", size,
                  addr);
    fail(vm, at_pc, ps, buf);
    return false;
  };

  for (;;) {
    const int32_t here = pc;
    const QvmInstr& in = code[pc++];
    ++ran;
    switch (in.op) {
      case kOpUndef:
      case kOpIgnore:
      case kOpBreak:
        break;
      case kOpEnter: {
        const int64_t next = static_cast<int64_t>(ps) - in.v;
        if (next <= bottom || next <= 0) {
          fail(vm, here, static_cast<int32_t>(next), "program stack overflow");
          goto done;
        }
        if (sp + in.aux >= kQvmOperandStack - 1) {
          fail(vm, here, static_cast<int32_t>(next), "operand stack overflow");
          goto done;
        }
        ps = static_cast<int32_t>(next);
        break;
      }
      case kOpLeave: {
        ps += in.v;
        if (!check(ps, 4, here)) goto done;
        const int32_t ret = qvm_word(vm, ps);
        if (ret == -1) {
          if (sp != 1) {
            char buf[96];
            std::snprintf(buf, sizeof buf, "the call ended with %d operand words, not 1", sp);
            fail(vm, here, ps, buf);
            goto done;
          }
          result = s[0];
          goto done;
        }
        if (ret < 0 || ret >= count) {
          fail(vm, here, ps, "program counter out of range in LEAVE");
          goto done;
        }
        pc = ret;
        break;
      }
      case kOpCall: {
        const int32_t target = s[sp - 1];
        qvm_set_word(vm, ps, pc);
        if (target < 0) {
          int32_t args[kQvmSyscallArgs + 1] = {};
          const int32_t number = -target - 1;
          qvm_set_word(vm, ps + 4, number);
          for (int k = 0; k <= kQvmSyscallArgs; ++k) {
            const int64_t a = int64_t{ps} + 4 + int64_t{4} * k;
            if (a + 4 > static_cast<int64_t>(mem_size)) break;
            args[k] = qvm_word(vm, static_cast<int32_t>(a));
          }
          const int32_t outer_start = vm.start_stack;
          vm.start_stack = ps - 8;
          vm.ps = ps;
          vm.instructions += ran;
          ran = 0;
          const int32_t r = vm.syscall != nullptr ? vm.syscall(vm.user, vm, args) : 0;
          vm.start_stack = outer_start;
          if (vm.syscall == nullptr) qvm_raise(vm, "a system call with no host to answer it");
          if (!vm.fault.empty()) {
            char buf[64];
            std::snprintf(buf, sizeof buf, " (in system call %d)", number);
            if (vm.fault_pc < 0) vm.fault += buf;
            fail(vm, here, ps, vm.fault);
            goto done;
          }
          s[sp - 1] = r;
          break;
        }
        if (target >= count || code[target].op != kOpEnter) {
          fail(vm, here, ps, "a CALL to an address that is not a function");
          goto done;
        }
        --sp;
        pc = target;
        break;
      }
      case kOpPush:
        s[sp++] = 0;
        break;
      case kOpPop:
        --sp;
        break;
      case kOpConst:
        s[sp++] = in.v;
        break;
      case kOpLocal:
        s[sp++] = ps + in.v;
        break;
      case kOpJump: {
        const int32_t t = s[--sp];
        if (t < 0 || t >= count || code[t].fn != in.fn || code[t].aux != 0 ||
            code[t].op == kOpEnter) {
          fail(vm, here, ps, "a JUMP outside its function or into the middle of an expression");
          goto done;
        }
        pc = t;
        break;
      }
#define GE_QVM_BRANCH(OP, TYPE, CMP) \
  case OP: {                         \
    sp -= 2;                         \
    TYPE n, t;                       \
    std::memcpy(&n, &s[sp], 4);      \
    std::memcpy(&t, &s[sp + 1], 4);  \
    if (n CMP t) pc = in.v;          \
    break;                           \
  }
        GE_QVM_BRANCH(kOpEq, int32_t, ==)
        GE_QVM_BRANCH(kOpNe, int32_t, !=)
        GE_QVM_BRANCH(kOpLti, int32_t, <)
        GE_QVM_BRANCH(kOpLei, int32_t, <=)
        GE_QVM_BRANCH(kOpGti, int32_t, >)
        GE_QVM_BRANCH(kOpGei, int32_t, >=)
        GE_QVM_BRANCH(kOpLtu, uint32_t, <)
        GE_QVM_BRANCH(kOpLeu, uint32_t, <=)
        GE_QVM_BRANCH(kOpGtu, uint32_t, >)
        GE_QVM_BRANCH(kOpGeu, uint32_t, >=)
        GE_QVM_BRANCH(kOpEqf, float, ==)
        GE_QVM_BRANCH(kOpNef, float, !=)
        GE_QVM_BRANCH(kOpLtf, float, <)
        GE_QVM_BRANCH(kOpLef, float, <=)
        GE_QVM_BRANCH(kOpGtf, float, >)
        GE_QVM_BRANCH(kOpGef, float, >=)
#undef GE_QVM_BRANCH
      case kOpLoad1:
        if (!check(s[sp - 1], 1, here)) goto done;
        s[sp - 1] = mem[static_cast<uint32_t>(s[sp - 1])];
        break;
      case kOpLoad2: {
        if (!check(s[sp - 1], 2, here)) goto done;
        uint16_t v;
        std::memcpy(&v, mem + static_cast<uint32_t>(s[sp - 1]), 2);
        s[sp - 1] = v;
        break;
      }
      case kOpLoad4:
        if (!check(s[sp - 1], 4, here)) goto done;
        s[sp - 1] = qvm_word(vm, s[sp - 1]);
        break;
      case kOpStore1:
        sp -= 2;
        if (!check(s[sp], 1, here)) goto done;
        mem[static_cast<uint32_t>(s[sp])] = static_cast<uint8_t>(s[sp + 1]);
        break;
      case kOpStore2: {
        sp -= 2;
        if (!check(s[sp], 2, here)) goto done;
        const auto v = static_cast<uint16_t>(s[sp + 1]);
        std::memcpy(mem + static_cast<uint32_t>(s[sp]), &v, 2);
        break;
      }
      case kOpStore4:
        sp -= 2;
        if (!check(s[sp], 4, here)) goto done;
        qvm_set_word(vm, s[sp], s[sp + 1]);
        break;
      case kOpArg:
        qvm_set_word(vm, ps + in.v, s[--sp]);
        break;
      case kOpBlockCopy: {
        sp -= 2;
        const auto n = static_cast<uint32_t>(in.v);
        if (!check(s[sp], n, here) || !check(s[sp + 1], n, here)) goto done;
        std::memmove(mem + static_cast<uint32_t>(s[sp]), mem + static_cast<uint32_t>(s[sp + 1]), n);
        break;
      }
      case kOpSex8:
        s[sp - 1] = static_cast<int32_t>(static_cast<uint32_t>(s[sp - 1]) << 24) >> 24;
        break;
      case kOpSex16:
        s[sp - 1] = static_cast<int32_t>(static_cast<uint32_t>(s[sp - 1]) << 16) >> 16;
        break;
      case kOpNegi:
        s[sp - 1] = static_cast<int32_t>(0u - static_cast<uint32_t>(s[sp - 1]));
        break;
#define GE_QVM_UINT(OP, EXPR)                        \
  case OP: {                                         \
    --sp;                                            \
    const auto n = static_cast<uint32_t>(s[sp - 1]); \
    const auto t = static_cast<uint32_t>(s[sp]);     \
    s[sp - 1] = static_cast<int32_t>(EXPR);          \
    break;                                           \
  }
        GE_QVM_UINT(kOpAdd, n + t)
        GE_QVM_UINT(kOpSub, n - t)
        GE_QVM_UINT(kOpMuli, n * t)
        GE_QVM_UINT(kOpMulu, n * t)
        GE_QVM_UINT(kOpBand, n & t)
        GE_QVM_UINT(kOpBor, n | t)
        GE_QVM_UINT(kOpBxor, n ^ t)
        GE_QVM_UINT(kOpLsh, n << (t & 31))
        GE_QVM_UINT(kOpRshu, n >> (t & 31))
#undef GE_QVM_UINT
      case kOpRshi: {
        --sp;
        // An arithmetic shift: C++20 defines >> on a negative signed value as sign-filling.
        s[sp - 1] = s[sp - 1] >> (s[sp] & 31);
        break;
      }
      case kOpDivi:
      case kOpModi:
      case kOpDivu:
      case kOpModu: {
        --sp;
        const int32_t n = s[sp - 1], t = s[sp];
        if (t == 0) {
          fail(vm, here, ps, "division by zero");
          goto done;
        }
        if ((in.op == kOpDivi || in.op == kOpModi) && n == INT32_MIN && t == -1) {
          fail(vm, here, ps, "division overflow");
          goto done;
        }
        const auto un = static_cast<uint32_t>(n), ut = static_cast<uint32_t>(t);
        s[sp - 1] = in.op == kOpDivi   ? n / t
                    : in.op == kOpModi ? n % t
                    : in.op == kOpDivu ? static_cast<int32_t>(un / ut)
                                       : static_cast<int32_t>(un % ut);
        break;
      }
      case kOpBcom:
        s[sp - 1] = ~s[sp - 1];
        break;
      case kOpNegf:
        s[sp - 1] = static_cast<int32_t>(static_cast<uint32_t>(s[sp - 1]) ^ 0x80000000u);
        break;
#define GE_QVM_FLOAT(OP, CALC)            \
  case OP: {                              \
    --sp;                                 \
    const float n = qvm_float(s[sp - 1]); \
    const float t = qvm_float(s[sp]);     \
    s[sp - 1] = qvm_bits(CALC);           \
    break;                                \
  }
        GE_QVM_FLOAT(kOpAddf, n + t)
        GE_QVM_FLOAT(kOpSubf, n - t)
        GE_QVM_FLOAT(kOpMulf, n * t)
        GE_QVM_FLOAT(kOpDivf, n / t)
#undef GE_QVM_FLOAT
      case kOpCvif:
        s[sp - 1] = qvm_bits(static_cast<float>(s[sp - 1]));
        break;
      case kOpCvfi: {
        const float f = qvm_float(s[sp - 1]);
        // In range: -2^31 <= f < 2^31 (both exact in a float); NaN fails both compares.
        s[sp - 1] =
            (f >= -2147483648.0f && f < 2147483648.0f) ? static_cast<int32_t>(f) : INT32_MIN;
        break;
      }
      default:
        fail(vm, here, ps, "an opcode the loader should have refused");
        goto done;
    }
  }
done:
  vm.instructions += ran;
  vm.start_stack = saved_start;
  --vm.depth;
  return vm.fault.empty() ? result : 0;
}

}  // namespace ge::progs
