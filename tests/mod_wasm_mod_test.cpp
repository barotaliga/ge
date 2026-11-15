// THE WEBASSEMBLY MOD (engine/mod/wasm/mod.hpp, bind.hpp): a test mod clang compiled for wasm32
// against the mod ABI (mod_wasm_mod_fixture.hpp) is loaded, its table and hook table read; the
// host table is laid out in its memory with the engine's libm, a seeded stream and a log bound by
// their C++ types, and a services table of the test's own (a string, a vector each way, a float);
// a match is started and hooks called; a snapshot taken and restored carries the mod's statics as
// well as its state block; a host the mod refuses and a damaged table are named.
//
// ge:summary Tests the WebAssembly mod: load, the host table laid out, hooks, services, snapshots
// ge:keywords test wasm wasm-mod load_wasm_mod lay_host bind_native call_hook snapshot
// ge:skill mod testing
#include <cstring>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/libm/libm.hpp"
#include "core/prng.hpp"
#include "mod/wasm/bind.hpp"
#include "mod/wasm/mod.hpp"
#include "mod_wasm_mod_fixture.hpp"

namespace w = ge::mod::wasm;
namespace libm = ge::core::libm;

namespace {

// The test's own services table, as the C names it.
struct Svc {
  uint32_t size, api;
  int32_t (*count)(void* ctx, const char* name);
  void (*get_vec)(void* ctx, int32_t e, float out[3]);
  void (*set_vec)(void* ctx, int32_t e, const float v[3]);
  float (*scale)(void* ctx, float x);
};

struct Game {
  ge::core::Rng rng = ge::core::make_rng(38);
  std::vector<std::string> log;
  std::vector<std::string> asked;
  float vec[3] = {2.0f, 4.0f, 8.0f};
};

uint32_t random_u32(void* ctx) {
  return static_cast<uint32_t>(ge::core::next(static_cast<Game*>(ctx)->rng) * 4294967296.0);
}
double random01(void* ctx) { return ge::core::next(static_cast<Game*>(ctx)->rng); }
void log_line(void* ctx, int32_t level, const char* text) {
  static_cast<Game*>(ctx)->log.push_back(std::to_string(level) + " " + text);
}
int32_t count(void* ctx, const char* name) {
  static_cast<Game*>(ctx)->asked.emplace_back(name);
  return name == std::string("rockets") ? 5 : 0;
}
void get_vec(void* ctx, int32_t, float out[3]) {
  std::memcpy(out, static_cast<Game*>(ctx)->vec, sizeof(float) * 3);
}
void set_vec(void* ctx, int32_t, const float v[3]) {
  std::memcpy(static_cast<Game*>(ctx)->vec, v, sizeof(float) * 3);
}
float scale(void*, float x) { return x * 10.0f; }

ge_mod_host_v1 native_host(Game& g) {
  ge_mod_host_v1 h{};
  h.size = sizeof h;
  h.abi = GE_MOD_ABI;
  h.ctx = &g;
  h.sin = libm::sin;
  h.cos = libm::cos;
  h.tan = libm::tan;
  h.asin = libm::asin;
  h.acos = libm::acos;
  h.atan = libm::atan;
  h.atan2 = libm::atan2;
  h.exp = libm::exp;
  h.expm1 = libm::expm1;
  h.log = libm::log;
  h.log2 = libm::log2;
  h.pow = libm::pow;
  h.cbrt = libm::cbrt;
  h.tanh = libm::tanh;
  h.random_u32 = random_u32;
  h.random = random01;
  h.log_line = log_line;
  return h;
}

// The mod loaded and the host laid out over `g`, its services table laid out by the test as a
// game lays out its own.
bool load(w::WasmMod& m, Game& g) {
  std::string error;
  if (!w::load_wasm_mod(kWasmModFixture, sizeof kWasmModFixture, GE_MOD_ABI, m, error)) {
    std::fprintf(stderr, "load: %s\n", error.c_str());
    return false;
  }
  const uint32_t svc = w::reserve(m, 24);
  if (svc == 0) return false;
  const uint32_t words[6] = {24,
                             7,
                             w::bind_native(m.in, m.bindings, &count, &g),
                             w::bind_native(m.in, m.bindings, &get_vec, &g),
                             w::bind_native(m.in, m.bindings, &set_vec, &g),
                             w::bind_native(m.in, m.bindings, &scale, &g)};
  if (!w::write_memory(m.in, svc, words, sizeof words)) return false;
  if (!w::lay_host(m, native_host(g), svc, error)) {
    std::fprintf(stderr, "lay_host: %s\n", error.c_str());
    return false;
  }
  return true;
}

constexpr uint32_t kStep = 0, kTotal = 1, kPush = 2, kUnused = 3, kWave = 4;

}  // namespace

TEST(mod_wasm_mod_reads_the_table) {
  w::WasmMod m;
  std::string error;
  CHECK(w::load_wasm_mod(kWasmModFixture, sizeof kWasmModFixture, GE_MOD_ABI, m, error));
  CHECK((m.abi) == (GE_MOD_ABI));
  CHECK((m.name) == (std::string("test-mod")));
  CHECK((m.version) == (std::string("1.2.0")));
  CHECK((m.game) == (std::string("test_game")));
  CHECK((m.game_api) == (7u));
  CHECK((m.state_bytes) == (24u));  // three ints, a u32 and a double, in wasm32's layout
  CHECK(m.init != 0);
  CHECK((m.hooks_size) == (28u));
  CHECK((m.hooks_api) == (7u));
  CHECK((m.hooks.size()) == (size_t{5}));
  CHECK((m.hooks[kUnused]) == (0u));
}

TEST(mod_wasm_mod_runs_hooks_through_the_host) {
  Game g;
  w::WasmMod m;
  CHECK(load(m, g));
  CHECK(w::start_match(m) == w::Trap::none);
  CHECK((g.log.size()) == (size_t{1}));
  CHECK((g.log[0]) == (std::string("0 test-mod ready")));
  ge::core::Rng want = ge::core::make_rng(38);
  uint32_t draw = 0;
  CHECK(w::read_memory(m.in, m.state_at + 12, &draw, 4));
  CHECK(draw == static_cast<uint32_t>(ge::core::next(want) * 4294967296.0));

  for (int32_t n = 1; n <= 3; ++n) CHECK(w::call_hook<void>(m, kStep, nullptr, n) == w::Trap::none);
  int32_t total = 0;
  CHECK(w::call_hook(m, kTotal, &total) == w::Trap::none);
  CHECK((total) == (6 + 5 + 3000));  // the sum, the game's count, the static's calls
  CHECK((g.asked.size()) == (size_t{1}));
  CHECK((g.asked[0]) == (std::string("rockets")));
  double wave = 0;
  CHECK(w::call_hook(m, kWave, &wave) == w::Trap::none);
  CHECK_BITS(wave, libm::sin(1.0) + libm::sin(2.0) + libm::sin(3.0));

  CHECK(w::call_hook<void>(m, kPush, nullptr, int32_t{4}) == w::Trap::none);
  CHECK((g.vec[0]) == (20.0f));
  CHECK((g.vec[1]) == (4.0f));
  CHECK((g.vec[2]) == (9.5f));

  // A null hook is nothing done; a hook past the table too.
  CHECK(w::call_hook<void>(m, kUnused, nullptr) == w::Trap::none);
  CHECK(w::call_hook<void>(m, 40, nullptr) == w::Trap::none);
  // A hook called as another type than its own is refused, not run.
  CHECK(w::call_hook<void>(m, kStep, nullptr, 1.0f) == w::Trap::indirect);
}

TEST(mod_wasm_mod_snapshot_carries_statics) {
  Game g;
  w::WasmMod m;
  CHECK(load(m, g));
  CHECK(w::start_match(m) == w::Trap::none);
  CHECK(w::call_hook<void>(m, kStep, nullptr, int32_t{2}) == w::Trap::none);
  w::WasmSnapshot snap;
  w::snapshot(m, snap);
  int32_t before = 0;
  CHECK(w::call_hook(m, kTotal, &before) == w::Trap::none);
  for (int i = 0; i < 4; ++i)
    CHECK(w::call_hook<void>(m, kStep, nullptr, int32_t{9}) == w::Trap::none);
  int32_t after = 0;
  CHECK(w::call_hook(m, kTotal, &after) == w::Trap::none);
  CHECK(after != before);
  w::restore(m, snap);
  int32_t again = 0;
  CHECK(w::call_hook(m, kTotal, &again) == w::Trap::none);
  CHECK((again) == (before));  // the static `calls` came back with the block

  // A new match zeroes the block but keeps the statics, as the native form does.
  CHECK(w::start_match(m) == w::Trap::none);
  int32_t fresh = 0;
  CHECK(w::call_hook(m, kTotal, &fresh) == w::Trap::none);
  CHECK((fresh) == (5 + 1000));
}

TEST(mod_wasm_mod_refusals_are_named) {
  w::WasmMod m;
  std::string error;
  // The mod hands a host of another major version nothing.
  CHECK(!w::load_wasm_mod(kWasmModFixture, sizeof kWasmModFixture, 2u << 16, m, error));
  CHECK(error.find("missing") != std::string::npos);
  CHECK(error.find("next:") != std::string::npos);
  // Not a module at all.
  const uint8_t junk[] = {1, 2, 3, 4};
  CHECK(!w::load_wasm_mod(junk, sizeof junk, GE_MOD_ABI, m, error));
  CHECK(error.find("next:") != std::string::npos);
}

TEST(mod_wasm_bind_marshals_by_type) {
  // A bound function's slot answers what a call through it from the module would: tested through
  // the mod above; here, the null pointer is slot 0 and a module with no table refuses.
  Game g;
  w::WasmMod m;
  CHECK(load(m, g));
  CHECK((w::bind_native(m.in, m.bindings, static_cast<float (*)(void*, float)>(nullptr), &g)) ==
        (0u));
  const uint32_t s = w::bind_native(m.in, m.bindings, &scale, &g);
  CHECK(s != 0 && s != w::kNullFunc);
}
