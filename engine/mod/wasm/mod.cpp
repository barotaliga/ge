// A mod's WebAssembly module loaded, its host table laid out in its memory and its hooks called
// (mod.hpp).
//
// ge:summary The WebAssembly mod's loader: entry, wasm32 tables, the host's layout, snapshots
// ge:keywords wasm wasm-mod load_wasm_mod lay_host reserve start_match snapshot
// ge:skill mod
// ge:test tests/mod_wasm_mod_test.cpp
#include "mod/wasm/mod.hpp"

#include <cstring>

namespace ge::mod::wasm {

namespace {

bool u32_at(const Instance& in, uint32_t at, uint32_t& v) { return read_memory(in, at, &v, 4); }

void put_u32(Instance& in, uint32_t at, uint32_t v) { (void)write_memory(in, at, &v, 4); }

}  // namespace

bool load_wasm_mod(const uint8_t* bytes, size_t size, uint32_t host_abi, WasmMod& out,
                   std::string& error) {
  if (!read_module(bytes, size, out.module, error)) return false;
  if (!instantiate(out.module, {}, kModMemoryPages, kHookFuel, out.in, error)) return false;
  const Export* entry = out.module.find_export(GE_MOD_ENTRY_NAME, ExternKind::func);
  if (entry == nullptr) {
    error = "the module exports no " GE_MOD_ENTRY_NAME
            "; next: define it with GE_MOD_EXPORT "
            "and link with the engine's template (it exports the entry)";
    return false;
  }
  if (!out.module.has_table) {
    error =
        "the module has no function table; next: link with --export-table --growable-table "
        "(the engine's template does)";
    return false;
  }
  const FuncType want{{ValType::i32}, {ValType::i32}};
  if (!(out.module.func_type(entry->index) == want)) {
    error = "the module's " GE_MOD_ENTRY_NAME
            " does not take and return one i32 (the host's ABI, "
            "the table's address); next: declare it as mod/abi.hpp's ge_mod_entry_fn";
    return false;
  }
  const uint64_t arg[1] = {host_abi};
  uint64_t result[1] = {0};
  const Trap t = call(out.in, entry->index, arg, result, kHookFuel);
  if (t != Trap::none) {
    error = std::string("the module's " GE_MOD_ENTRY_NAME " trapped (") + trap_name(t) +
            "): " + out.in.trap_message + "; next: fix the entry";
    return false;
  }
  const uint32_t table = static_cast<uint32_t>(result[0]);
  uint32_t init = 0, hooks_at = 0, name_at = 0, version_at = 0, game_at = 0;
  uint32_t mod_size = 0;
  const Instance& in = out.in;
  if (table == 0 || !u32_at(in, table, mod_size) || mod_size < kModTableBytes ||
      !u32_at(in, table + 4, out.abi) || !u32_at(in, table + 8, name_at) ||
      !u32_at(in, table + 12, version_at) || !u32_at(in, table + 16, game_at) ||
      !u32_at(in, table + 20, out.game_api) || !u32_at(in, table + 24, out.state_bytes) ||
      !u32_at(in, table + 28, init) || !u32_at(in, table + 32, hooks_at)) {
    error = "the entry's table is missing or short (" + std::to_string(mod_size) + " bytes, " +
            std::to_string(kModTableBytes) +
            " wanted in wasm32's layout); next: return a "
            "filled ge_mod_v1 from " GE_MOD_ENTRY_NAME;
    return false;
  }
  out.init = init;
  if (!read_string(in, name_at, 64, out.name) || !read_string(in, version_at, 64, out.version) ||
      !read_string(in, game_at, 64, out.game)) {
    error =
        "the entry's table names a string past the module's memory; next: point name, "
        "version and game at string literals";
    return false;
  }
  if (out.state_bytes > GE_MOD_STATE_MAX) {
    error = "the mod asks for a state block of " + std::to_string(out.state_bytes) +
            " bytes, past " + std::to_string(GE_MOD_STATE_MAX) + "; next: keep the state smaller";
    return false;
  }
  out.hooks.clear();
  if (hooks_at != 0) {
    if (!u32_at(in, hooks_at, out.hooks_size) || !u32_at(in, hooks_at + 4, out.hooks_api) ||
        out.hooks_size < 8 || out.hooks_size > 8 + 4 * 256 || out.hooks_size % 4 != 0) {
      error = "the hook table is short or not in wasm32's layout (" +
              std::to_string(out.hooks_size) +
              " bytes); next: fill the game's hook table, "
              "its size first";
      return false;
    }
    for (uint32_t at = hooks_at + 8; at < hooks_at + out.hooks_size; at += 4) {
      uint32_t slot = 0;
      if (!u32_at(in, at, slot)) {
        error = "the hook table runs past the module's memory; next: rebuild the mod";
        return false;
      }
      out.hooks.push_back(slot);
    }
  }
  return true;
}

uint32_t reserve(WasmMod& m, uint32_t bytes) {
  Instance& in = m.in;
  const uint64_t at = (in.memory.size() + 15) & ~uint64_t{15};
  const uint64_t end = at + bytes;
  const uint64_t pages = (end + kPageBytes - 1) / kPageBytes;
  if (pages > in.memory_max) return 0;
  in.memory.resize(pages * kPageBytes, 0);
  return static_cast<uint32_t>(at);
}

bool lay_host(WasmMod& m, const ge_mod_host_v1& native, uint32_t game_at, std::string& error) {
  Instance& in = m.in;
  m.host_at = reserve(m, kHostTableBytes);
  if (m.host_at == 0) {
    error = "the mod's memory cannot grow to hold the host table; next: give it less memory";
    return false;
  }
  const uint32_t h = m.host_at;
  put_u32(in, h, kHostTableBytes);
  put_u32(in, h + 4, native.abi);
  put_u32(in, h + 8, 0);  // ctx: the host's own, never the module's to read
  void* ctx = native.ctx;
  Bindings& k = m.bindings;
  const uint32_t slots[] = {
      bind_native(in, k, native.sin, ctx),        bind_native(in, k, native.cos, ctx),
      bind_native(in, k, native.tan, ctx),        bind_native(in, k, native.asin, ctx),
      bind_native(in, k, native.acos, ctx),       bind_native(in, k, native.atan, ctx),
      bind_native(in, k, native.atan2, ctx),      bind_native(in, k, native.exp, ctx),
      bind_native(in, k, native.expm1, ctx),      bind_native(in, k, native.log, ctx),
      bind_native(in, k, native.log2, ctx),       bind_native(in, k, native.pow, ctx),
      bind_native(in, k, native.cbrt, ctx),       bind_native(in, k, native.tanh, ctx),
      bind_native(in, k, native.random_u32, ctx), bind_native(in, k, native.random, ctx),
      bind_native(in, k, native.log_line, ctx)};
  uint32_t at = h + 12;
  for (uint32_t s : slots) {
    if (s == kNullFunc) {
      error =
          "the mod's table cannot grow to hold the host's functions; next: link with "
          "--growable-table (the engine's template does)";
      return false;
    }
    put_u32(in, at, s);
    at += 4;
  }
  put_u32(in, h + kHostGameAt, game_at);
  m.state_at = reserve(m, m.state_bytes > 0 ? m.state_bytes : 1);
  if (m.state_at == 0) {
    error = "the mod's memory cannot grow to hold its state block of " +
            std::to_string(m.state_bytes) + " bytes; next: keep the state smaller";
    return false;
  }
  return true;
}

Trap start_match(WasmMod& m) {
  if (m.state_bytes > 0) std::memset(m.in.memory.data() + m.state_at, 0, m.state_bytes);
  if (m.init == 0) return Trap::none;
  return call_slot<void>(m.in, m.init, kHookFuel, nullptr, m.state_at, m.host_at);
}

void snapshot(const WasmMod& m, WasmSnapshot& out) {
  out.memory = m.in.memory;
  out.globals = m.in.globals;
}

void restore(WasmMod& m, const WasmSnapshot& from) {
  m.in.memory = from.memory;
  m.in.globals = from.globals;
}

}  // namespace ge::mod::wasm
