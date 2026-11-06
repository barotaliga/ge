// Tests: the compiled-module VM. Hand-assembled modules run: the entry reads its command and
// arguments, a function call passes a parameter, a system call reads the caller's outgoing
// argument and its result replaces the address, the host calls the module again from inside a
// system call and the outer call resumes; every integer and float opcode at its edges (shifts
// masked, CVFI of NaN and out of range, wrapping, sign extension); the data and lit image; every
// guard faults with its frames (division by zero and overflow, a load or store outside memory,
// the program stack's overflow by recursion, a computed jump into another function, a fault raised
// by the host); and the loader refuses each rule it states. With GE_QVM_FILE set to a real module
// (a match mod's qwprogs.qvm) it loads that too and prints its size, memory and CRC.
//
// ge:summary Tests: the compiled-module VM's loader, load-time rules, opcodes, calls, faults
// ge:keywords test qvm compiled-module syscall reentry verifier fault opcode cvfi shifts crc16
// ge:skill progs testing
// ge:test tests/progs_qvm_test.cpp
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

#include "check.hpp"
#include "progs/qvm.hpp"
#include "progs/qvm_asm.hpp"

using namespace ge::progs;

namespace {

using Module = QvmAsm;

// The entry `ENTER 8; <body>; LEAVE 8; PUSH; LEAVE 8`: the body leaves one word. The command is at
// LOCAL 16, the arguments at LOCAL 20, 24, 28.
Module entry(void (*body)(Module&)) {
  Module m;
  m.op(kOpEnter, 8);
  body(m);
  m.op(kOpLeave, 8).op(kOpPush).op(kOpLeave, 8);
  return m;
}

bool load(const Module& m, Qvm& vm) {
  std::string error;
  const bool ok = load_qvm(m.file(), vm, error);
  if (!ok) std::fprintf(stderr, "load refused: %s\n", error.c_str());
  return ok;
}

std::string refusal(const Module& m) {
  Qvm vm;
  std::string error;
  return load_qvm(m.file(), vm, error) ? std::string() : error;
}

// A module that applies a binary opcode to arguments 0 and 1.
int32_t binop(QvmOp o, int32_t a, int32_t b, std::string* fault = nullptr) {
  static QvmOp the_op;
  the_op = o;
  Module m = entry(
      [](Module& x) { x.op(kOpLocal, 20).op(kOpLoad4).op(kOpLocal, 24).op(kOpLoad4).op(the_op); });
  Qvm vm;
  CHECK(load(m, vm));
  const int32_t r = qvm_call(vm, 0, a, b);
  if (fault != nullptr) *fault = vm.fault;
  return r;
}

int32_t unop(QvmOp o, int32_t a) {
  static QvmOp the_op;
  the_op = o;
  Module m = entry([](Module& x) { x.op(kOpLocal, 20).op(kOpLoad4).op(the_op); });
  Qvm vm;
  CHECK(load(m, vm));
  return qvm_call(vm, 0, a);
}

int32_t fbits(float f) { return qvm_bits(f); }

int32_t host(void* user, Qvm& vm, const int32_t* args) {
  auto* log = static_cast<std::vector<int32_t>*>(user);
  log->push_back(args[0]);
  if (args[0] == 0) return args[1] * 10;
  if (args[0] == 1) return qvm_call(vm, 7);  // the host calls the module back
  if (args[0] == 2) qvm_raise(vm, "the host refused entity 99");
  return 0;
}

}  // namespace

TEST(the_entry_reads_its_command_and_arguments) {
  Module m = entry([](Module& x) {
    x.op(kOpLocal, 16).op(kOpLoad4).op(kOpLocal, 20).op(kOpLoad4).op(kOpAdd);
    x.op(kOpLocal, 28).op(kOpLoad4).op(kOpSub);
  });
  Qvm vm;
  CHECK(load(m, vm));
  CHECK(qvm_call(vm, 40, 5, 0, 3) == 42);
  CHECK(vm.fault.empty());
  CHECK(vm.mem.size() == 32768);  // the smallest power of two over the empty image's margin
  CHECK(vm.start_stack == 32768);
  CHECK(vm.code[0].aux == 2);  // the deepest operand stack the entry reaches
}

TEST(a_call_passes_a_parameter_and_a_system_call_its_argument) {
  Module m;
  // entry: f(5)
  m.op(kOpEnter, 16).op(kOpConst, 5).op(kOpArg, 8);
  const int32_t call_f = m.here();
  m.op(kOpConst, 0).op(kOpCall).op(kOpLeave, 16).op(kOpPush).op(kOpLeave, 16);
  // f(p): return syscall0(p) + 1
  const int32_t f = m.here();
  m.op(kOpEnter, 12).op(kOpLocal, 20).op(kOpLoad4).op(kOpArg, 8).op(kOpConst, -1).op(kOpCall);
  m.op(kOpConst, 1).op(kOpAdd).op(kOpLeave, 12).op(kOpPush).op(kOpLeave, 12);
  m.patch(call_f, f);
  Qvm vm;
  std::vector<int32_t> log;
  vm.syscall = host;
  vm.user = &log;
  CHECK(load(m, vm));
  CHECK(qvm_call(vm, 0) == 51);
  CHECK(log.size() == 1 && log[0] == 0);
  CHECK(vm.depth == 0 && vm.start_stack == static_cast<int32_t>(vm.mem.size()));
}

TEST(the_host_calls_the_module_again_inside_a_system_call) {
  // entry(cmd): cmd == 7 ? 100 : syscall1() + 1
  Module m;
  m.op(kOpEnter, 12).op(kOpLocal, 20).op(kOpLoad4).op(kOpConst, 7);
  const int32_t ne = m.here();
  m.op(kOpNe, 0).op(kOpConst, 100).op(kOpLeave, 12);
  m.patch(ne, m.here());
  m.op(kOpConst, -2).op(kOpCall).op(kOpConst, 1).op(kOpAdd).op(kOpLeave, 12);
  m.op(kOpPush).op(kOpLeave, 12);
  Qvm vm;
  std::vector<int32_t> log;
  vm.syscall = host;
  vm.user = &log;
  CHECK(load(m, vm));
  CHECK(qvm_call(vm, 0) == 101);
  CHECK(log.size() == 1 && log[0] == 1);
  CHECK(vm.fault.empty() && vm.depth == 0);
}

TEST(integer_opcodes_at_their_edges) {
  CHECK(binop(kOpAdd, INT32_MAX, 1) == INT32_MIN);
  CHECK(binop(kOpMuli, 65536, 65536) == 0);
  CHECK(binop(kOpLsh, 1, 33) == 2);  // the count's low 5 bits
  CHECK(binop(kOpRshi, -8, 1) == -4);
  CHECK(binop(kOpRshu, -8, 1) == 0x7ffffffc);
  CHECK(binop(kOpDivi, -7, 2) == -3);
  CHECK(binop(kOpModi, -7, 2) == -1);
  CHECK(binop(kOpDivu, -1, 2) == 0x7fffffff);
  CHECK(binop(kOpModu, -1, 10) == 5);
  CHECK(binop(kOpBand, 12, 10) == 8 && binop(kOpBor, 12, 10) == 14 && binop(kOpBxor, 12, 10) == 6);
  CHECK(unop(kOpNegi, INT32_MIN) == INT32_MIN);
  CHECK(unop(kOpBcom, 0) == -1);
  CHECK(unop(kOpSex8, 0x80) == -128 && unop(kOpSex8, 0x17f) == 127);
  CHECK(unop(kOpSex16, 0x8000) == -32768);
  std::string fault;
  CHECK(binop(kOpDivi, 1, 0, &fault) == 0);
  CHECK(fault.find("division by zero at instruction") != std::string::npos);
  CHECK(binop(kOpModi, INT32_MIN, -1, &fault) == 0);
  CHECK(fault.find("division overflow") != std::string::npos);
}

TEST(float_opcodes_and_conversions) {
  CHECK_BITS(binop(kOpAddf, fbits(0.1f), fbits(0.2f)), fbits(0.1f + 0.2f));
  CHECK_BITS(binop(kOpDivf, fbits(1.0f), fbits(0.0f)),
             fbits(std::numeric_limits<float>::infinity()));
  CHECK_BITS(binop(kOpMulf, fbits(3.0f), fbits(-0.5f)), fbits(-1.5f));
  CHECK(unop(kOpNegf, 0) == INT32_MIN);  // the sign bit of +0
  CHECK_BITS(unop(kOpCvif, 3), fbits(3.0f));
  CHECK_BITS(unop(kOpCvif, 16777217), fbits(16777216.0f));  // to nearest even
  CHECK(unop(kOpCvfi, fbits(-2.75f)) == -2);
  CHECK(unop(kOpCvfi, fbits(std::numeric_limits<float>::quiet_NaN())) == INT32_MIN);
  CHECK(unop(kOpCvfi, fbits(3.0e9f)) == INT32_MIN);
  CHECK(unop(kOpCvfi, fbits(-2147483648.0f)) == INT32_MIN);
}

TEST(branches_compare_as_their_type) {
  // entry(a, b): a <op> b ? 1 : 0
  auto run = [](QvmOp o, int32_t a, int32_t b) {
    Module m;
    m.op(kOpEnter, 8).op(kOpLocal, 20).op(kOpLoad4).op(kOpLocal, 24).op(kOpLoad4);
    const int32_t br = m.here();
    m.op(o, 0).op(kOpConst, 0).op(kOpLeave, 8);
    m.patch(br, m.here());
    m.op(kOpConst, 1).op(kOpLeave, 8).op(kOpPush).op(kOpLeave, 8);
    Qvm vm;
    CHECK(load(m, vm));
    return qvm_call(vm, 0, a, b);
  };
  CHECK(run(kOpLti, -1, 0) == 1 && run(kOpLtu, -1, 0) == 0);
  CHECK(run(kOpGeu, -1, 0) == 1 && run(kOpGei, -1, 0) == 0);
  CHECK(run(kOpEqf, fbits(-0.0f), fbits(0.0f)) == 1 && run(kOpEq, fbits(-0.0f), 0) == 0);
  CHECK(run(kOpNef, fbits(std::numeric_limits<float>::quiet_NaN()),
            fbits(std::numeric_limits<float>::quiet_NaN())) == 1 &&
        run(kOpLef, fbits(std::numeric_limits<float>::quiet_NaN()), fbits(1.0f)) == 0);
}

TEST(the_image_holds_data_then_lit_and_loads_are_checked) {
  Module m = entry([](Module& x) { x.op(kOpLocal, 20).op(kOpLoad4).op(kOpLoad4); });
  m.data = {11, 22};
  m.lit = std::string("hi\0", 3);
  m.bss = 100000;
  Qvm vm;
  CHECK(load(m, vm));
  CHECK(vm.exact == 8 + 3 + 100000);
  CHECK(vm.mem.size() == 262144);
  CHECK(qvm_call(vm, 0, 4) == 22);
  CHECK(qvm_string(vm, 8, "text") == "hi");
  CHECK(qvm_string(vm, 0, "text").empty());
  CHECK(qvm_call(vm, 0, static_cast<int32_t>(vm.mem.size()) - 4) == 0 && vm.fault.empty());
  CHECK(qvm_call(vm, 0, static_cast<int32_t>(vm.mem.size()) - 3) == 0);
  CHECK(vm.fault.find("4 bytes at address 262141 are outside") != std::string::npos);
  CHECK(qvm_call(vm, 0, 4) == 0);  // refused until cleared
  qvm_clear_fault(vm);
  CHECK(qvm_call(vm, 0, 4) == 22);
  CHECK(qvm_put_string(vm, 100, 4, "hello", "buffer") && qvm_string(vm, 100, "t") == "hel");
  CHECK(!qvm_put_string(vm, 262140, 8, "x", "buffer") && !vm.fault.empty());
}

TEST(stores_and_block_copies_are_checked) {
  // entry(addr, value): *addr = value; memcpy(addr + 4, addr, 4); return *(addr + 4)
  Module m = entry([](Module& x) {
    x.op(kOpLocal, 20).op(kOpLoad4).op(kOpLocal, 24).op(kOpLoad4).op(kOpStore4);
    x.op(kOpLocal, 20).op(kOpLoad4).op(kOpConst, 4).op(kOpAdd).op(kOpLocal, 20).op(kOpLoad4);
    x.op(kOpBlockCopy, 4);
    x.op(kOpLocal, 20).op(kOpLoad4).op(kOpConst, 4).op(kOpAdd).op(kOpLoad4);
  });
  m.bss = 64;
  Qvm vm;
  CHECK(load(m, vm));
  CHECK(qvm_call(vm, 0, 16, 1234) == 1234);
  CHECK(qvm_word(vm, 20) == 1234);
  CHECK(qvm_call(vm, 0, -4, 1) == 0);
  CHECK(vm.fault.find("outside the module's memory at instruction") != std::string::npos);
}

TEST(recursion_overflows_the_program_stack_with_frames) {
  Module m;
  m.op(kOpEnter, 8);
  const int32_t c = m.here();
  m.op(kOpConst, 0).op(kOpCall).op(kOpLeave, 8).op(kOpPush).op(kOpLeave, 8);
  const int32_t f = m.here();
  m.op(kOpEnter, 1024)
      .op(kOpConst, f)
      .op(kOpCall)
      .op(kOpLeave, 1024)
      .op(kOpPush)
      .op(kOpLeave, 1024);
  m.patch(c, f);
  m.bss = 70000;
  Qvm vm;
  CHECK(load(m, vm));
  CHECK(qvm_call(vm, 0) == 0);
  CHECK(vm.fault.find("program stack overflow at instruction 6 (ENTER) in function 6; frames: 6") ==
        0);
  CHECK(vm.fault_pc == 6);
  qvm_clear_fault(vm);
  CHECK(vm.start_stack == static_cast<int32_t>(vm.mem.size()) && vm.depth == 0);
}

TEST(a_fault_the_host_raises_stops_the_call) {
  Module m = entry([](Module& x) { x.op(kOpConst, -3).op(kOpCall); });
  Qvm vm;
  std::vector<int32_t> log;
  vm.syscall = host;
  vm.user = &log;
  CHECK(load(m, vm));
  CHECK(qvm_call(vm, 0) == 0);
  CHECK(vm.fault.find("the host refused entity 99 (in system call 2) at instruction 2 (CALL)") ==
        0);
}

TEST(a_computed_jump_must_stay_in_its_function) {
  // entry(t): goto *t; the targets are 3 (inside, depth 0) and 9 (the next function)
  Module m;
  m.op(kOpEnter, 8).op(kOpLocal, 20).op(kOpLoad4).op(kOpJump);                    // 0..3
  m.op(kOpConst, 5).op(kOpLeave, 8).op(kOpPush).op(kOpLeave, 8);                  // 4..7
  m.op(kOpEnter, 8).op(kOpConst, 9).op(kOpLeave, 8).op(kOpPush).op(kOpLeave, 8);  // 8..12
  Qvm vm;
  CHECK(load(m, vm));
  CHECK(qvm_call(vm, 0, 4) == 5);
  CHECK(qvm_call(vm, 0, 9) == 0);
  CHECK(vm.fault.find("a JUMP outside its function") == 0);
  qvm_clear_fault(vm);
  CHECK(qvm_call(vm, 0, 5) == 0);  // into the middle of an expression (depth 1)
}

TEST(the_loader_refuses_each_rule) {
  auto ok = [](Module& x) { x.op(kOpConst, 1); };
  CHECK(refusal(entry(ok)).empty());
  {
    auto f = entry(ok).file();
    f[0] = 0;
    Qvm vm;
    std::string e;
    CHECK(!load_qvm(f, vm, e) && e.find("bad file magic") == 0);
    CHECK(!load_qvm(std::span(f).first(20), vm, e) && e.find("truncated image header") == 0);
    f = entry(ok).file();
    f.push_back(0);
    CHECK(!load_qvm(f, vm, e) && e.find("bad lit segment length") == 0);
  }
  {
    Module m = entry(ok);
    m.code[5] = 77;
    CHECK(refusal(m).find("instruction 1: bad opcode (value 77)") == 0);
  }
  Module leave;
  leave.op(kOpEnter, 8).op(kOpConst, 1).op(kOpLeave, 12).op(kOpPush).op(kOpLeave, 8);
  CHECK(refusal(leave).find("instruction 2: LEAVE's frame differs") == 0);
  Module arg = entry([](Module& x) { x.op(kOpConst, 1).op(kOpArg, 8).op(kOpConst, 1); });
  CHECK(refusal(arg).find("ARG outside the frame") != std::string::npos);
  Module unended;
  unended.op(kOpEnter, 8).op(kOpConst, 1).op(kOpLeave, 8);
  CHECK(refusal(unended).find("a function with no PUSH LEAVE end") != std::string::npos);
  Module far = entry([](Module& x) { x.op(kOpConst, 4096).op(kOpLoad4); });
  CHECK(refusal(far).find("a constant load outside the image") != std::string::npos);
  Module deep = entry([](Module& x) {
    for (int i = 0; i < 30; ++i) x.op(kOpConst, i);
  });
  CHECK(refusal(deep).find("reaches 30 words") != std::string::npos);
  Module branch;
  branch.op(kOpEnter, 8).op(kOpConst, 1).op(kOpConst, 2).op(kOpEq, 9).op(kOpConst, 1);
  branch.op(kOpLeave, 8).op(kOpPush).op(kOpLeave, 8);
  branch.op(kOpEnter, 8).op(kOpConst, 1).op(kOpLeave, 8).op(kOpPush).op(kOpLeave, 8);
  CHECK(refusal(branch).find("instruction 3: a branch target outside its function") == 0);
  Module self;
  self.op(kOpEnter, 8).op(kOpConst, 0).op(kOpCall).op(kOpLeave, 8).op(kOpPush).op(kOpLeave, 8);
  CHECK(refusal(self).find("a CALL target that is not a function's ENTER (or is the entry)") !=
        std::string::npos);
}

TEST(the_sample_module_runs_its_three_exports) {
  Qvm vm;
  std::string error;
  CHECK(load_qvm(qvm_sample(), vm, error));
  std::vector<int32_t> log;
  vm.syscall = host;
  vm.user = &log;
  CHECK(qvm_call(vm, kQvmSampleFib, 10) == 55);
  CHECK(qvm_call(vm, kQvmSampleSum, 4) == 60 && log.size() == 4);
  CHECK(qvm_call(vm, kQvmSampleDiv, 84, 2) == 42);
  CHECK(qvm_call(vm, kQvmSampleDiv, 1, 0) == 0);
  CHECK(vm.fault.find("division by zero") == 0);
}

TEST(a_real_module_loads_when_given) {
  const char* path = std::getenv("GE_QVM_FILE");
  if (path == nullptr) return;
  std::ifstream in(path, std::ios::binary);
  const std::vector<uint8_t> file{std::istreambuf_iterator<char>(in), {}};
  Qvm vm;
  std::string error;
  const bool ok = load_qvm(file, vm, error);
  if (!ok) std::fprintf(stderr, "%s: %s\n", path, error.c_str());
  CHECK(ok);
  std::printf("%s: %zu bytes, %zu instructions, memory %zu, crc %u, entry depth %d\n", path,
              file.size(), vm.code.size(), vm.mem.size(), vm.crc, vm.code[0].aux);
}
