// The hand-written module's builder and the qvm lab's sample (qvm_asm.hpp).
//
// ge:summary The hand-written module's builder and the sample module (qvm_asm.hpp)
// ge:keywords qvm assembler module-builder qvm_sample
// ge:skill progs
// ge:test tests/progs_qvm_test.cpp
#include "progs/qvm_asm.hpp"

namespace ge::progs {

namespace {

void put32(std::vector<uint8_t>& b, int32_t v) {
  for (int i = 0; i < 4; ++i)
    b.push_back(static_cast<uint8_t>(static_cast<uint32_t>(v) >> (8 * i)));
}

bool four(int o) {
  return o == kOpEnter || o == kOpLeave || o == kOpConst || o == kOpLocal || o == kOpBlockCopy ||
         (o >= kOpEq && o <= kOpGef);
}

}  // namespace

QvmAsm& QvmAsm::op(QvmOp o) {
  code.push_back(o);
  ++count;
  return *this;
}

QvmAsm& QvmAsm::op(QvmOp o, int32_t v) {
  code.push_back(o);
  if (o == kOpArg)
    code.push_back(static_cast<uint8_t>(v));
  else if (four(o))
    put32(code, v);
  ++count;
  return *this;
}

void QvmAsm::patch(int32_t i, int32_t v) {
  size_t p = 0;
  for (int32_t k = 0; k < i; ++k) {
    const int o = code[p++];
    p += o == kOpArg ? 1 : four(o) ? 4 : 0;
  }
  for (size_t b = 0; b < 4; ++b)
    code[p + 1 + b] = static_cast<uint8_t>(static_cast<uint32_t>(v) >> (8 * b));
}

std::vector<uint8_t> QvmAsm::file() const {
  std::vector<uint8_t> f;
  const auto code_len = static_cast<int32_t>(code.size());
  put32(f, static_cast<int32_t>(kQvmMagic1));
  put32(f, count);
  put32(f, 32);
  put32(f, code_len);
  put32(f, 32 + code_len);
  put32(f, static_cast<int32_t>(4 * data.size()));
  put32(f, static_cast<int32_t>(lit.size()));
  put32(f, bss);
  f.insert(f.end(), code.begin(), code.end());
  for (int32_t w : data) put32(f, w);
  f.insert(f.end(), lit.begin(), lit.end());
  return f;
}

std::vector<uint8_t> qvm_sample() {
  QvmAsm m;
  m.bss = 65536;  // the program stack, as a compiled module reserves it
  // entry(cmd, a, b), frame 16: cmd 0 -> fib(a); 1 -> sum(a); 2 -> a / b
  m.op(kOpEnter, 16).op(kOpLocal, 24).op(kOpLoad4).op(kOpConst, kQvmSampleFib);
  const int32_t not_fib = m.here();
  m.op(kOpNe, 0).op(kOpLocal, 28).op(kOpLoad4).op(kOpArg, 8);
  const int32_t call_fib = m.here();
  m.op(kOpConst, 0).op(kOpCall).op(kOpLeave, 16);
  m.patch(not_fib, m.here());
  m.op(kOpLocal, 24).op(kOpLoad4).op(kOpConst, kQvmSampleSum);
  const int32_t not_sum = m.here();
  m.op(kOpNe, 0).op(kOpLocal, 28).op(kOpLoad4).op(kOpArg, 8);
  const int32_t call_sum = m.here();
  m.op(kOpConst, 0).op(kOpCall).op(kOpLeave, 16);
  m.patch(not_sum, m.here());
  m.op(kOpLocal, 28).op(kOpLoad4).op(kOpLocal, 32).op(kOpLoad4).op(kOpDivi).op(kOpLeave, 16);
  m.op(kOpPush).op(kOpLeave, 16);
  // fib(n), frame 16 (local 12 holds fib(n - 1)): n < 2 ? n : fib(n - 1) + fib(n - 2)
  const int32_t fib = m.here();
  m.op(kOpEnter, 16).op(kOpLocal, 24).op(kOpLoad4).op(kOpConst, 2);
  const int32_t big = m.here();
  m.op(kOpGei, 0).op(kOpLocal, 24).op(kOpLoad4).op(kOpLeave, 16);
  m.patch(big, m.here());
  m.op(kOpLocal, 12).op(kOpLocal, 24).op(kOpLoad4).op(kOpConst, 1).op(kOpSub).op(kOpArg, 8);
  m.op(kOpConst, fib).op(kOpCall).op(kOpStore4);
  m.op(kOpLocal, 24).op(kOpLoad4).op(kOpConst, 2).op(kOpSub).op(kOpArg, 8);
  m.op(kOpConst, fib).op(kOpCall).op(kOpLocal, 12).op(kOpLoad4).op(kOpAdd).op(kOpLeave, 16);
  m.op(kOpPush).op(kOpLeave, 16);
  // sum(n), frame 20 (local 12 the total, 16 the counter): the total of syscall0(i), i < n
  const int32_t sum = m.here();
  m.op(kOpEnter, 20).op(kOpLocal, 12).op(kOpConst, 0).op(kOpStore4);
  m.op(kOpLocal, 16).op(kOpConst, 0).op(kOpStore4);
  const int32_t top = m.here();
  m.op(kOpLocal, 16).op(kOpLoad4).op(kOpLocal, 28).op(kOpLoad4);
  const int32_t out = m.here();
  m.op(kOpGei, 0);
  m.op(kOpLocal, 12).op(kOpLocal, 12).op(kOpLoad4);
  m.op(kOpLocal, 16)
      .op(kOpLoad4)
      .op(kOpArg, 8)
      .op(kOpConst, -1)
      .op(kOpCall)
      .op(kOpAdd)
      .op(kOpStore4);
  m.op(kOpLocal, 16).op(kOpLocal, 16).op(kOpLoad4).op(kOpConst, 1).op(kOpAdd).op(kOpStore4);
  m.op(kOpConst, top).op(kOpJump);
  m.patch(out, m.here());
  m.op(kOpLocal, 12).op(kOpLoad4).op(kOpLeave, 20).op(kOpPush).op(kOpLeave, 20);
  m.patch(call_fib, fib);
  m.patch(call_sum, sum);
  return m.file();
}

}  // namespace ge::progs
