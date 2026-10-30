// Tests: the progs loader and checker. The assembler's file loads back record for record; every
// refusal the checker owes fires on a file broken in exactly that one place and names the
// section, the record, the field and the value; the engine-side layout's offsets are the ones
// the header's constants say; lookups by name take the first definition.
//
// ge:summary Tests: the progs loader's round trip, every refusal, the layout and lookups by name
// ge:keywords test progs qc file load check refuse header section opcode operand jump function
// ge:keywords layout crc lookup
// ge:skill progs testing
// ge:test tests/progs_file_test.cpp
#include <cstring>
#include <string>
#include <vector>

#include "check.hpp"
#include "progs/assembler.hpp"
#include "progs/file.hpp"
#include "progs/layout.hpp"
#include "progs/sample.hpp"

using namespace ge::progs;

namespace {

std::vector<uint8_t> sample_bytes() {
  std::vector<int32_t> lines;
  return build_sample(lines);
}

int32_t word(const std::vector<uint8_t>& b, size_t at) {
  int32_t v = 0;
  std::memcpy(&v, b.data() + at, 4);
  return v;
}
void put(std::vector<uint8_t>& b, size_t at, int32_t v) { std::memcpy(b.data() + at, &v, 4); }
void put16(std::vector<uint8_t>& b, size_t at, int16_t v) { std::memcpy(b.data() + at, &v, 2); }

// Loads `b` and expects a refusal containing every one of `words`.
void refused(const std::vector<uint8_t>& b, std::initializer_list<const char*> words,
             const char* what) {
  Progs p;
  std::string error;
  const bool ok = load_progs(b.data(), b.size(), p, error);
  CHECK_MSG(!ok, std::string("a file with ") + what + " loaded");
  for (const char* w : words) {
    CHECK_MSG(error.find(w) != std::string::npos,
              std::string(what) + ": the refusal '" + error + "' does not say '" + w + "'");
  }
  CHECK_MSG(error.find("rebuild the mod") != std::string::npos || ok,
            std::string(what) + ": the refusal names no next action: " + error);
}

}  // namespace

TEST(the_assemblers_file_loads_back_record_for_record) {
  Assembler as;
  as.global("score", kTypeFloat, {0x40400000u});
  as.field("armor", kTypeVector);
  const int32_t fn = as.begin_function("f", {3, 1}, 2, "x.qc");
  as.emit(kOpReturn, as.param(0));
  as.end_function();
  const std::vector<uint8_t> b = as.bytes();
  Progs p;
  std::string error;
  CHECK_MSG(load_progs(b.data(), b.size(), p, error), error);
  CHECK(p.version == 6 && p.crc == kClassicCrc);
  CHECK(p.statements.size() == as.statements.size());
  CHECK(p.functions.size() == as.functions.size());
  CHECK(p.globals == as.globals);
  CHECK(p.strings == as.strings);
  CHECK(p.entityfields == kEngineFields + 3);
  const Function& f = p.functions[static_cast<size_t>(fn)];
  CHECK(f.num_parms == 2 && f.parm_size[0] == 3 && f.parm_size[1] == 1 && f.locals == 6);
  CHECK(std::strcmp(progs_string(p, f.file), "x.qc") == 0);
  CHECK(find_function(p, "f") == fn);
  CHECK(find_field(p, "armor") != nullptr && find_field(p, "armor")->offset == kEngineFields);
  CHECK(find_global(p, "score") != nullptr);
  CHECK(find_global(p, "self")->offset == kGSelf);
  CHECK(find_function(p, "nothing") == -1);
}

TEST(the_layout_counts_to_the_headers_constants) {
  int n = 0;
  const LayoutEntry* g = layout_globals(n);
  CHECK(g[0].offset == kReservedGlobals && n == 52);
  CHECK(layout_global("PutClientInServer")->offset == kGPutClientInServer);
  CHECK(layout_global("trace_plane_normal")->offset == kGTracePlaneNormal);
  const LayoutEntry* f = layout_fields(n);
  CHECK(f[0].offset == 0 && f[n - 1].offset == kEngineFields - 1);
  CHECK(layout_field("noise3")->offset == kEngineFields - 1);
  CHECK(layout_field("movedir")->type == kTypeVector);
  CHECK(layout_field("sky") == nullptr);
}

TEST(the_header_is_checked) {
  const std::vector<uint8_t> good = sample_bytes();
  std::vector<uint8_t> b = good;
  put(b, 0, 7);
  refused(b, {"wrong version: 7, expected 6"}, "version 7");
  b = good;
  put(b, 4, 12345);
  refused(b, {"crc is 12345", "54730"}, "another layout's crc");
  b = good;
  b.resize(30);
  refused(b, {"header"}, "a cut header");
  b = good;
  put(b, 8 + 4, word(good, 8 + 4) + 100000);  // statements' count
  refused(b, {"statements", "run past the end"}, "too many statements");
  b = good;
  put(b, 36, -1);  // the functions' count
  refused(b, {"functions", "must not be negative"}, "a negative count");
}

TEST(every_record_is_checked) {
  const std::vector<uint8_t> good = sample_bytes();
  const size_t st = static_cast<size_t>(word(good, 8));
  const size_t gd = static_cast<size_t>(word(good, 16));
  const size_t fd = static_cast<size_t>(word(good, 24));
  const size_t fn = static_cast<size_t>(word(good, 32));
  const size_t str = static_cast<size_t>(word(good, 40));
  const int32_t str_size = word(good, 44);
  const int32_t globals = word(good, 52);
  std::vector<uint8_t> b = good;

  put16(b, st + size_t{8} * 1, 99);  // statement 1's opcode
  refused(b, {"statements[1].op = 99", "not an opcode"}, "an unknown opcode");
  b = good;
  put16(b, st + size_t{8} * 1, kOpAddF);
  put16(b, st + size_t{8} * 1 + 6, static_cast<int16_t>(globals));
  refused(b, {"statements[1].c", "outside the globals"}, "an operand past the globals");
  b = good;
  put16(b, st + size_t{8} * 1, kOpStoreV);
  put16(b, st + size_t{8} * 1 + 2, static_cast<int16_t>(globals - 2));
  refused(b, {"statements[1].a", "STORE_V"}, "a vector operand two words from the end");
  b = good;
  put16(b, st + size_t{8} * 1, kOpGoto);
  put16(b, st + size_t{8} * 1 + 2, -5);
  refused(b, {"statements[1].a = -5", "jumps to statement -4"}, "a jump before statement 0");

  // Function 7 is fib, one parameter (five builtins and think before it).
  b = good;
  put(b, fn + size_t{36} * 7, 1000000);
  refused(b, {"functions[7].first_statement", "outside the statements"}, "a body past the end");
  b = good;
  put(b, fn + size_t{36} * 7 + 4, globals);
  refused(b, {"functions[7].parm_start", "outside the globals"}, "locals past the globals");
  b = good;
  put(b, fn + size_t{36} * 7 + 24, 9);
  refused(b, {"functions[7].num_parms = 9"}, "nine parameters");
  b = good;
  b[fn + size_t{36} * 7 + 28] = 2;
  refused(b, {"functions[7].parm_size[0] = 2", "not 1 or 3"}, "a two-word parameter");
  b = good;
  put(b, fn + size_t{36} * 7 + 8, 0);
  refused(b, {"functions[7].locals = 0", "fewer than its parameters"}, "no room for a parameter");
  b = good;
  put(b, fn + size_t{36} * 2 + 16, str_size);
  refused(b, {"functions[2].name", "outside the strings"}, "a name past the strings");

  b = good;
  put(b, gd + 4, -3);
  refused(b, {"globaldefs[0].name"}, "a negative name");
  b = good;
  b[fd] = static_cast<uint8_t>(kTypeFloat);
  b[fd + 1] = 0x80;
  refused(b, {"fielddefs[0].type", "saved bit"}, "a saved field");
  b = good;
  b[fd + 2] = 0xff;
  b[fd + 3] = 0x0f;
  refused(b, {"fielddefs[0].offset", "outside the entity"}, "a field past the entity");
  b = good;
  b[str + static_cast<size_t>(str_size) - 1] = 'x';
  refused(b, {"strings", "NUL"}, "an unterminated string table");
}

TEST(the_engine_side_definitions_must_sit_where_the_layout_says) {
  Assembler as;
  // A program whose first "time" is a vector at the program's own end: the layout's is earlier,
  // so move the layout's definition's name instead to stage a mismatch.
  as.globaldefs[3].offset = 50;  // "time" (the fourth engine-side global) declared elsewhere
  const std::vector<uint8_t> b = as.bytes();
  refused(b, {"'time' is a float at 50", "layout has a float at 31"}, "time at the wrong offset");
  Assembler shadow;
  shadow.global("time", kTypeVector);  // a later definition of the name: a local, accepted
  const std::vector<uint8_t> s = shadow.bytes();
  Progs p;
  std::string error;
  CHECK_MSG(load_progs(s.data(), s.size(), p, error), error);
  LoadOptions any;
  any.check_layout = false;
  CHECK(load_progs(b.data(), b.size(), p, error, any));
}

TEST(a_line_table_must_match_the_statements) {
  Progs p;
  std::string error;
  CHECK(load_sample(p, error));
  CHECK(p.lines.size() == p.statements.size());
  CHECK(!set_lines(p, {1, 2, 3}, error));
  CHECK(error.find("3 lines for") != std::string::npos);
}
