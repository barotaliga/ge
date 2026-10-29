// The progs loader and checker (file.hpp): little-endian reads into the arrays, then every bound.
//
// ge:summary The progs loader and checker: sections read, every bound and index validated
// ge:keywords qc progs load check refuse little-endian sections operands jumps functions defs
// ge:skill progs
// ge:test tests/progs_file_test.cpp
#include "progs/file.hpp"

#include <cstring>

#include "progs/layout.hpp"
#include "progs/opcodes.hpp"

namespace ge::progs {

namespace {

constexpr int kHeaderWords = 15;
constexpr const char* kNext = "; rebuild the mod with a working compiler, or report the file";

uint32_t u32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 |
         static_cast<uint32_t>(p[2]) << 16 | static_cast<uint32_t>(p[3]) << 24;
}
int32_t i32(const uint8_t* p) { return static_cast<int32_t>(u32(p)); }
uint16_t u16(const uint8_t* p) {
  return static_cast<uint16_t>(static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8);
}

struct Check {
  std::string& error;
  bool fail(const std::string& what) {
    error = "progs: " + what + kNext;
    return false;
  }
};

std::string at(const char* section, size_t i, const char* field) {
  return std::string(section) + "[" + std::to_string(i) + "]." + field;
}

// A section (offset, count of records of `size` bytes) inside the file.
bool section(Check& ck, const char* name, int32_t offset, int32_t count, size_t size,
             size_t file_size) {
  if (offset < 0 || count < 0)
    return ck.fail(std::string(name) + ": offset " + std::to_string(offset) + " and count " +
                   std::to_string(count) + " must not be negative");
  const uint64_t end = static_cast<uint64_t>(offset) + static_cast<uint64_t>(count) * size;
  if (end > file_size)
    return ck.fail(std::string(name) + ": " + std::to_string(count) + " records at byte " +
                   std::to_string(offset) + " run past the end of the file (" +
                   std::to_string(file_size) + " bytes)");
  return true;
}

bool name_ok(Check& ck, const Progs& p, const char* section_name, size_t i, int32_t name,
             const char* field) {
  if (name < 0 || static_cast<size_t>(name) >= p.strings.size())
    return ck.fail(at(section_name, i, field) + " = " + std::to_string(name) +
                   " is outside the strings (0.." + std::to_string(p.strings.size()) + ")");
  return true;
}

bool operand_ok(Check& ck, const Progs& p, size_t i, char which, int16_t v, Operand kind) {
  const int64_t n = static_cast<int64_t>(p.globals.size());
  const char field[2] = {which, 0};
  if (kind == Operand::word || kind == Operand::vector) {
    const int w = kind == Operand::vector ? 3 : 1;
    if (v < 0 || v + w > n)
      return ck.fail(at("statements", i, field) + " = " + std::to_string(v) + " (" +
                     op_name(p.statements[i].op) + ") is outside the globals (0.." +
                     std::to_string(n) + ")");
  } else if (kind == Operand::jump) {
    const int64_t to = static_cast<int64_t>(i) + v;
    if (to < 0 || to >= static_cast<int64_t>(p.statements.size()))
      return ck.fail(at("statements", i, field) + " = " + std::to_string(v) + " (" +
                     op_name(p.statements[i].op) + ") jumps to statement " + std::to_string(to) +
                     ", outside the statements (0.." + std::to_string(p.statements.size()) + ")");
  }
  return true;
}

bool check_layout(Check& ck, const Progs& p) {
  if (p.globals.size() < static_cast<size_t>(kEngineGlobalsEnd))
    return ck.fail("globals: " + std::to_string(p.globals.size()) +
                   " words are fewer than the engine-side layout's " +
                   std::to_string(kEngineGlobalsEnd));
  if (p.entityfields < kEngineFields)
    return ck.fail("entityfields = " + std::to_string(p.entityfields) +
                   " is fewer than the engine-side layout's " + std::to_string(kEngineFields));
  // Each engine-side name's FIRST definition (a later one is a local that shadows the name).
  for (int table = 0; table < 2; table++) {
    int count = 0;
    const LayoutEntry* entries = table == 0 ? layout_globals(count) : layout_fields(count);
    for (int i = 0; i < count; i++) {
      const LayoutEntry& e = entries[i];
      const Def* d = table == 0 ? find_global(p, e.name) : find_field(p, e.name);
      if (d != nullptr && (d->offset != e.offset || (d->type & kTypeMask) != e.type))
        return ck.fail(std::string(table == 0 ? "globaldefs" : "fielddefs") + ": '" + e.name +
                       "' is a " + type_name(d->type) + " at " + std::to_string(d->offset) +
                       "; the engine-side layout has a " + type_name(e.type) + " at " +
                       std::to_string(e.offset));
    }
  }
  return true;
}

bool check(Check& ck, const Progs& p, const LoadOptions& o) {
  if (p.strings.empty() || p.strings.front() != 0 || p.strings.back() != 0)
    return ck.fail("strings: the table must start and end with a NUL byte (" +
                   std::to_string(p.strings.size()) + " bytes)");
  if (p.statements.empty()) return ck.fail("statements: none (statement 0 must exist)");
  if (p.functions.empty()) return ck.fail("functions: none (function 0 must exist)");
  if (p.globals.size() < static_cast<size_t>(kReservedGlobals))
    return ck.fail("globals: " + std::to_string(p.globals.size()) + " words are fewer than the " +
                   std::to_string(kReservedGlobals) + " reserved ones");
  if (p.entityfields < 0 || p.entityfields > 0x10000)
    return ck.fail("entityfields = " + std::to_string(p.entityfields) + " is outside 0..65536");
  for (size_t i = 0; i < p.statements.size(); i++) {
    const Statement& s = p.statements[i];
    if (s.op >= kOpCount)
      return ck.fail(at("statements", i, "op") + " = " + std::to_string(s.op) +
                     " is not an opcode (0.." + std::to_string(kOpCount - 1) + ")");
    const OpShape& sh = op_shape(s.op);
    if (!operand_ok(ck, p, i, 'a', s.a, sh.a) || !operand_ok(ck, p, i, 'b', s.b, sh.b) ||
        !operand_ok(ck, p, i, 'c', s.c, sh.c))
      return false;
  }
  for (size_t i = 0; i < p.globaldefs.size(); i++) {
    const Def& d = p.globaldefs[i];
    if (!name_ok(ck, p, "globaldefs", i, d.name, "name")) return false;
    if ((d.type & kTypeMask) > kTypePointer)
      return ck.fail(at("globaldefs", i, "type") + " = " + std::to_string(d.type & kTypeMask) +
                     " is not a type (0..7)");
    if (d.offset + type_words(d.type) > static_cast<int>(p.globals.size()))
      return ck.fail(at("globaldefs", i, "offset") + " = " + std::to_string(d.offset) +
                     " is outside the globals (0.." + std::to_string(p.globals.size()) + ")");
  }
  for (size_t i = 0; i < p.fielddefs.size(); i++) {
    const Def& d = p.fielddefs[i];
    if (!name_ok(ck, p, "fielddefs", i, d.name, "name")) return false;
    if ((d.type & kTypeSaved) != 0)
      return ck.fail(at("fielddefs", i, "type") + " has the saved bit, which no field carries");
    if ((d.type & kTypeMask) > kTypePointer)
      return ck.fail(at("fielddefs", i, "type") + " = " + std::to_string(d.type) +
                     " is not a type (0..7)");
    if (d.offset + type_words(d.type) > p.entityfields)
      return ck.fail(at("fielddefs", i, "offset") + " = " + std::to_string(d.offset) +
                     " is outside the entity (" + std::to_string(p.entityfields) + " words)");
  }
  for (size_t i = 0; i < p.functions.size(); i++) {
    const Function& f = p.functions[i];
    if (!name_ok(ck, p, "functions", i, f.name, "name") ||
        !name_ok(ck, p, "functions", i, f.file, "file"))
      return false;
    if (f.num_parms < 0 || f.num_parms > kMaxParms)
      return ck.fail(at("functions", i, "num_parms") + " = " + std::to_string(f.num_parms) +
                     " is outside 0..8");
    if (f.first_statement < 0) continue;  // a builtin: its globals are never touched
    if (f.first_statement >= static_cast<int32_t>(p.statements.size()))
      return ck.fail(at("functions", i, "first_statement") + " = " +
                     std::to_string(f.first_statement) + " is outside the statements (0.." +
                     std::to_string(p.statements.size()) + ")");
    int32_t parm_words = 0;
    for (int k = 0; k < f.num_parms; k++) {
      if (f.parm_size[k] != 1 && f.parm_size[k] != 3)
        return ck.fail(at("functions", i, "parm_size") + "[" + std::to_string(k) +
                       "] = " + std::to_string(f.parm_size[k]) + " is not 1 or 3");
      parm_words += f.parm_size[k];
    }
    if (f.locals < parm_words)
      return ck.fail(at("functions", i, "locals") + " = " + std::to_string(f.locals) +
                     " is fewer than its parameters' " + std::to_string(parm_words) + " words");
    if (f.parm_start < 0 ||
        static_cast<int64_t>(f.parm_start) + f.locals > static_cast<int64_t>(p.globals.size()))
      return ck.fail(at("functions", i, "parm_start") + " = " + std::to_string(f.parm_start) +
                     " with " + std::to_string(f.locals) + " locals is outside the globals (0.." +
                     std::to_string(p.globals.size()) + ")");
  }
  if (o.check_layout && !check_layout(ck, p)) return false;
  return true;
}

}  // namespace

bool load_progs(const uint8_t* bytes, size_t size, Progs& p, std::string& error,
                const LoadOptions& o) {
  Check ck{error};
  if (size < kHeaderWords * 4)
    return ck.fail("header: the file has " + std::to_string(size) + " bytes, fewer than the " +
                   std::to_string(kHeaderWords * 4) + " of the header");
  int32_t h[kHeaderWords];
  for (int i = 0; i < kHeaderWords; i++) h[i] = i32(bytes + 4 * i);
  p.version = h[0];
  p.crc = h[1];
  if (p.version != 6)
    return ck.fail("header: wrong version: " + std::to_string(p.version) + ", expected 6");
  if (p.crc != o.crc)
    return ck.fail("header: the engine-side layout's crc is " + std::to_string(p.crc) +
                   ", this game provides " + std::to_string(o.crc) +
                   " (a single-player or another game's progs)");
  if (!section(ck, "statements", h[2], h[3], 8, size) ||
      !section(ck, "globaldefs", h[4], h[5], 8, size) ||
      !section(ck, "fielddefs", h[6], h[7], 8, size) ||
      !section(ck, "functions", h[8], h[9], 36, size) ||
      !section(ck, "strings", h[10], h[11], 1, size) ||
      !section(ck, "globals", h[12], h[13], 4, size))
    return false;
  p.entityfields = h[14];

  p.statements.resize(static_cast<size_t>(h[3]));
  for (size_t i = 0; i < p.statements.size(); i++) {
    const uint8_t* r = bytes + h[2] + 8 * i;
    p.statements[i] = {u16(r), static_cast<int16_t>(u16(r + 2)), static_cast<int16_t>(u16(r + 4)),
                       static_cast<int16_t>(u16(r + 6))};
  }
  auto defs = [&](std::vector<Def>& out, int32_t offset, int32_t count) {
    out.resize(static_cast<size_t>(count));
    for (size_t i = 0; i < out.size(); i++) {
      const uint8_t* r = bytes + offset + 8 * i;
      out[i] = {u16(r), u16(r + 2), i32(r + 4)};
    }
  };
  defs(p.globaldefs, h[4], h[5]);
  defs(p.fielddefs, h[6], h[7]);
  p.functions.resize(static_cast<size_t>(h[9]));
  for (size_t i = 0; i < p.functions.size(); i++) {
    const uint8_t* r = bytes + h[8] + 36 * i;
    Function& f = p.functions[i];
    f.first_statement = i32(r);
    f.parm_start = i32(r + 4);
    f.locals = i32(r + 8);
    // r + 12: the file's profile counter, a compiler's leftover; the VM keeps its own.
    f.name = i32(r + 16);
    f.file = i32(r + 20);
    f.num_parms = i32(r + 24);
    std::memcpy(f.parm_size, r + 28, 8);
  }
  p.strings.assign(bytes + h[10], bytes + h[10] + h[11]);
  p.globals.resize(static_cast<size_t>(h[13]));
  for (size_t i = 0; i < p.globals.size(); i++) p.globals[i] = u32(bytes + h[12] + 4 * i);
  p.lines.clear();
  return check(ck, p, o);
}

bool set_lines(Progs& p, std::vector<int32_t> lines, std::string& error) {
  if (lines.size() != p.statements.size()) {
    error = "progs: the line table has " + std::to_string(lines.size()) + " lines for " +
            std::to_string(p.statements.size()) +
            " statements; write it again from the same build as the progs";
    return false;
  }
  p.lines = std::move(lines);
  return true;
}

std::string lines_text(const std::vector<int32_t>& lines) {
  std::string out = "# qc line table: one source line a statement\n";
  for (size_t i = 0; i < lines.size(); i++)
    out += std::to_string(lines[i]) + ((i + 1) % 16 == 0 || i + 1 == lines.size() ? "\n" : " ");
  return out;
}

bool parse_lines(const std::string& text, std::vector<int32_t>& lines, std::string& error) {
  lines.clear();
  size_t i = 0;
  while (i < text.size()) {
    const char c = text[i];
    if (c == '#') {
      while (i < text.size() && text[i] != '\n') i++;
    } else if (c >= '0' && c <= '9') {
      int64_t v = 0;
      while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
        v = v * 10 + (text[i++] - '0');
        if (v > INT32_MAX) {
          error = "the line table has a number past 2147483647 at byte " + std::to_string(i) +
                  "; write it again from the build that wrote the progs";
          return false;
        }
      }
      lines.push_back(static_cast<int32_t>(v));
    } else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
      i++;
    } else {
      error = std::string("the line table has '") + c + "' at byte " + std::to_string(i) +
              "; it holds only decimal numbers (file.hpp)";
      return false;
    }
  }
  return true;
}

const char* progs_string(const Progs& p, int32_t offset) {
  if (offset < 0 || static_cast<size_t>(offset) >= p.strings.size()) return "";
  return p.strings.data() + offset;
}

int find_function(const Progs& p, const char* name) {
  for (size_t i = 1; i < p.functions.size(); i++) {
    if (std::strcmp(progs_string(p, p.functions[i].name), name) == 0) return static_cast<int>(i);
  }
  return -1;
}

namespace {
const Def* find_def(const Progs& p, const std::vector<Def>& defs, const char* name) {
  for (const Def& d : defs) {
    if (std::strcmp(progs_string(p, d.name), name) == 0) return &d;
  }
  return nullptr;
}
}  // namespace

const Def* find_field(const Progs& p, const char* name) { return find_def(p, p.fielddefs, name); }
const Def* find_global(const Progs& p, const char* name) { return find_def(p, p.globaldefs, name); }

}  // namespace ge::progs
