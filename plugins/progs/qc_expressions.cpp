// Expressions (qc_compiler.hpp): names, literals, calls, fields and vector members, the operators
// by precedence, assignments, and the type rules that pick each opcode.
//
// A CALL evaluates every argument before the first is stored in its parameter slot (so a call
// inside an argument cannot overwrite another argument), and copies a result out of the return
// slot into a temporary at once (so the next call cannot overwrite it).
//
// ge:summary The QC compiler's expressions: operators, calls, fields, members, assignment, types
// ge:keywords qc compiler expression operator precedence call argument field member assignment
// ge:keywords compound-assignment type-check truth and or not negate dot-product rv store
// ge:skill progs
// ge:test tests/progs_qc_compile_test.cpp
#include <bit>

#include "progs/qc_compiler.hpp"

namespace ge::progs::qc {

namespace {

int precedence(const Token& t) {
  if (t.kind != Tok::punct) return -1;
  const std::string& s = t.text;
  if (s == "||") return 1;
  if (s == "&&") return 2;
  if (s == "|") return 3;
  if (s == "&") return 4;
  if (s == "==" || s == "!=") return 5;
  if (s == "<" || s == ">" || s == "<=" || s == ">=") return 6;
  if (s == "+" || s == "-") return 7;
  if (s == "*" || s == "/") return 8;
  return -1;
}

}  // namespace

const Symbol* Compiler::lookup(std::string_view name) const {
  if (const auto it = local_by_name.find(name); it != local_by_name.end()) {
    return &locals[size_t(it->second)];
  }
  if (const auto it = global_by_name.find(name); it != global_by_name.end()) {
    return &globals[size_t(it->second)];
  }
  return nullptr;
}

Val Compiler::temp_val(int t) {
  Val v;
  v.type = t;
  v.ofs = temp(size_of(t) == 3 ? 3 : 1);
  return v;
}

int Compiler::rv(const Val& v) {
  if (v.kind == Val::Kind::global) return v.ofs;
  const int t = temp(size_of(v.type) == 3 ? 3 : 1);
  emit(load_op(base(v.type)), v.ofs, v.field_ofs, t);
  return t;
}

void Compiler::check_assign(int to, int from, std::string_view what) const {
  if (to == from) return;
  if (base(to) == kTypeFunction && base(from) == kTypeFunction) return;
  fail(std::string(what) + " wants " + type_name(to) + " but this is " + type_name(from));
}

void Compiler::store(const Val& target, int value_ofs, int value_type) {
  if (!target.assignable) fail("this cannot be assigned: it is a constant or a computed value");
  check_assign(target.type, value_type, "the assignment");
  const uint16_t b = base(target.type);
  if (target.kind == Val::Kind::global) {
    emit(store_op(b), value_ofs, target.ofs, 0);
  } else {
    const int p = temp(1);
    emit(kOpAddress, target.ofs, target.field_ofs, p);
    emit(storep_op(b), value_ofs, p, 0);
  }
}

int Compiler::truth(int ofs, int t) {
  if (base(t) == kTypeFloat) return ofs;
  const int n = temp(1);
  emit(not_op(base(t)), ofs, 0, n);
  const int y = temp(1);
  emit(kOpNotF, n, 0, y);
  return y;
}

Val Compiler::expression() { return assignment(); }

Val Compiler::assignment() {
  const Val left = binary(1);
  const Token& t = peek();
  if (t.kind != Tok::punct) return left;
  const std::string op = t.text;
  if (op != "=" && op != "+=" && op != "-=" && op != "*=" && op != "/=") return left;
  next();
  const Val right = assignment();
  Val value;
  if (op == "=") {
    value.type = right.type;
    value.ofs = rv(right);
  } else {
    if (!left.assignable) fail("this cannot be assigned: it is a constant or a computed value");
    const Val r = binop(op.substr(0, 1), left, right);
    value.type = r.type;
    value.ofs = rv(r);
  }
  store(left, value.ofs, value.type);
  return value;
}

Val Compiler::binary(int min_prec) {
  Val left = unary();
  for (;;) {
    const int prec = precedence(peek());
    if (prec < min_prec) return left;
    const std::string op = next().text;
    if (left.kind == Val::Kind::field) {  // read it before the right side runs
      Val loaded;
      loaded.type = left.type;
      loaded.ofs = rv(left);
      left = loaded;
    }
    const Val right = binary(prec + 1);
    left = binop(op, left, right);
  }
}

Val Compiler::binop(std::string_view op, const Val& l, const Val& r) {
  const uint16_t lb = base(l.type);
  const uint16_t rb = base(r.type);
  const int a = rv(l);
  const int b = rv(r);
  auto make = [&](uint16_t opcode, int t) {
    Val v = temp_val(t);
    emit(opcode, a, b, v.ofs);
    return v;
  };
  const bool ff = lb == kTypeFloat && rb == kTypeFloat;
  const bool vv = lb == kTypeVector && rb == kTypeVector;
  if (op == "+") {
    if (ff) return make(kOpAddF, t_float);
    if (vv) return make(kOpAddV, t_vector);
  } else if (op == "-") {
    if (ff) return make(kOpSubF, t_float);
    if (vv) return make(kOpSubV, t_vector);
  } else if (op == "*") {
    if (ff) return make(kOpMulF, t_float);
    if (vv) return make(kOpMulV, t_float);
    if (lb == kTypeFloat && rb == kTypeVector) return make(kOpMulFV, t_vector);
    if (lb == kTypeVector && rb == kTypeFloat) return make(kOpMulVF, t_vector);
  } else if (op == "/") {
    if (ff) return make(kOpDivF, t_float);
    if (lb == kTypeVector && rb == kTypeFloat) {
      const int inv = temp(1);
      emit(kOpDivF, float_const(1), b, inv);
      Val v = temp_val(t_vector);
      emit(kOpMulVF, a, inv, v.ofs);
      return v;
    }
  } else if (op == "==" || op == "!=") {
    const bool eq = op == "==";
    const bool same = lb == rb;
    if (same && lb == kTypeFloat) return make(eq ? kOpEqF : kOpNeF, t_float);
    if (same && lb == kTypeVector) return make(eq ? kOpEqV : kOpNeV, t_float);
    if (same && lb == kTypeString) return make(eq ? kOpEqS : kOpNeS, t_float);
    if (same && (lb == kTypeEntity || lb == kTypeField)) return make(eq ? kOpEqE : kOpNeE, t_float);
    if (same && lb == kTypeFunction) return make(eq ? kOpEqFnc : kOpNeFnc, t_float);
  } else if (op == "<" || op == ">" || op == "<=" || op == ">=") {
    if (ff) {
      return make(op == "<" ? kOpLt : op == ">" ? kOpGt : op == "<=" ? kOpLe : kOpGe, t_float);
    }
  } else if (op == "&&" || op == "||") {
    if (lb != kTypeVoid && rb != kTypeVoid) {
      const int ta = truth(a, l.type);
      const int tb = truth(b, r.type);
      Val v = temp_val(t_float);
      emit(op == "&&" ? kOpAnd : kOpOr, ta, tb, v.ofs);
      return v;
    }
  } else if (op == "&" || op == "|") {
    if (ff) return make(op == "&" ? kOpBitAnd : kOpBitOr, t_float);
  }
  fail("the operator " + std::string(op) + " does not take " + type_name(l.type) + " and " +
       type_name(r.type));
}

Val Compiler::unary() {
  if (accept("!")) {
    const Val v = unary();
    if (base(v.type) == kTypeVoid) fail("! needs a value; this is void");
    const int a = rv(v);
    Val out = temp_val(t_float);
    emit(not_op(base(v.type)), a, 0, out.ofs);
    return out;
  }
  if (accept("-")) {
    if (peek().kind == Tok::number) {  // a literal's own sign: `-1` is the constant -1
      Val v;
      v.type = t_float;
      v.ofs = float_const(-next().number);
      return v;
    }
    const Val v = unary();
    const int a = rv(v);
    if (base(v.type) == kTypeFloat) {
      Val out = temp_val(t_float);
      emit(kOpSubF, float_const(0), a, out.ofs);
      return out;
    }
    if (base(v.type) == kTypeVector) {
      Val out = temp_val(t_vector);
      emit(kOpSubV, immediate(kTypeVector, {0, 0, 0}), a, out.ofs);
      return out;
    }
    fail("unary - takes a float or a vector, not " + type_name(v.type));
  }
  return postfix();
}

Val Compiler::postfix() {
  Val v = primary();
  for (;;) {
    if (accept("(")) {
      v = call(v);
    } else if (accept(".")) {
      v = member(v);
    } else {
      return v;
    }
  }
}

Val Compiler::primary() {
  const Token& t = next();
  Val v;
  switch (t.kind) {
    case Tok::number:
      v.type = t_float;
      v.ofs = float_const(t.number);
      return v;
    case Tok::string:
      v.type = t_string;
      v.ofs = immediate(kTypeString, {static_cast<uint32_t>(intern_string(t.text))});
      return v;
    case Tok::vector:
      v.type = t_vector;
      v.ofs = immediate(kTypeVector,
                        {std::bit_cast<uint32_t>(t.vec[0]), std::bit_cast<uint32_t>(t.vec[1]),
                         std::bit_cast<uint32_t>(t.vec[2])});
      return v;
    case Tok::name: {
      const Symbol* s = lookup(t.text);
      if (!s) {
        fail_at(t.line, "unknown name '" + t.text + "': declare it before it is used");
      }
      v.type = s->type;
      v.ofs = s->ofs;
      v.assignable = !s->constant;
      return v;
    }
    case Tok::punct:
      if (t.text == "(") {
        v = expression();
        expect(")", "to close the parentheses");
        return v;
      }
      [[fallthrough]];
    default:
      fail_at(t.line, t.kind == Tok::end ? "expected an expression, got the end of the file"
                                         : "expected an expression, got '" + t.text + "'");
  }
}

Val Compiler::member(const Val& left) {
  const int line = peek().line;
  const std::string name = expect_name("a field or member name after .");
  const uint16_t lb = base(left.type);
  if (lb == kTypeVector && name.size() == 1 && (name == "x" || name == "y" || name == "z")) {
    const int k = name[0] - 'x';
    Val v = left;
    v.type = t_float;
    if (left.kind == Val::Kind::global) {
      v.ofs = left.ofs + k;
    } else {  // a component of a vector field: the field's offset plus k
      if (left.field_ofs >= kLocalBase) fail("a field's offset is a file-scope global");
      const uint32_t at = prog.globals[size_t(left.field_ofs)] + static_cast<uint32_t>(k);
      v.field_ofs = immediate(kTypeField, {at});
    }
    return v;
  }
  if (lb != kTypeEntity) {
    fail_at(line, "." + name + " needs an entity on its left, not " + type_name(left.type));
  }
  // A field is named at file scope: a local of the same name does not hide it.
  const Symbol* f = lookup(name);
  if (f && base(f->type) != kTypeField) {
    const auto it = global_by_name.find(name);
    f = it == global_by_name.end() ? nullptr : &globals[size_t(it->second)];
  }
  if (!f || base(f->type) != kTypeField) {
    fail_at(line, "'" + name + "' is not a field: declare it with ." + std::string("float ") +
                      name + ";");
  }
  Val v;
  v.kind = Val::Kind::field;
  v.type = type(f->type).inner;
  v.ofs = rv(left);
  v.field_ofs = f->ofs;
  v.assignable = true;
  return v;
}

Val Compiler::call(const Val& callee) {
  const Type ft = type(callee.type);
  if (ft.base != kTypeFunction) fail("this is " + type_name(callee.type) + ", not a function");
  const int fn = rv(callee);
  std::vector<std::pair<int, int>> args;  // global, type
  if (!accept(")")) {
    for (;;) {
      const Val a = expression();
      if (base(a.type) == kTypeVoid) fail("an argument needs a value; this is void");
      args.emplace_back(rv(a), a.type);
      if (accept(",")) continue;
      expect(")", "to close the call");
      break;
    }
  }
  if (args.size() > static_cast<size_t>(kMaxParms)) fail("a call passes at most 8 arguments");
  if (args.size() < ft.params.size() || (!ft.varargs && args.size() > ft.params.size())) {
    fail("this function takes " + std::to_string(ft.params.size()) +
         (ft.varargs ? " or more" : "") + " arguments (" + type_name(callee.type) + "), got " +
         std::to_string(args.size()));
  }
  for (size_t i = 0; i < ft.params.size(); ++i) {
    check_assign(ft.params[i], args[i].second, "argument " + std::to_string(i + 1));
  }
  for (size_t i = 0; i < args.size(); ++i) {
    emit(store_op(base(args[i].second)), args[i].first, kGlobalParm0 + 3 * static_cast<int>(i), 0);
  }
  emit(static_cast<uint16_t>(kOpCall0 + args.size()), fn, 0, 0);
  Val out;
  out.type = ft.inner;
  if (base(ft.inner) == kTypeVoid) return out;
  out = temp_val(ft.inner);
  emit(store_op(base(ft.inner)), kGlobalReturn, out.ofs, 0);
  return out;
}

}  // namespace ge::progs::qc
