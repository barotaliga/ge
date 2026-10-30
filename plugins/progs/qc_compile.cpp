// The compiler's front (qc_compile.hpp): tokens, types, the global layout, declarations, the
// end-of-program checks, and the project file. A function's body is qc_statements.cpp; an
// expression qc_expressions.cpp.
//
// ge:summary The QC compiler's declarations, layout and project file; compile_qc, compile_project
// ge:keywords qc compiler declaration prototype builtin field-declaration constant progs-src
// ge:keywords layout-check crc type-parse
// ge:skill progs
// ge:test tests/progs_qc_compile_test.cpp
#include <algorithm>
#include <bit>
#include <fstream>
#include <sstream>

#include "progs/qc_compiler.hpp"

namespace ge::progs {

namespace qc {

uint16_t store_op(uint16_t t) {
  switch (t) {
    case kTypeVector:
      return kOpStoreV;
    case kTypeString:
      return kOpStoreS;
    case kTypeEntity:
      return kOpStoreEnt;
    case kTypeField:
      return kOpStoreFld;
    case kTypeFunction:
      return kOpStoreFnc;
    default:
      return kOpStoreF;
  }
}

uint16_t storep_op(uint16_t t) {
  switch (t) {
    case kTypeVector:
      return kOpStorePV;
    case kTypeString:
      return kOpStorePS;
    case kTypeEntity:
      return kOpStorePEnt;
    case kTypeField:
      return kOpStorePFld;
    case kTypeFunction:
      return kOpStorePFnc;
    default:
      return kOpStorePF;
  }
}

uint16_t load_op(uint16_t t) {
  switch (t) {
    case kTypeVector:
      return kOpLoadV;
    case kTypeString:
      return kOpLoadS;
    case kTypeEntity:
      return kOpLoadEnt;
    case kTypeField:
      return kOpLoadFld;
    case kTypeFunction:
      return kOpLoadFnc;
    default:
      return kOpLoadF;
  }
}

uint16_t not_op(uint16_t t) {
  switch (t) {
    case kTypeVector:
      return kOpNotV;
    case kTypeString:
      return kOpNotS;
    case kTypeEntity:
    case kTypeField:  // a field value is an integer word, as an entity's
      return kOpNotEnt;
    case kTypeFunction:
      return kOpNotFnc;
    default:
      return kOpNotF;
  }
}

Compiler::Compiler() {
  t_void = intern({kTypeVoid, -1, {}, false});
  t_float = intern({kTypeFloat, -1, {}, false});
  t_vector = intern({kTypeVector, -1, {}, false});
  t_string = intern({kTypeString, -1, {}, false});
  t_entity = intern({kTypeEntity, -1, {}, false});
  t_void_fn = function_of(t_void, {}, false);
  prog.version = 6;
  prog.statements.push_back({0, 0, 0, 0});
  prog.lines.push_back(0);
  prog.functions.push_back({});
  prog.strings.push_back('\0');
  prog.globals.resize(kReservedGlobals, 0);
}

// --- tokens ---

const Token& Compiler::peek(size_t ahead) const {
  const size_t at = std::min(pos + ahead, toks->size() - 1);
  return (*toks)[at];
}

const Token& Compiler::next() {
  const Token& t = peek();
  if (pos + 1 < toks->size()) ++pos;
  return t;
}

bool Compiler::is(std::string_view word, size_t ahead) const {
  const Token& t = peek(ahead);
  return (t.kind == Tok::punct || t.kind == Tok::name) && t.text == word;
}

bool Compiler::accept(std::string_view word) {
  if (!is(word)) return false;
  next();
  return true;
}

void Compiler::expect(std::string_view word, std::string_view what) {
  if (accept(word)) return;
  const Token& t = peek();
  const std::string got = t.kind == Tok::end      ? std::string("the end of the file")
                          : t.kind == Tok::string ? std::string("a string")
                          : t.kind == Tok::number ? std::string("a number")
                          : t.kind == Tok::vector ? std::string("a vector")
                                                  : "'" + t.text + "'";
  fail("expected '" + std::string(word) + "' " + std::string(what) + ", got " + got);
}

std::string Compiler::expect_name(std::string_view what) {
  if (peek().kind != Tok::name) fail("expected " + std::string(what));
  return next().text;
}

void Compiler::fail(std::string message) const { fail_at(peek().line, std::move(message)); }

void Compiler::fail_at(int line, std::string message) const {
  throw CompileError{line, std::move(message)};
}

// --- types ---

int Compiler::intern(const Type& t) {
  for (size_t i = 0; i < types.size(); ++i) {
    if (types[i] == t) return static_cast<int>(i);
  }
  types.push_back(t);
  return static_cast<int>(types.size()) - 1;
}

int Compiler::field_of(int t) {
  Type f;
  f.base = kTypeField;
  f.inner = t;
  return intern(f);
}

int Compiler::function_of(int ret, std::vector<int> params, bool varargs) {
  Type f;
  f.base = kTypeFunction;
  f.inner = ret;
  f.params = std::move(params);
  f.varargs = varargs;
  return intern(f);
}

std::string Compiler::type_name(int t) const {
  const Type& ty = type(t);
  switch (ty.base) {
    case kTypeVoid:
      return "void";
    case kTypeString:
      return "string";
    case kTypeFloat:
      return "float";
    case kTypeVector:
      return "vector";
    case kTypeEntity:
      return "entity";
    case kTypeField:
      return "." + type_name(ty.inner);
    case kTypeFunction: {
      std::string s = type_name(ty.inner) + "(";
      for (size_t i = 0; i < ty.params.size(); ++i) {
        s += (i ? ", " : "") + type_name(ty.params[i]);
      }
      if (ty.varargs) s += ty.params.empty() ? "..." : ", ...";
      return s + ")";
    }
    default:
      return "?";
  }
}

bool Compiler::is_type_word(size_t ahead) const {
  return is("void", ahead) || is("float", ahead) || is("vector", ahead) || is("string", ahead) ||
         is("entity", ahead);
}

int Compiler::parse_type(std::vector<std::string>* names) {
  if (accept(".")) return field_of(parse_type(names));
  int t = -1;
  if (accept("void"))
    t = t_void;
  else if (accept("float"))
    t = t_float;
  else if (accept("vector"))
    t = t_vector;
  else if (accept("string"))
    t = t_string;
  else if (accept("entity"))
    t = t_entity;
  else
    fail("expected a type (void, float, vector, string, entity, or . for a field)");
  if (is("(")) t = parse_params(t, names);
  return t;
}

int Compiler::parse_params(int ret, std::vector<std::string>* names) {
  expect("(", "to open the parameter list");
  std::vector<int> params;
  std::vector<std::string> pnames;
  bool varargs = false;
  if (!accept(")")) {
    for (;;) {
      if (accept("...")) {
        varargs = true;
        expect(")", "after ...: it is the last parameter");
        break;
      }
      const int pt = parse_type(nullptr);
      if (pt == t_void && params.empty() && accept(")")) break;  // `float(void)`
      if (size_of(pt) == 0) fail("a parameter cannot be void");
      params.push_back(pt);
      pnames.push_back(expect_name("a parameter name"));
      if (accept(",")) continue;
      expect(")", "to close the parameter list");
      break;
    }
  }
  if (params.size() > static_cast<size_t>(kMaxParms)) {
    fail("a function takes at most 8 parameters, this one " + std::to_string(params.size()));
  }
  if (names) *names = std::move(pnames);
  return function_of(ret, std::move(params), varargs);
}

// --- layout ---

int Compiler::alloc_global(int words) {
  const int ofs = static_cast<int>(prog.globals.size());
  if (ofs + words > 32767) {
    fail(
        "the program needs more than 32767 global words, the format's limit: split the mod or "
        "share constants");
  }
  prog.globals.resize(static_cast<size_t>(ofs) + static_cast<size_t>(words), 0);
  return ofs;
}

int Compiler::intern_string(const std::string& s) {
  if (s.empty()) return 0;
  const auto it = strings.find(s);
  if (it != strings.end()) return it->second;
  const int ofs = static_cast<int>(prog.strings.size());
  prog.strings.insert(prog.strings.end(), s.begin(), s.end());
  prog.strings.push_back('\0');
  strings.emplace(s, ofs);
  return ofs;
}

int Compiler::immediate(uint16_t t, std::vector<uint32_t> words) {
  std::vector<uint32_t> key = words;
  key.insert(key.begin(), t);
  const auto it = immediates.find(key);
  if (it != immediates.end()) return it->second;
  const int ofs = alloc_global(static_cast<int>(words.size()));
  for (size_t i = 0; i < words.size(); ++i) prog.globals[size_t(ofs) + i] = words[i];
  immediates.emplace(std::move(key), ofs);
  return ofs;
}

int Compiler::float_const(float f) { return immediate(kTypeFloat, {std::bit_cast<uint32_t>(f)}); }

void Compiler::add_global_def(const std::string& name, int t, int ofs, bool saved) {
  Def d{};
  d.type = static_cast<uint16_t>(base(t) | (saved ? kTypeSaved : 0));
  d.offset = static_cast<uint16_t>(ofs);
  d.name = intern_string(name);
  prog.globaldefs.push_back(d);
}

// --- declarations ---

int Compiler::declare_global(const std::string& name, int t, int line) {
  if (const auto it = global_by_name.find(name); it != global_by_name.end()) {
    const Symbol& s = globals[size_t(it->second)];
    if (s.type != t) {
      fail("'" + name + "' is declared as " + type_name(s.type) + " on line " +
           std::to_string(s.line) + "; here it is " + type_name(t));
    }
    return it->second;
  }
  auto add = [&](const std::string& n, int ty, int ofs) {
    Symbol s;
    s.name = n;
    s.type = ty;
    s.ofs = ofs;
    s.line = line;
    globals.push_back(s);
    global_by_name.emplace(n, static_cast<int>(globals.size()) - 1);
    return static_cast<int>(globals.size()) - 1;
  };
  if (base(t) == kTypeField) {
    const int inner = type(t).inner;
    if (base(inner) == kTypeVoid) fail("a field cannot be void");
    const int field = next_field;
    next_field += size_of(inner);
    // A field: its definition, and a global holding its offset (what LOAD and ADDRESS name).
    auto field_def = [&](const std::string& n, int value_type, int at) {
      Def d{};
      d.type = base(value_type);
      d.offset = static_cast<uint16_t>(at);
      d.name = intern_string(n);
      prog.fielddefs.push_back(d);
      const int g = alloc_global(1);
      prog.globals[size_t(g)] = static_cast<uint32_t>(at);
      add_global_def(n, field_of(value_type), g, false);
      return add(n, field_of(value_type), g);
    };
    const int sym = field_def(name, inner, field);
    if (base(inner) == kTypeVector) {
      for (int k = 0; k < 3; ++k) field_def(name + "_" + "xyz"[k], t_float, field + k);
    }
    return sym;
  }
  const int ofs = alloc_global(size_of(t));
  add_global_def(name, t, ofs, base(t) != kTypeVoid);
  const int sym = add(name, t, ofs);
  if (base(t) == kTypeVector) {
    for (int k = 0; k < 3; ++k) {
      const std::string n = name + "_" + "xyz"[k];
      add_global_def(n, t_float, ofs + k, false);
      add(n, t_float, ofs + k);
    }
  }
  return sym;
}

int Compiler::declare_function(const std::string& name, int t, int line) {
  if (const auto it = global_by_name.find(name); it != global_by_name.end()) {
    const Symbol& s = globals[size_t(it->second)];
    if (s.type != t) {
      fail("'" + name + "' is declared as " + type_name(s.type) + " on line " +
           std::to_string(s.line) + "; here it is " + type_name(t));
    }
    return it->second;
  }
  Function f{};
  f.name = intern_string(name);
  f.file = file_name;
  const Type& ty = type(t);
  f.num_parms = static_cast<int32_t>(ty.params.size());
  for (size_t i = 0; i < ty.params.size(); ++i) {
    f.parm_size[i] = static_cast<uint8_t>(size_of(ty.params[i]));
  }
  prog.functions.push_back(f);
  const int ofs = alloc_global(1);
  prog.globals[size_t(ofs)] = static_cast<uint32_t>(prog.functions.size() - 1);
  add_global_def(name, t, ofs, false);
  Symbol s;
  s.name = name;
  s.type = t;
  s.ofs = ofs;
  s.constant = true;
  s.function = static_cast<int>(prog.functions.size()) - 1;
  s.line = line;
  globals.push_back(s);
  global_by_name.emplace(name, static_cast<int>(globals.size()) - 1);
  return static_cast<int>(globals.size()) - 1;
}

// `= literal` after a global: it becomes a constant holding the literal.
void Compiler::constant_value(int sym, int t) {
  Symbol& s = globals[size_t(sym)];
  if (s.constant) fail("'" + s.name + "' already has a value");
  const uint16_t b = base(t);
  const int line = peek().line;
  bool negative = false;
  if (accept("-")) negative = true;
  const Token& lit = next();
  uint32_t* w = &prog.globals[size_t(s.ofs)];
  if (b == kTypeFloat && lit.kind == Tok::number) {
    w[0] = std::bit_cast<uint32_t>(negative ? -lit.number : lit.number);
  } else if (b == kTypeString && lit.kind == Tok::string && !negative) {
    w[0] = static_cast<uint32_t>(intern_string(lit.text));
  } else if (b == kTypeVector && lit.kind == Tok::vector && !negative) {
    for (int k = 0; k < 3; ++k) w[k] = std::bit_cast<uint32_t>(lit.vec[size_t(k)]);
  } else {
    fail_at(line, "'" + s.name + "' is " + type_name(t) +
                      ": a constant takes a literal of its type (a number, \"text\" or 'x y z')");
  }
  s.constant = true;
  for (Def& d : prog.globaldefs) {  // a constant is not saved with a game
    if (d.offset == s.ofs) d.type &= static_cast<uint16_t>(~kTypeSaved);
  }
}

void Compiler::declaration() {
  const int line = peek().line;
  std::vector<std::string> names;
  const int t = parse_type(&names);
  for (;;) {
    const std::string name = expect_name("a name to declare");
    int dt = t;
    std::vector<std::string> pnames = names;
    if (base(t) != kTypeFunction && is("(")) dt = parse_params(t, &pnames);  // the C form
    bool had_body = false;
    if (base(dt) == kTypeFunction) {
      const int sym = declare_function(name, dt, line);
      const bool assigned = accept("=");
      if (assigned && accept("#")) {
        if (peek().kind != Tok::number) fail("a builtin is bound by number: = #12;");
        const float n = next().number;
        Symbol& s = globals[size_t(sym)];
        if (s.defined) fail("'" + name + "' is defined twice");
        s.defined = true;
        prog.functions[size_t(s.function)].first_statement = -static_cast<int32_t>(n);
      } else if (is("[") || is("{")) {
        function_body(sym, pnames);
        had_body = true;
      } else if (assigned) {
        fail(
            "a function is given a body { ... }, a frame [$frame, next] { ... } or a builtin "
            "number #N");
      }
    } else {
      const int sym = declare_global(name, dt, line);
      if (accept("=")) constant_value(sym, dt);
      if (name == "end_sys_fields") claims_layout = true;
    }
    if (accept(",")) continue;
    if (had_body) {
      accept(";");
    } else {
      expect(";", "to end the declaration");
    }
    return;
  }
}

void Compiler::function_body(int sym, const std::vector<std::string>& parm_names) {
  {
    const Symbol& s = globals[size_t(sym)];
    if (s.defined) {
      fail("'" + s.name + "' is defined twice (declared on line " + std::to_string(s.line) + ")");
    }
  }
  const int t = globals[size_t(sym)].type;
  fn_return = type(t).inner;
  locals.clear();
  local_by_name.clear();
  code.clear();
  block_words = 0;
  free1.clear();
  free3.clear();
  temps1.clear();
  temps3.clear();
  const std::vector<int> params = type(t).params;
  for (size_t i = 0; i < params.size(); ++i) {
    if (i >= parm_names.size()) fail("every parameter of a function with a body needs a name");
    declare_local(parm_names[i], params[i]);
  }
  if (accept("[")) {
    const Val frame = expression();
    if (base(frame.type) != kTypeFloat) fail("a frame function's frame is a float");
    const int frame_ofs = rv(frame);
    expect(",", "between the frame and the next function");
    const int think_line = peek().line;
    const std::string think = expect_name("the next function's name");
    const Symbol* next_fn = lookup(think);
    int think_ofs = 0;
    if (!next_fn) {
      think_ofs = globals[size_t(declare_function(think, t_void_fn, think_line))].ofs;
    } else {
      if (base(next_fn->type) != kTypeFunction) fail("'" + think + "' is not a function");
      think_ofs = next_fn->ofs;
    }
    expect("]", "to close the frame");
    emit(kOpState, frame_ofs, think_ofs, 0);
  }
  block();
  emit(kOpDone, 0, 0, 0);
  finish_function(sym);
}

void Compiler::finish() {
  for (const Symbol& s : globals) {
    if (s.function >= 0 && !s.defined) {
      fail_at(s.line, "'" + s.name +
                          "' is declared but never defined: give it a body or a "
                          "builtin number");
    }
  }
  prog.entityfields = next_field;
  // RETURN copies three words whatever the type, so a float returned from the last global must
  // have two words after it: the globals end with two spare zeros.
  alloc_global(2);
  // The layout: every engine-side global and field at its name, type and offset.
  std::string first_wrong;
  int wrong = 0;
  auto check = [&](const LayoutEntry* rows, int count, const std::vector<Def>& defs,
                   const char* what) {
    for (int i = 0; i < count; ++i) {
      const LayoutEntry& e = rows[i];
      const Def* found = nullptr;
      for (const Def& d : defs) {
        if (std::string_view(progs_string(prog, d.name)) == e.name) {
          found = &d;
          break;
        }
      }
      const bool good = found && (found->type & kTypeMask) == e.type && found->offset == e.offset;
      if (good) continue;
      if (++wrong == 1) {
        first_wrong = std::string("engine ") + what + " '" + e.name + "' should be " +
                      progs::type_name(e.type) + " at " + std::to_string(e.offset);
        if (found) {
          first_wrong += ", is " + std::string(progs::type_name(found->type & kTypeMask)) + " at " +
                         std::to_string(found->offset);
        }
      }
    }
  };
  int ng = 0, nf = 0;
  const LayoutEntry* lg = layout_globals(ng);
  const LayoutEntry* lf = layout_fields(nf);
  check(lg, ng, prog.globaldefs, "global");
  check(lf, nf, prog.fielddefs, "field");
  prog.crc = wrong == 0 ? kClassicCrc : 0;
  if (wrong && claims_layout) {
    diagnostics.push_back({file, 0, false,
                           "the engine layout does not match (" + std::to_string(wrong) +
                               " slots), so the program is stamped crc 0 and a game refuses it; "
                               "first: " +
                               first_wrong});
  }
}

}  // namespace qc

std::string diagnostic_text(const QcDiagnostic& d) {
  return d.file + ":" + std::to_string(d.line) + ": " + (d.error ? "error: " : "warning: ") +
         d.message;
}

QcResult compile_qc(std::span<const QcSource> sources) {
  qc::Compiler c;
  QcResult out;
  auto stop = [&](const std::string& f, int line, const std::string& message) {
    c.diagnostics.push_back({f, line, true, message});
    out.diagnostics = std::move(c.diagnostics);
    return out;
  };
  for (const QcSource& f : sources) {
    c.file = f.name;
    c.file_name = c.intern_string(f.name);
    const Lexed lexed = lex_file(f.text);
    if (!lexed.errors.empty()) {
      for (size_t i = 0; i + 1 < lexed.errors.size(); ++i) {
        c.diagnostics.push_back({f.name, lexed.errors[i].line, true, lexed.errors[i].message});
      }
      return stop(f.name, lexed.errors.back().line, lexed.errors.back().message);
    }
    c.toks = &lexed.tokens;
    c.pos = 0;
    try {
      while (c.peek().kind != Tok::end) {
        if (c.accept(";")) continue;
        c.declaration();
      }
    } catch (const qc::CompileError& e) {
      return stop(f.name, e.line, e.message);
    }
    c.toks = nullptr;
  }
  try {
    c.finish();
  } catch (const qc::CompileError& e) {
    return stop(c.file, e.line, e.message);
  }
  out.ok = true;
  out.progs = std::move(c.prog);
  out.diagnostics = std::move(c.diagnostics);
  return out;
}

QcProject compile_project(const std::filesystem::path& project) {
  QcProject out;
  auto read = [](const std::filesystem::path& p, std::string& text) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    std::ostringstream s;
    s << in.rdbuf();
    text = s.str();
    return true;
  };
  const std::string pname = project.filename().string();
  std::string text;
  if (!read(project, text)) {
    out.result.diagnostics.push_back(
        {project.string(), 0, true, "cannot read the project file: check the path"});
    return out;
  }
  std::vector<std::string> words;
  std::istringstream lines(text);
  std::string line;
  while (std::getline(lines, line)) {
    if (const size_t c = line.find("//"); c != std::string::npos) line.resize(c);
    std::istringstream ws(line);
    std::string w;
    while (ws >> w) words.push_back(w);
  }
  if (words.size() < 2) {
    out.result.diagnostics.push_back(
        {pname, 1, true, "a project file names the output, then at least one source"});
    return out;
  }
  const std::filesystem::path dir = project.parent_path();
  out.output = dir / words[0];
  std::vector<QcSource> files;
  for (size_t i = 1; i < words.size(); ++i) {
    QcSource f;
    f.name = words[i];
    if (!read(dir / words[i], f.text)) {
      out.result.diagnostics.push_back(
          {pname, 0, true, "cannot read the source '" + words[i] + "' the project names"});
      return out;
    }
    files.push_back(std::move(f));
  }
  out.result = compile_qc(files);
  return out;
}

}  // namespace ge::progs
