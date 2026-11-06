// The compiled-module test mod (qvm_test_mod.hpp), instruction by instruction.
//
// ge:summary The compiled-module test mod's code: exports, the entity text loop, think and connect
// ge:keywords qvm test-mod assembler entity_token spawn think connect client-command
// ge:skill progs shooter
// ge:test tests/shooters_edge_qvm_server_test.cpp
#include "shooters_edge/server/qvm_test_mod.hpp"

#include <string>

#include "progs/layout.hpp"
#include "progs/qvm_asm.hpp"

namespace shooters_edge::server {

namespace {

namespace pg = ge::progs;

constexpr int32_t kFrame = 64;        // the entry's frame: outgoing args 8..39, locals 40..
constexpr int32_t kCmd = kFrame + 8;  // the export's number
constexpr int32_t kLocalI = 40, kLocalN = 44;

// The system calls it makes.
enum : uint8_t {
  kSysBprint = 12,
  kSysSprint = 13,
  kSysEntityToken = 3,
  kSysSpawn = 4,
  kSysPrecacheModel = 7,
  kSysSetorigin = 9,
  kSysSetsize = 10,
  kSysSetmodel = 11,
  kSysArgv = 57,
  kSysStrcmp = 69,
};

struct Gen {
  pg::QvmAsm m;
  void c(int32_t v) { m.op(pg::kOpConst, v); }
  void cf(float f) { c(pg::qvm_bits(f)); }
  void local(int32_t o) { m.op(pg::kOpLocal, o); }
  void load() { m.op(pg::kOpLoad4); }
  void store() { m.op(pg::kOpStore4); }
  void arg(int i) { m.op(pg::kOpArg, 8 + 4 * i); }
  void sys(int32_t n) {
    c(-n - 1);
    m.op(pg::kOpCall);
  }
  void pop() { m.op(pg::kOpPop); }
  // A branch whose target is set later by `land`.
  int32_t branch(pg::QvmOp o) {
    const int32_t at = m.here();
    m.op(o, 0);
    return at;
  }
  void land(int32_t at) { m.patch(at, m.here()); }
  void ret(int32_t v) {
    c(v);
    m.op(pg::kOpLeave, kFrame);
  }
};

}  // namespace

std::vector<uint8_t> qvm_test_mod() {
  // The lit's strings, after the 16 data words.
  constexpr int32_t kLit = 64;
  std::string lit;
  auto str = [&lit](const char* t) {
    const auto at = static_cast<int32_t>(kLit + lit.size());
    lit += t;
    lit += '\0';
    return at;
  };
  const int32_t player_mdl = str("progs/player.mdl");
  const int32_t connected = str("connected\n");
  const int32_t hello = str("hello");
  const int32_t hi = str("hi\n");
  while (lit.size() % 4 != 0) lit += '\0';
  const auto globals = static_cast<int32_t>(kLit + lit.size());
  const int32_t ents = globals + 512;
  const int32_t tokens = ents + kQvmTestEntitySize * kQvmTestMaxEntities;
  const int32_t words = tokens + 1024;
  const int32_t end = words + 64 + 65536;  // the program stack at the top

  Gen g;
  pg::QvmAsm& m = g.m;
  m.data.assign(16, 0);
  m.data[1] = ents;
  m.data[2] = kQvmTestEntitySize;
  m.data[3] = globals;
  m.data[4] = 28;  // the field table: one end record (words 7..9, all zero)
  m.data[5] = 16;
  m.data[6] = kQvmTestMaxEntities;
  m.lit = lit;
  m.bss = end - globals;

  const int32_t self_at = globals + pg::kGSelf * 4, time_at = globals + pg::kGTime * 4;
  // The record of the entity value on the stack, plus a field's byte offset.
  auto self_field = [&](int field) {
    g.c(ents);
    g.c(self_at);
    g.load();
    m.op(pg::kOpAdd);
    g.c(field * 4);
    m.op(pg::kOpAdd);
  };
  auto is_cmd = [&](int32_t cmd) {
    g.local(kCmd);
    g.load();
    g.c(cmd);
    return g.branch(pg::kOpNe);
  };

  m.op(pg::kOpEnter, kFrame);

  // 0 init: the game data block's address.
  int32_t next = is_cmd(0);
  g.ret(4);
  g.land(next);

  // 1 load entities: the player's model; every block after the world's spawns a thinker.
  next = is_cmd(1);
  g.c(player_mdl);
  g.arg(0);
  g.sys(kSysPrecacheModel);
  g.pop();
  const int32_t token_loop = m.here();
  g.c(tokens);
  g.arg(0);
  g.c(1024);
  g.arg(1);
  g.sys(kSysEntityToken);
  g.c(0);
  const int32_t tokens_done = g.branch(pg::kOpEq);
  g.c(tokens);
  m.op(pg::kOpLoad1);
  g.c('{');
  const int32_t not_block = g.branch(pg::kOpNe);
  g.c(kQvmTestBlocks);
  g.c(kQvmTestBlocks);
  g.load();
  g.c(1);
  m.op(pg::kOpAdd);
  g.store();
  g.land(not_block);
  g.c(token_loop);
  m.op(pg::kOpJump);
  g.land(tokens_done);
  g.local(kLocalI);  // i = 1
  g.c(1);
  g.store();
  const int32_t spawn_loop = m.here();
  g.local(kLocalI);
  g.load();
  g.c(kQvmTestBlocks);
  g.load();
  const int32_t spawned_all = g.branch(pg::kOpGei);
  g.local(kLocalN);  // n = spawn()
  g.sys(kSysSpawn);
  g.store();
  g.local(kLocalN);  // record(n).nextthink = time + 0.1
  g.load();
  g.c(kQvmTestEntitySize);
  m.op(pg::kOpMuli);
  g.c(ents + pg::kFNextthink * 4);
  m.op(pg::kOpAdd);
  g.c(time_at);
  g.load();
  g.cf(0.1f);
  m.op(pg::kOpAddf);
  g.store();
  g.local(kLocalI);  // i++
  g.local(kLocalI);
  g.load();
  g.c(1);
  m.op(pg::kOpAdd);
  g.store();
  g.c(spawn_loop);
  m.op(pg::kOpJump);
  g.land(spawned_all);
  g.ret(0);
  g.land(next);

  // 3 connect: everyone hears of it.
  next = is_cmd(3);
  g.c(2);
  g.arg(0);
  g.c(connected);
  g.arg(1);
  g.c(0);
  g.arg(2);
  g.sys(kSysBprint);
  g.pop();
  g.ret(0);
  g.land(next);

  // 4 put in server: the model, the box, the start spot, the walking move, solid, health.
  next = is_cmd(4);
  g.local(kLocalN);  // n = self / size
  g.c(self_at);
  g.load();
  g.c(kQvmTestEntitySize);
  m.op(pg::kOpDivi);
  g.store();
  g.local(kLocalN);
  g.load();
  g.arg(0);
  g.c(player_mdl);
  g.arg(1);
  g.sys(kSysSetmodel);
  g.pop();
  g.local(kLocalN);
  g.load();
  g.arg(0);
  const float box[6] = {-16, -16, -24, 16, 16, 32};
  for (int i = 0; i < 6; i++) {
    g.cf(box[i]);
    g.arg(1 + i);
  }
  g.sys(kSysSetsize);
  g.pop();
  g.local(kLocalN);
  g.load();
  g.arg(0);
  const float spot[3] = {-416, 0, 24};
  for (int i = 0; i < 3; i++) {
    g.cf(spot[i]);
    g.arg(1 + i);
  }
  g.sys(kSysSetorigin);
  g.pop();
  const struct {
    int field;
    float value;
  } sets[] = {{pg::kFMovetype, 3}, {pg::kFSolid, 3}, {pg::kFHealth, 100}};
  for (const auto& st : sets) {
    self_field(st.field);
    g.cf(st.value);
    g.store();
  }
  g.ret(0);
  g.land(next);

  // 7 client command: `hello` answered with "hi" (handled); anything else not handled.
  next = is_cmd(7);
  g.c(0);
  g.arg(0);
  g.c(words);
  g.arg(1);
  g.c(64);
  g.arg(2);
  g.sys(kSysArgv);
  g.pop();
  g.c(words);
  g.arg(0);
  g.c(hello);
  g.arg(1);
  g.sys(kSysStrcmp);
  g.c(0);
  const int32_t not_hello = g.branch(pg::kOpNe);
  g.c(self_at);
  g.load();
  g.c(kQvmTestEntitySize);
  m.op(pg::kOpDivi);
  g.arg(0);
  g.c(2);
  g.arg(1);
  g.c(hi);
  g.arg(2);
  g.c(0);
  g.arg(3);
  g.sys(kSysSprint);
  g.pop();
  g.ret(1);
  g.land(not_hello);
  g.ret(0);
  g.land(next);

  // 11 start frame: counted.
  next = is_cmd(11);
  g.c(kQvmTestFrames);
  g.c(kQvmTestFrames);
  g.load();
  g.c(1);
  m.op(pg::kOpAdd);
  g.store();
  g.ret(0);
  g.land(next);

  // 16 think: again in 0.1 s, one more counted in frags.
  next = is_cmd(16);
  self_field(pg::kFNextthink);
  g.c(time_at);
  g.load();
  g.cf(0.1f);
  m.op(pg::kOpAddf);
  g.store();
  self_field(pg::kFFrags);
  self_field(pg::kFFrags);
  g.load();
  g.cf(1);
  m.op(pg::kOpAddf);
  g.store();
  g.ret(0);
  g.land(next);

  g.ret(0);
  m.op(pg::kOpPush).op(pg::kOpLeave, kFrame);
  return m.file();
}

}  // namespace shooters_edge::server
