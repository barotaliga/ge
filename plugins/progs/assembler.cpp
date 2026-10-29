// The assembler (assembler.hpp): the layout it starts from, allocation, labels and the file writer.
//
// ge:summary The QC assembler's definitions: the starting layout, allocation, labels, the writer
// ge:keywords qc progs assembler writer little-endian labels patch layout
// ge:skill progs
// ge:test tests/progs_vm_test.cpp
#include "progs/assembler.hpp"

#include <bit>
#include <cstring>
#include <iterator>

namespace ge::progs {

Assembler::Assembler() {
  strings.push_back(0);  // offset 0: the empty string
  globals.assign(kReservedGlobals, 0u);
  int count = 0;
  const LayoutEntry* g = layout_globals(count);
  for (int i = 0; i < count; i++) global(g[i].name, g[i].type);
  const LayoutEntry* f = layout_fields(count);
  for (int i = 0; i < count; i++)
    fielddefs.push_back({f[i].type, static_cast<uint16_t>(f[i].offset), string(f[i].name)});
  functions.push_back(Function{0, 0, 0, 0, 0, 0, {}});  // the null function
  statements.push_back({kOpDone, 0, 0, 0});             // statement 0: never run
  lines_.push_back(0);
}

int32_t Assembler::string(const char* text) {
  const size_t n = std::strlen(text);
  if (n == 0) return 0;
  for (size_t i = 1; i + n < strings.size(); i++) {
    if (std::memcmp(strings.data() + i, text, n + 1) == 0) return static_cast<int32_t>(i);
  }
  const int32_t at = static_cast<int32_t>(strings.size());
  strings.insert(strings.end(), text, text + n + 1);
  return at;
}

int Assembler::alloc(int words) {
  const int at = static_cast<int>(globals.size());
  globals.resize(globals.size() + static_cast<size_t>(words), 0u);
  return at;
}

int Assembler::global(const char* name, uint16_t type, std::initializer_list<uint32_t> init) {
  const int at = alloc(type_words(type));
  int k = 0;
  for (uint32_t w : init) globals[static_cast<size_t>(at) + static_cast<size_t>(k++)] = w;
  globaldefs.push_back({type, static_cast<uint16_t>(at), string(name)});
  return at;
}

int Assembler::temp(int words) { return alloc(words); }

int Assembler::constant_f(float v) {
  const int at = alloc(1);
  globals[static_cast<size_t>(at)] = std::bit_cast<uint32_t>(v);
  return at;
}

int Assembler::constant_v(float x, float y, float z) {
  const int at = alloc(3);
  globals[static_cast<size_t>(at)] = std::bit_cast<uint32_t>(x);
  globals[static_cast<size_t>(at) + 1] = std::bit_cast<uint32_t>(y);
  globals[static_cast<size_t>(at) + 2] = std::bit_cast<uint32_t>(z);
  return at;
}

int Assembler::constant_i(int32_t v) {
  const int at = alloc(1);
  globals[static_cast<size_t>(at)] = static_cast<uint32_t>(v);
  return at;
}

int Assembler::constant_s(const char* text) { return constant_i(string(text)); }

int Assembler::constant_fn(int32_t function) { return constant_i(function); }

int Assembler::field(const char* name, uint16_t type) {
  if (const LayoutEntry* e = layout_field(name)) return constant_i(e->offset);
  const int ofs = entityfields;
  entityfields += type_words(type);
  fielddefs.push_back({type, static_cast<uint16_t>(ofs), string(name)});
  // The global holding the field's offset, named as a compiler names it.
  return global(name, kTypeField, {static_cast<uint32_t>(ofs)});
}

int32_t Assembler::declare(const char* name) {
  Function f{};
  f.name = string(name);
  f.file = string("test.qc");
  functions.push_back(f);
  return static_cast<int32_t>(functions.size() - 1);
}

int32_t Assembler::builtin(const char* name, int number, std::initializer_list<uint8_t> sizes) {
  const int32_t fn = declare(name);
  Function& f = functions[static_cast<size_t>(fn)];
  f.first_statement = -number;
  for (uint8_t s : sizes) {
    if (f.num_parms >= static_cast<int32_t>(std::size(f.parm_size)))
      break;  // a function takes 8 at most
    f.parm_size[f.num_parms++] = s;
  }
  return fn;
}

int32_t Assembler::begin_function(const char* name, std::initializer_list<uint8_t> sizes,
                                  int local_words, const char* file) {
  int32_t fn = -1;
  const int32_t name_ofs = string(name);
  for (size_t i = 1; i < functions.size(); i++) {
    if (functions[i].name == name_ofs && functions[i].first_statement == 0)
      fn = static_cast<int32_t>(i);
  }
  if (fn < 0) fn = declare(name);
  Function& f = functions[static_cast<size_t>(fn)];
  f.file = string(file);
  f.num_parms = 0;
  int parm_words = 0;
  for (uint8_t s : sizes) {
    if (f.num_parms >= static_cast<int32_t>(std::size(f.parm_size)))
      break;  // a function takes 8 at most
    f.parm_size[f.num_parms++] = s;
    parm_words += s;
  }
  f.parm_start = alloc(parm_words + local_words);
  f.locals = parm_words + local_words;
  f.first_statement = static_cast<int32_t>(statements.size());
  open_ = fn;
  open_parm_words_ = parm_words;
  return fn;
}

int Assembler::param(int i) const {
  const Function& f = functions[static_cast<size_t>(open_)];
  int at = f.parm_start;
  for (int k = 0; k < i; k++) at += f.parm_size[k];
  return at;
}

int Assembler::local(int k) const {
  return functions[static_cast<size_t>(open_)].parm_start + open_parm_words_ + k;
}

void Assembler::end_function() { open_ = -1; }

int Assembler::emit(uint16_t op, int a, int b, int c) {
  statements.push_back(
      {op, static_cast<int16_t>(a), static_cast<int16_t>(b), static_cast<int16_t>(c)});
  lines_.push_back(line_);
  return static_cast<int>(statements.size() - 1);
}

void Assembler::call(int32_t function, std::initializer_list<std::pair<int, int>> args) {
  int i = 0;
  for (const auto& [ofs, words] : args) {
    emit(words == 3 ? kOpStoreV : kOpStoreF, ofs, kGlobalParm0 + 3 * i);
    i++;
  }
  emit(static_cast<uint16_t>(kOpCall0 + i), constant_fn(function));
}

int Assembler::label() {
  labels_.push_back({});
  return static_cast<int>(labels_.size() - 1);
}

void Assembler::jump(uint16_t op, int label_id, int cond) {
  const int at = op == kOpGoto ? emit(op) : emit(op, cond);
  labels_[static_cast<size_t>(label_id)].uses.push_back(at);
  if (labels_[static_cast<size_t>(label_id)].at >= 0) place(label_id);
}

void Assembler::place(int label_id) {
  Label& l = labels_[static_cast<size_t>(label_id)];
  if (l.at < 0) l.at = static_cast<int>(statements.size());
  for (int use : l.uses) {
    Statement& s = statements[static_cast<size_t>(use)];
    const auto rel = static_cast<int16_t>(l.at - use);
    if (s.op == kOpGoto)
      s.a = rel;
    else
      s.b = rel;
  }
}

namespace {

void put32(std::vector<uint8_t>& out, uint32_t v) {
  for (int i = 0; i < 4; i++) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
void put16(std::vector<uint8_t>& out, uint16_t v) {
  out.push_back(static_cast<uint8_t>(v));
  out.push_back(static_cast<uint8_t>(v >> 8));
}

}  // namespace

std::vector<uint8_t> Assembler::bytes() const {
  const auto n = [](size_t v) { return static_cast<uint32_t>(v); };
  const uint32_t header = 60;
  const uint32_t st_ofs = header;
  const uint32_t gd_ofs = st_ofs + n(statements.size()) * 8;
  const uint32_t fd_ofs = gd_ofs + n(globaldefs.size()) * 8;
  const uint32_t fn_ofs = fd_ofs + n(fielddefs.size()) * 8;
  const uint32_t str_ofs = fn_ofs + n(functions.size()) * 36;
  const uint32_t gl_ofs = str_ofs + n(strings.size());
  std::vector<uint8_t> out;
  for (uint32_t v :
       {6u, static_cast<uint32_t>(kClassicCrc), st_ofs, n(statements.size()), gd_ofs,
        n(globaldefs.size()), fd_ofs, n(fielddefs.size()), fn_ofs, n(functions.size()), str_ofs,
        n(strings.size()), gl_ofs, n(globals.size()), static_cast<uint32_t>(entityfields)})
    put32(out, v);
  for (const Statement& s : statements) {
    put16(out, s.op);
    put16(out, static_cast<uint16_t>(s.a));
    put16(out, static_cast<uint16_t>(s.b));
    put16(out, static_cast<uint16_t>(s.c));
  }
  for (const auto* defs : {&globaldefs, &fielddefs}) {
    for (const Def& d : *defs) {
      put16(out, d.type);
      put16(out, d.offset);
      put32(out, static_cast<uint32_t>(d.name));
    }
  }
  for (const Function& f : functions) {
    for (int32_t v : {f.first_statement, f.parm_start, f.locals, 0, f.name, f.file, f.num_parms})
      put32(out, static_cast<uint32_t>(v));
    for (uint8_t s : f.parm_size) out.push_back(s);
  }
  out.insert(out.end(), strings.begin(), strings.end());
  for (uint32_t w : globals) put32(out, w);
  return out;
}

bool Assembler::build(Progs& out, std::string& error) const {
  const std::vector<uint8_t> b = bytes();
  if (!load_progs(b.data(), b.size(), out, error)) return false;
  return set_lines(out, lines_, error);
}

}  // namespace ge::progs
