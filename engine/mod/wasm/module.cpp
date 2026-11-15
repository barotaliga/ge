// A WebAssembly module's sections read (module.hpp); each body is checked and compiled by
// validate.cpp.
//
// ge:summary The WebAssembly module's binary reader: sections, imports, exports, segments
// ge:keywords wasm read_module leb128 section custom-section init-expr refuse
// ge:skill mod
// ge:test tests/mod_wasm_test.cpp
#include "mod/wasm/module.hpp"

#include "mod/wasm/reader.hpp"

namespace ge::mod::wasm {

const char* type_name(ValType t) {
  switch (t) {
    case ValType::i32:
      return "i32";
    case ValType::i64:
      return "i64";
    case ValType::f32:
      return "f32";
    case ValType::f64:
      return "f64";
  }
  return "?";
}

const FuncType& Module::func_type(uint32_t index) const {
  if (index < imports.size()) return types[imports[index].type];
  return types[funcs[index - imports.size()].type];
}

const Export* Module::find_export(std::string_view name, ExternKind kind) const {
  for (const Export& e : exports)
    if (e.kind == kind && e.name == name) return &e;
  return nullptr;
}

namespace {

bool read_valtype(Reader& r, ValType& t) {
  const uint8_t b = r.byte();
  if (b == 0x7f || b == 0x7e || b == 0x7d || b == 0x7c) {
    t = static_cast<ValType>(b);
    return true;
  }
  return r.fail("a value type " + hex_byte(b) +
                " that is not i32, i64, f32 or f64 (SIMD and references are not read)");
}

bool read_limits(Reader& r, uint32_t& min, uint32_t& max) {
  const uint8_t flag = r.byte();
  if (flag > 1) return r.fail("limits flagged " + hex_byte(flag) + " (shared memory is not read)");
  min = r.u32();
  max = flag == 1 ? r.u32() : 0xffffffffu;
  if (flag == 1 && max < min) return r.fail("limits whose maximum is below their minimum");
  return r.ok();
}

// A constant expression: one const (or a global.get of an earlier immutable global) and end.
bool read_init(Reader& r, const Module& m, ValType want, uint64_t& bits) {
  const uint8_t op = r.byte();
  ValType got = ValType::i32;
  switch (op) {
    case 0x41:
      bits = static_cast<uint32_t>(r.s32());
      got = ValType::i32;
      break;
    case 0x42:
      bits = static_cast<uint64_t>(r.s64());
      got = ValType::i64;
      break;
    case 0x43:
      bits = r.fixed(4);
      got = ValType::f32;
      break;
    case 0x44:
      bits = r.fixed(8);
      got = ValType::f64;
      break;
    case 0x23: {
      const uint32_t g = r.u32();
      if (g >= m.globals.size() || m.globals[g].mut)
        return r.fail("a constant expression reads global " + std::to_string(g) +
                      ", which is not an earlier immutable global");
      bits = m.globals[g].init;
      got = m.globals[g].type;
      break;
    }
    default:
      return r.fail("a constant expression begins with " + hex_byte(op) +
                    "; only a const or a global.get is read");
  }
  if (r.byte() != 0x0b) return r.fail("a constant expression with more than one instruction");
  if (got != want)
    return r.fail(std::string("a constant expression of ") + type_name(got) + " where " +
                  type_name(want) + " is wanted");
  return r.ok();
}

bool read_types(Reader& r, Module& m) {
  const uint32_t n = r.u32();
  for (uint32_t i = 0; i < n && r.ok(); i++) {
    if (r.byte() != 0x60) return r.fail("a type that is not a function type");
    FuncType t;
    const uint32_t np = r.u32();
    for (uint32_t k = 0; k < np && r.ok(); k++) read_valtype(r, t.params.emplace_back());
    const uint32_t nr = r.u32();
    for (uint32_t k = 0; k < nr && r.ok(); k++) read_valtype(r, t.results.emplace_back());
    m.types.push_back(std::move(t));
  }
  return r.ok();
}

bool read_imports(Reader& r, Module& m) {
  const uint32_t n = r.u32();
  for (uint32_t i = 0; i < n && r.ok(); i++) {
    Import im;
    im.module = r.name();
    im.name = r.name();
    const uint8_t kind = r.byte();
    if (kind != 0)
      return r.fail("the import " + im.module + "." + im.name +
                    " is not a function; a mod imports only the host's functions (link without "
                    "--import-memory or --import-table)");
    im.type = r.u32();
    if (im.type >= m.types.size()) return r.fail("an import of type " + std::to_string(im.type));
    m.imports.push_back(std::move(im));
  }
  return r.ok();
}

bool read_functions(Reader& r, Module& m) {
  const uint32_t n = r.u32();
  for (uint32_t i = 0; i < n && r.ok(); i++) {
    Func f;
    f.type = r.u32();
    if (f.type >= m.types.size())
      return r.fail("a function of type " + std::to_string(f.type) + " past the types");
    m.funcs.push_back(std::move(f));
  }
  return r.ok();
}

bool read_table(Reader& r, Module& m) {
  const uint32_t n = r.u32();
  if (n > 1 || m.has_table) return r.fail("more than one table");
  if (n == 0) return r.ok();
  if (r.byte() != 0x70) return r.fail("a table that does not hold function references");
  m.has_table = true;
  return read_limits(r, m.table_min, m.table_max);
}

bool read_memory(Reader& r, Module& m) {
  const uint32_t n = r.u32();
  if (n > 1 || m.has_memory) return r.fail("more than one memory");
  if (n == 0) return r.ok();
  m.has_memory = true;
  uint32_t max = 0;
  if (!read_limits(r, m.memory_min, max)) return false;
  m.memory_max = max == 0xffffffffu ? 0x10000 : max;
  if (m.memory_min > 0x10000 || m.memory_max > 0x10000) return r.fail("a memory past 4 GiB");
  return r.ok();
}

bool read_globals(Reader& r, Module& m) {
  const uint32_t n = r.u32();
  for (uint32_t i = 0; i < n && r.ok(); i++) {
    Global g;
    if (!read_valtype(r, g.type)) return false;
    const uint8_t mut = r.byte();
    if (mut > 1) return r.fail("a global's mutability " + hex_byte(mut));
    g.mut = mut == 1;
    if (!read_init(r, m, g.type, g.init)) return false;
    m.globals.push_back(g);
  }
  return r.ok();
}

bool read_exports(Reader& r, Module& m) {
  const uint32_t n = r.u32();
  for (uint32_t i = 0; i < n && r.ok(); i++) {
    Export e;
    e.name = r.name();
    const uint8_t kind = r.byte();
    if (kind > 3) return r.fail("an export of kind " + hex_byte(kind));
    e.kind = static_cast<ExternKind>(kind);
    e.index = r.u32();
    m.exports.push_back(std::move(e));
  }
  return r.ok();
}

bool read_elements(Reader& r, Module& m) {
  const uint32_t n = r.u32();
  for (uint32_t i = 0; i < n && r.ok(); i++) {
    const uint32_t flags = r.u32();
    if (flags != 0)
      return r.fail("an element segment flagged " + std::to_string(flags) +
                    "; only active segments of function indices into table 0 are read");
    uint64_t offset = 0;
    if (!read_init(r, m, ValType::i32, offset)) return false;
    Segment s;
    s.offset = static_cast<uint32_t>(offset);
    const uint32_t count = r.u32();
    for (uint32_t k = 0; k < count && r.ok(); k++) s.funcs.push_back(r.u32());
    m.elements.push_back(std::move(s));
  }
  return r.ok();
}

bool read_data(Reader& r, Module& m) {
  const uint32_t n = r.u32();
  for (uint32_t i = 0; i < n && r.ok(); i++) {
    const uint32_t flags = r.u32();
    if (flags != 0)
      return r.fail("a data segment flagged " + std::to_string(flags) +
                    "; only active segments into memory 0 are read (build without "
                    "-mbulk-memory's passive segments)");
    uint64_t offset = 0;
    if (!read_init(r, m, ValType::i32, offset)) return false;
    Segment s;
    s.offset = static_cast<uint32_t>(offset);
    const uint32_t size = r.u32();
    const uint8_t* p = r.take(size);
    if (p != nullptr) s.bytes.assign(p, p + size);
    m.data.push_back(std::move(s));
  }
  return r.ok();
}

// The indices the sections name, checked once they are all read.
bool check_indices(Reader& r, const Module& m) {
  for (const Export& e : m.exports) {
    const bool ok = (e.kind == ExternKind::func && e.index < m.func_count()) ||
                    (e.kind == ExternKind::table && m.has_table && e.index == 0) ||
                    (e.kind == ExternKind::memory && m.has_memory && e.index == 0) ||
                    (e.kind == ExternKind::global && e.index < m.globals.size());
    if (!ok) return r.fail("the export " + e.name + " names an index past its kind's");
  }
  if (m.start != kNoStart) {
    if (m.start >= m.func_count()) return r.fail("a start function past the functions");
    const FuncType& t = m.func_type(m.start);
    if (!t.params.empty() || !t.results.empty())
      return r.fail("a start function that takes or returns values");
  }
  for (const Segment& s : m.elements) {
    if (!m.has_table) return r.fail("an element segment with no table");
    for (uint32_t f : s.funcs)
      if (f >= m.func_count()) return r.fail("an element segment names a function past the end");
  }
  if (!m.data.empty() && !m.has_memory) return r.fail("a data segment with no memory");
  return r.ok();
}

}  // namespace

bool validate_code(Reader& r, Module& m);  // validate.cpp

bool read_module(const uint8_t* bytes, size_t size, Module& out, std::string& error) {
  out = Module{};
  Reader r(bytes, size);
  if (size < 8 || bytes[0] != 0 || bytes[1] != 'a' || bytes[2] != 's' || bytes[3] != 'm') {
    error =
        "not a WebAssembly module (no \\0asm at the start); next: build the mod with "
        "--target=wasm32 (ge mod build --wasm)";
    return false;
  }
  if (bytes[4] != 1 || bytes[5] != 0 || bytes[6] != 0 || bytes[7] != 0) {
    error =
        "a WebAssembly module of a version other than 1; next: build with a clang that "
        "writes version 1";
    return false;
  }
  r.skip(8);
  uint8_t last = 0;
  bool have_code = false;
  while (r.ok() && !r.done()) {
    const uint8_t id = r.byte();
    const uint32_t len = r.u32();
    const size_t start = r.pos();
    if (!r.ok() || len > r.left()) {
      r.fail("a section that runs past the end of the file");
      break;
    }
    // Custom sections (0) may come anywhere; the others once each, in order (the data count, 12,
    // sits between the elements and the code).
    const uint8_t order = id == 12 ? 10 : (id >= 10 ? id + 1 : id);
    if (id != 0) {
      if (id > 12) {
        r.fail("section " + std::to_string(id) +
               " (not one of the format's; tags and the like "
               "are not read)");
        break;
      }
      if (order <= last) {
        r.fail("section " + std::to_string(id) + " out of order or twice");
        break;
      }
      last = order;
    }
    Reader s(bytes + start, len, start);
    bool ok = true;
    switch (id) {
      case 0:
        break;
      case 1:
        ok = read_types(s, out);
        break;
      case 2:
        ok = read_imports(s, out);
        break;
      case 3:
        ok = read_functions(s, out);
        break;
      case 4:
        ok = read_table(s, out);
        break;
      case 5:
        ok = read_memory(s, out);
        break;
      case 6:
        ok = read_globals(s, out);
        break;
      case 7:
        ok = read_exports(s, out);
        break;
      case 8:
        out.start = s.u32();
        ok = s.ok();
        break;
      case 9:
        ok = read_elements(s, out);
        break;
      case 10:
        ok = validate_code(s, out);
        have_code = true;
        break;
      case 11:
        ok = read_data(s, out);
        break;
      case 12:
        (void)s.u32();
        ok = s.ok();
        break;
      default:
        break;
    }
    if (ok && id != 0 && !s.done()) ok = s.fail("bytes left over at the end of the section");
    if (!ok) {
      error = s.error().empty() ? r.error() : s.error();
      return false;
    }
    r.skip(len);
  }
  if (r.ok() && !have_code && !out.funcs.empty()) r.fail("functions declared but no code section");
  if (r.ok()) check_indices(r, out);
  if (!r.ok()) {
    error = r.error();
    return false;
  }
  return true;
}

}  // namespace ge::mod::wasm
