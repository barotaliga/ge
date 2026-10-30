// A function's body (qc_compiler.hpp): its block of parameters, locals and temporaries, the
// statements and their jumps, and the placing of the block when the function ends.
//
// TEMPORARIES live in the function's block, so a recursive call saves them with the locals. A
// temporary is held until the statement that made it ends; between statements every temporary is
// free, and the next statement reuses them.
//
// ge:summary The QC compiler's function bodies: locals, temporaries, statements, jumps, the block
// ge:keywords qc compiler statement if else while do for break continue return local temporary
// ge:keywords jump patch relocation finish_function local-block
// ge:skill progs
// ge:test tests/progs_qc_compile_test.cpp
#include "progs/qc_compiler.hpp"

namespace ge::progs::qc {

int Compiler::alloc_local(int words) {
  const int ofs = kLocalBase + block_words;
  block_words += words;
  return ofs;
}

int Compiler::declare_local(const std::string& name, int t) {
  if (local_by_name.count(name)) fail("'" + name + "' is declared twice in this function");
  if (size_of(t) == 0) fail("a local cannot be void");
  if (base(t) == kTypeField) fail("a field is declared at file scope: .float name;");
  auto add = [&](const std::string& n, int ty, int ofs) {
    Symbol s;
    s.name = n;
    s.type = ty;
    s.ofs = ofs;
    s.line = peek().line;
    locals.push_back(s);
    local_by_name.emplace(n, static_cast<int>(locals.size()) - 1);
  };
  const int ofs = alloc_local(size_of(t));
  add(name, t, ofs);
  if (base(t) == kTypeVector) {
    for (int k = 0; k < 3; ++k) add(name + "_" + "xyz"[k], t_float, ofs + k);
  }
  return ofs;
}

int Compiler::temp(int words) {
  std::vector<int>& free_list = words == 3 ? free3 : free1;
  int ofs = 0;
  if (!free_list.empty()) {
    ofs = free_list.back();
    free_list.pop_back();
  } else {
    ofs = alloc_local(words);
  }
  (words == 3 ? temps3 : temps1).push_back(ofs);
  return ofs;
}

void Compiler::release_temps() {
  free1.insert(free1.end(), temps1.begin(), temps1.end());
  free3.insert(free3.end(), temps3.begin(), temps3.end());
  temps1.clear();
  temps3.clear();
}

int Compiler::emit(uint16_t op, int a, int b, int c) {
  const int line = pos > 0 ? (*toks)[pos - 1].line : peek().line;
  code.push_back({op, a, b, c, line});
  return here() - 1;
}

void Compiler::patch_jump(int at, int target) {
  Pending& s = code[size_t(at)];
  if (s.op == kOpGoto) {
    s.a = target - at;
  } else {
    s.b = target - at;
  }
}

void Compiler::block() {
  expect("{", "to open the block");
  while (!accept("}")) {
    if (peek().kind == Tok::end) fail("the block is never closed: add }");
    statement();
  }
}

void Compiler::local_declaration() {
  accept("local");
  const int t = parse_type(nullptr);
  for (;;) {
    const std::string name = expect_name("a local's name");
    const int ofs = declare_local(name, t);
    if (accept("=")) {
      const Val v = expression();
      Val target;
      target.type = t;
      target.ofs = ofs;
      target.assignable = true;
      store(target, rv(v), v.type);
      release_temps();
    }
    if (accept(",")) continue;
    expect(";", "to end the declaration");
    return;
  }
}

void Compiler::jump_if_false(const Val& v, std::vector<int>& jumps) {
  const uint16_t b = base(v.type);
  if (b == kTypeVoid) fail("a condition needs a value; this is void");
  const int value = rv(v);
  if (b == kTypeVector) {  // IF / IFNOT test one word: a vector is tested by NOT_V
    const int t = temp(1);
    emit(kOpNotV, value, 0, t);
    jumps.push_back(emit(kOpIf, t, 0, 0));
  } else {
    jumps.push_back(emit(kOpIfNot, value, 0, 0));
  }
}

void Compiler::jump_if_true(const Val& v, int target) {
  const uint16_t b = base(v.type);
  if (b == kTypeVoid) fail("a condition needs a value; this is void");
  const int value = rv(v);
  if (b == kTypeVector) {
    const int t = temp(1);
    emit(kOpNotV, value, 0, t);
    patch_jump(emit(kOpIfNot, t, 0, 0), target);
  } else {
    patch_jump(emit(kOpIf, value, 0, 0), target);
  }
}

void Compiler::statement() {
  release_temps();
  if (is("{")) {
    block();
  } else if (is("local") || is_type_word()) {
    local_declaration();
  } else if (accept(";")) {
  } else if (accept("if")) {
    expect("(", "after if");
    const Val c = expression();
    expect(")", "to close the condition");
    std::vector<int> to_else;
    jump_if_false(c, to_else);
    statement();
    if (accept("else")) {
      const int over = emit(kOpGoto, 0, 0, 0);
      for (int j : to_else) patch_jump(j, here());
      statement();
      patch_jump(over, here());
    } else {
      for (int j : to_else) patch_jump(j, here());
    }
  } else if (accept("while")) {
    const int top = here();
    expect("(", "after while");
    const Val c = expression();
    expect(")", "to close the condition");
    std::vector<int> out;
    jump_if_false(c, out);
    breaks.emplace_back();
    continues.emplace_back();
    statement();
    patch_jump(emit(kOpGoto, 0, 0, 0), top);
    for (int j : out) patch_jump(j, here());
    for (int j : breaks.back()) patch_jump(j, here());
    for (int j : continues.back()) patch_jump(j, top);
    breaks.pop_back();
    continues.pop_back();
  } else if (accept("do")) {
    const int top = here();
    breaks.emplace_back();
    continues.emplace_back();
    statement();
    const int test = here();
    expect("while", "after a do body");
    expect("(", "after while");
    release_temps();
    const Val c = expression();
    expect(")", "to close the condition");
    expect(";", "after do ... while (...)");
    jump_if_true(c, top);
    for (int j : breaks.back()) patch_jump(j, here());
    for (int j : continues.back()) patch_jump(j, test);
    breaks.pop_back();
    continues.pop_back();
  } else if (accept("for")) {
    expect("(", "after for");
    if (!accept(";")) {
      expression();
      expect(";", "after the for loop's start");
    }
    const int top = here();
    std::vector<int> out;
    release_temps();
    if (!accept(";")) {
      const Val c = expression();
      jump_if_false(c, out);
      expect(";", "after the for loop's condition");
    }
    // The step is read after the body: remember where it is and skip it.
    const size_t step = pos;
    for (int depth = 0; !(depth == 0 && is(")"));) {
      if (peek().kind == Tok::end) fail("the for loop's parentheses are never closed");
      if (is("(")) ++depth;
      if (is(")")) --depth;
      next();
    }
    expect(")", "to close the for loop");
    breaks.emplace_back();
    continues.emplace_back();
    statement();
    const int cont = here();
    const size_t after = pos;
    pos = step;
    release_temps();
    if (!is(")")) expression();
    pos = after;
    patch_jump(emit(kOpGoto, 0, 0, 0), top);
    for (int j : out) patch_jump(j, here());
    for (int j : breaks.back()) patch_jump(j, here());
    for (int j : continues.back()) patch_jump(j, cont);
    breaks.pop_back();
    continues.pop_back();
  } else if (accept("return")) {
    if (accept(";")) {
      if (base(fn_return) != kTypeVoid) {
        fail("this function returns " + type_name(fn_return) + ": return a value");
      }
      emit(kOpReturn, 0, 0, 0);
      return;
    }
    const Val v = expression();
    if (base(fn_return) == kTypeVoid) fail("this function returns void: write return;");
    check_assign(fn_return, v.type, "the return value");
    emit(kOpReturn, rv(v), 0, 0);
    expect(";", "after the return value");
  } else if (accept("break") || accept("continue")) {
    const bool is_break = (*toks)[pos - 1].text == "break";
    if (breaks.empty()) fail(std::string(is_break ? "break" : "continue") + " outside a loop");
    (is_break ? breaks : continues).back().push_back(emit(kOpGoto, 0, 0, 0));
    expect(";", is_break ? "after break" : "after continue");
  } else {
    expression();
    expect(";", "to end the statement");
  }
}

void Compiler::finish_function(int sym) {
  const int start = alloc_global(block_words);
  const auto place = [&](int v) {
    const int out = v >= kLocalBase ? v - kLocalBase + start : v;
    if (out < -32768 || out > 32767) {
      fail("the program outgrew the format's 16-bit operands: split the mod or share constants");
    }
    return static_cast<int16_t>(out);
  };
  Symbol& s = globals[size_t(sym)];
  Function& f = prog.functions[size_t(s.function)];
  f.first_statement = static_cast<int32_t>(prog.statements.size());
  f.parm_start = start;
  f.locals = block_words;
  f.file = file_name;
  for (const Pending& p : code) {
    prog.statements.push_back({p.op, place(p.a), place(p.b), place(p.c)});
    prog.lines.push_back(p.line);
  }
  for (const Symbol& l : locals) add_global_def(l.name, l.type, l.ofs - kLocalBase + start, false);
  s.defined = true;
  code.clear();
}

}  // namespace ge::progs::qc
