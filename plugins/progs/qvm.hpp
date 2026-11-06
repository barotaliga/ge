// THE COMPILED-MODULE VM (.qvm): a game program compiled from C to a small stack machine, run
// sandboxed in one flat byte memory and talking to the game through numbered system calls. The
// other program form beside QC bytecode (vm.hpp): a server runs a mod shipped as a module
// unchanged. Deterministic to the bit: integer arithmetic wraps, floats are IEEE single precision
// with the build's flags (no contraction, no fast math), shifts take their count's low 5 bits, and
// a float converted to an int out of range or NaN gives INT32_MIN.
//
// THE FILE: a 32-byte header (36 for version 2) of little-endian words: magic, instruction count,
// code offset and length, data offset and length, lit length, bss length (and the jump-target
// table's length); the code is one opcode byte and its operand (4 bytes, 1 for ARG, or none) per
// instruction; code targets are instruction indices. `load_qvm` refuses a header whose segments
// do not tile the file, an unknown opcode, and a module the load-time rules refuse (every
// function an ENTER .. PUSH LEAVE with a frame under 64 KiB, branch targets inside their function
// at depth 0, ARG and LOCAL operands inside the frame, constant loads inside the image, a static
// operand depth never below 0 or at 30 words), each refusal naming the rule and the instruction.
//
// MEMORY: data at address 0, lit after it, bss zeroed after that, all in one block of the smallest
// power of two at least 32 KiB over the image (256 MiB at most); address 0 is the null pointer. The
// program stack grows down from the top; an ENTER that takes it 96 KiB below the top is the fault
// "program stack overflow". EVERY load, store and block copy is checked to lie inside memory, and
// the operand stack (1024 words a host call) is checked at every ENTER against the function's
// measured depth, a computed JUMP must land at depth 0 in its own function and a computed CALL on
// an ENTER, so a module can never read or write outside its block. Division by zero (and INT32_MIN
// / -1) is the fault "division by zero" ("division overflow").
//
// CALLING (`qvm_call`): the entry function is instruction 0. A host call writes a six-word block
// below the current stack (the end marker -1, 0, the command and three arguments) and runs until
// the LEAVE that reads the end marker; exactly one word must be left on the operand stack, and it
// is the result. A CALL to a negative address is SYSTEM CALL -address - 1: its number goes to the
// caller's frame word 4, the host's `syscall` reads it and the caller's outgoing arguments as
// `args[0..16]` (args[0] the number), and its result replaces the address. A system call may call
// the module again (`qvm_call` from inside the host function): the nested call's frames start below
// the system call's frame, and the outer call resumes untouched.
//
// FAULTS: a fault stops the outermost host call; `qvm_call` returns 0 and `vm.fault` says what,
// where (the instruction and the function, by its ENTER's index) and the frames above it. A
// faulted VM refuses calls until `qvm_clear_fault`.
//
// ge:summary The compiled-module VM (.qvm): loader, load-time checks, interpreter, system calls
// ge:keywords qvm compiled-module bytecode stack-machine vm interpreter syscall system-call
// ge:keywords load_qvm qvm_call qvm_clear_fault qvm_string qvm_ptr host-call reentry
// ge:keywords program-stack operand-stack data-mask module-memory match-mod
// ge:skill progs
// ge:test tests/progs_qvm_test.cpp
#pragma once

#include <bit>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ge::progs {

inline constexpr uint32_t kQvmMagic1 = 0x12721444u, kQvmMagic2 = 0x12721445u;
inline constexpr int kQvmOps = 60;              // opcodes 0..59
inline constexpr int kQvmOperandStack = 1024;   // words a host call's operand stack holds
inline constexpr int kQvmProgramStack = 65536;  // bytes the module's stack takes (in its bss)
inline constexpr int kQvmMargin = 32768;        // bytes of memory over the image
inline constexpr int kQvmSyscallArgs = 16;      // words a system call may read (args[1..16])

enum QvmOp : uint8_t {
  kOpUndef,
  kOpIgnore,
  kOpBreak,
  kOpEnter,
  kOpLeave,
  kOpCall,
  kOpPush,
  kOpPop,
  kOpConst,
  kOpLocal,
  kOpJump,
  kOpEq,
  kOpNe,
  kOpLti,
  kOpLei,
  kOpGti,
  kOpGei,
  kOpLtu,
  kOpLeu,
  kOpGtu,
  kOpGeu,
  kOpEqf,
  kOpNef,
  kOpLtf,
  kOpLef,
  kOpGtf,
  kOpGef,
  kOpLoad1,
  kOpLoad2,
  kOpLoad4,
  kOpStore1,
  kOpStore2,
  kOpStore4,
  kOpArg,
  kOpBlockCopy,
  kOpSex8,
  kOpSex16,
  kOpNegi,
  kOpAdd,
  kOpSub,
  kOpDivi,
  kOpDivu,
  kOpModi,
  kOpModu,
  kOpMuli,
  kOpMulu,
  kOpBand,
  kOpBor,
  kOpBxor,
  kOpBcom,
  kOpLsh,
  kOpRshi,
  kOpRshu,
  kOpNegf,
  kOpAddf,
  kOpSubf,
  kOpDivf,
  kOpMulf,
  kOpCvif,
  kOpCvfi,
};
// The opcode's name ("ENTER"), "?" past the table.
const char* qvm_op_name(int op);

struct QvmInstr {
  uint8_t op = 0;
  int32_t v = 0;    // the operand (0 when the opcode has none)
  int32_t aux = 0;  // ENTER: the function's deepest operand stack; else the static depth (words)
  int32_t fn = 0;   // the function holding it, by its ENTER's index
};

struct Qvm;
// A system call: args[0] is the number, args[1..16] the caller's outgoing arguments (words).
using QvmSyscall = int32_t (*)(void* user, Qvm& vm, const int32_t* args);

struct Qvm {
  std::vector<QvmInstr> code;
  std::vector<uint8_t> mem;  // the module's memory, `mem.size()` a power of two
  uint32_t exact = 0;        // data + lit + bss bytes
  uint32_t data_len = 0, lit_len = 0, bss_len = 0;
  uint16_t crc = 0;  // the file's CRC-16 (CCITT, 0xFFFF start), as a server reports it
  int version = 1;
  // The host.
  QvmSyscall syscall = nullptr;
  void* user = nullptr;
  // The run.
  int32_t start_stack = 0;  // where the next host call's frames begin
  int depth = 0;            // host calls active (nested through system calls)
  int32_t ps = 0;           // the program stack of the innermost call running
  uint64_t instructions = 0;
  std::string fault;      // empty while healthy
  int32_t fault_pc = -1;  // the instruction the fault is placed at (-1: not yet placed)
};

// Reads and checks a module; false with the reason (the rule, the instruction, the value).
bool load_qvm(std::span<const uint8_t> file, Qvm& vm, std::string& error);
// Puts the memory back to the file's image (a new map runs a fresh module), the stack at the top.
bool reset_qvm(std::span<const uint8_t> file, Qvm& vm, std::string& error);
// Calls the entry with `command` and three arguments; the module's result (0 after a fault).
int32_t qvm_call(Qvm& vm, int32_t command, int32_t a0 = 0, int32_t a1 = 0, int32_t a2 = 0);
// Stops the running module from inside a system call: the outermost call returns with `why`.
void qvm_raise(Qvm& vm, const std::string& why);
void qvm_clear_fault(Qvm& vm);

// Memory as the host reads and writes it. `qvm_ptr` is the address checked to hold `size` bytes
// (null, and a fault named by `what`, when it does not); `qvm_string` the NUL-terminated text at
// `addr` ("" for address 0; a fault when it runs off the memory's end).
uint8_t* qvm_ptr(Qvm& vm, int32_t addr, uint32_t size, const char* what);
std::string_view qvm_string(Qvm& vm, int32_t addr, const char* what);
// Writes `text` into the buffer at `addr` of `size` bytes, cut to size - 1 and terminated;
// false (and a fault) when the buffer is outside the memory.
bool qvm_put_string(Qvm& vm, int32_t addr, int32_t size, std::string_view text, const char* what);
int32_t qvm_word(const Qvm& vm, int32_t addr);  // caller checked the address
void qvm_set_word(Qvm& vm, int32_t addr, int32_t value);

inline float qvm_float(int32_t bits) { return std::bit_cast<float>(bits); }
inline int32_t qvm_bits(float f) { return std::bit_cast<int32_t>(f); }

}  // namespace ge::progs
