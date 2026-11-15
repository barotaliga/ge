// The console's expressions (expr.hpp): a tokenizer and a recursive descent over the precedence
// levels, evaluated as it parses.
//
// ge:summary The console's expression evaluator: tokens, precedence, values
// ge:keywords console expression expr eval tokenizer recursive-descent
// ge:skill console
// ge:test tests/console_expr_test.cpp
#include "console/expr.hpp"

#include <regex>
#include <vector>

#include "console/cvar.hpp"

namespace ge::console {

namespace {

struct Token {
  enum class Kind : unsigned char { value, op, open, close };
  Kind kind = Kind::value;
  std::string text;  // an operator's spelling, or a value's text
  bool quoted = false;
  bool cvar = false;  // %name
};

bool is_op_char(char ch) {
  return ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '<' || ch == '>' || ch == '=' ||
         ch == '!' || ch == '~' || ch == '&' || ch == '|';
}

bool word_op(const std::string& w) {
  return w == "and" || w == "or" || w == "isin" || w == "!isin";
}

bool tokenize(std::string_view s, std::vector<Token>& out, std::string& error) {
  size_t i = 0;
  while (i < s.size()) {
    const char ch = s[i];
    if (ch == ' ' || ch == '\t') {
      ++i;
    } else if (ch == '(' || ch == ')') {
      out.push_back({ch == '(' ? Token::Kind::open : Token::Kind::close, std::string(1, ch)});
      ++i;
    } else if (ch == '"' || ch == '\'') {
      const size_t end = s.find(ch, i + 1);
      if (end == std::string_view::npos) {
        error = std::string("the text starting ") + ch + " has no closing " + ch;
        return false;
      }
      out.push_back({Token::Kind::value, std::string(s.substr(i + 1, end - i - 1)), true});
      i = end + 1;
    } else if (ch == '!' && s.substr(i, 5) == "!isin") {
      out.push_back({Token::Kind::op, "!isin"});
      i += 5;
    } else if (is_op_char(ch)) {
      static const char* const kOps[] = {"==", "!=", "<>", "<=", ">=", "=~", "!~", "&&",
                                         "||", "=",  "<",  ">",  "+",  "-",  "*",  "/"};
      bool found = false;
      for (const char* op : kOps) {
        const std::string_view o(op);
        if (s.substr(i, o.size()) == o) {
          out.push_back({Token::Kind::op, std::string(o)});
          i += o.size();
          found = true;
          break;
        }
      }
      if (!found) {
        error = std::string("'") + ch + "' is not an operator";
        return false;
      }
    } else {
      size_t end = i;
      while (end < s.size() && s[end] != ' ' && s[end] != '\t' && s[end] != '(' && s[end] != ')' &&
             !(is_op_char(s[end]) && end > i && s[end] != '!'))
        ++end;
      // A number keeps its point and its exponent's sign; a word stops at an operator.
      std::string w(s.substr(i, end - i));
      const std::string lw = lower(w);
      if (word_op(lw)) {
        out.push_back({Token::Kind::op, lw});
      } else if (w.size() > 1 && w[0] == '%') {
        out.push_back({Token::Kind::value, w.substr(1), false, true});
      } else {
        out.push_back({Token::Kind::value, w});
      }
      i = end;
    }
  }
  return true;
}

ExprValue integer(double x) { return {ExprValue::Kind::integer, x, {}}; }

ExprValue from_text(const std::string& t, bool quoted) {
  double x = 0;
  if (!quoted && parse_number(t, x)) {
    const bool whole = t.find_first_of(".eE") == std::string::npos;
    return {whole ? ExprValue::Kind::integer : ExprValue::Kind::number, x, {}};
  }
  return {ExprValue::Kind::text, 0, t};
}

bool is_num(const ExprValue& v) { return v.kind != ExprValue::Kind::text; }

class Parser {
 public:
  Parser(const std::vector<Token>& t, const Cvars* cvars, std::string& error)
      : t_(t), cvars_(cvars), error_(error) {}

  bool parse(ExprValue& out) {
    if (t_.empty()) {
      error_ = "the expression is empty";
      return false;
    }
    if (!logic_or(out)) return false;
    if (at_ < t_.size()) {
      error_ = "'" + t_[at_].text + "' after the end of the expression";
      return false;
    }
    return true;
  }

 private:
  bool peek_op(std::initializer_list<const char*> ops, std::string& which) const {
    if (at_ >= t_.size() || t_[at_].kind != Token::Kind::op) return false;
    for (const char* o : ops) {
      if (t_[at_].text == o) {
        which = o;
        return true;
      }
    }
    return false;
  }

  bool logic_or(ExprValue& v) {
    if (!logic_and(v)) return false;
    std::string op;
    while (peek_op({"or", "||"}, op)) {
      ++at_;
      ExprValue r;
      if (!logic_and(r)) return false;
      v = integer(expr_truthy(v) || expr_truthy(r) ? 1 : 0);
    }
    return true;
  }

  bool logic_and(ExprValue& v) {
    if (!compare(v)) return false;
    std::string op;
    while (peek_op({"and", "&&"}, op)) {
      ++at_;
      ExprValue r;
      if (!compare(r)) return false;
      v = integer(expr_truthy(v) && expr_truthy(r) ? 1 : 0);
    }
    return true;
  }

  bool compare(ExprValue& v) {
    if (!additive(v)) return false;
    std::string op;
    if (!peek_op({"=", "==", "!=", "<>", "<", "<=", ">", ">=", "isin", "!isin", "=~", "!~"}, op))
      return true;
    ++at_;
    ExprValue r;
    if (!additive(r)) return false;
    const std::string a = expr_text(v), b = expr_text(r);
    bool yes = false;
    if (op == "isin" || op == "!isin") {
      yes = (b.find(a) != std::string::npos) == (op == "isin");
    } else if (op == "=~" || op == "!~") {
      try {
        yes = std::regex_search(a, std::regex(b)) == (op == "=~");
      } catch (const std::regex_error&) {
        error_ = "'" + b + "' is not a regular expression";
        return false;
      }
    } else {
      const bool nums = is_num(v) && is_num(r);
      const double x = v.num, y = r.num;
      if (op == "=" || op == "==") yes = nums ? x == y : a == b;
      if (op == "!=" || op == "<>") yes = nums ? x != y : a != b;
      if (op == "<") yes = nums ? x < y : a < b;
      if (op == "<=") yes = nums ? x <= y : a <= b;
      if (op == ">") yes = nums ? x > y : a > b;
      if (op == ">=") yes = nums ? x >= y : a >= b;
    }
    v = integer(yes ? 1 : 0);
    return true;
  }

  bool additive(ExprValue& v) {
    if (!multiplicative(v)) return false;
    std::string op;
    while (peek_op({"+", "-"}, op)) {
      ++at_;
      ExprValue r;
      if (!multiplicative(r)) return false;
      if (op == "+" && (!is_num(v) || !is_num(r))) {
        v = {ExprValue::Kind::text, 0, expr_text(v) + expr_text(r)};
        continue;
      }
      if (!numbers(v, r, op)) return false;
      v = arith(v, r, op == "+" ? v.num + r.num : v.num - r.num);
    }
    return true;
  }

  bool multiplicative(ExprValue& v) {
    if (!unary(v)) return false;
    std::string op;
    while (peek_op({"*", "/"}, op)) {
      ++at_;
      ExprValue r;
      if (!unary(r)) return false;
      if (!numbers(v, r, op)) return false;
      if (op == "*") {
        v = arith(v, r, v.num * r.num);
      } else {
        if (r.num == 0) {
          error_ = "division by zero";
          return false;
        }
        v = {ExprValue::Kind::number, v.num / r.num, {}};
      }
    }
    return true;
  }

  bool unary(ExprValue& v) {
    std::string op;
    if (peek_op({"-"}, op)) {
      ++at_;
      if (!unary(v)) return false;
      if (!is_num(v)) {
        error_ = "'-' before the text '" + v.text + "'";
        return false;
      }
      v.num = -v.num;
      return true;
    }
    return primary(v);
  }

  bool primary(ExprValue& v) {
    if (at_ >= t_.size()) {
      error_ = "the expression ends where a value was expected";
      return false;
    }
    const Token& tk = t_[at_++];
    if (tk.kind == Token::Kind::open) {
      if (!logic_or(v)) return false;
      if (at_ >= t_.size() || t_[at_].kind != Token::Kind::close) {
        error_ = "a '(' is not closed";
        return false;
      }
      ++at_;
      return true;
    }
    if (tk.kind != Token::Kind::value) {
      error_ = "'" + tk.text + "' where a value was expected";
      return false;
    }
    if (tk.cvar) {
      const Cvar* c = cvars_ == nullptr ? nullptr : cvars_->find(tk.text);
      v = from_text(c == nullptr ? std::string() : c->value, false);
      return true;
    }
    v = from_text(tk.text, tk.quoted);
    return true;
  }

  bool numbers(const ExprValue& a, const ExprValue& b, const std::string& op) {
    if (is_num(a) && is_num(b)) return true;
    error_ = "'" + op + "' needs numbers, not '" + expr_text(is_num(a) ? b : a) + "'";
    return false;
  }

  static ExprValue arith(const ExprValue& a, const ExprValue& b, double r) {
    const bool ints = a.kind == ExprValue::Kind::integer && b.kind == ExprValue::Kind::integer;
    return {ints ? ExprValue::Kind::integer : ExprValue::Kind::number, r, {}};
  }

  const std::vector<Token>& t_;
  const Cvars* cvars_;
  std::string& error_;
  size_t at_ = 0;
};

}  // namespace

bool eval_expr(std::string_view src, const Cvars* cvars, ExprValue& out, std::string& error) {
  std::vector<Token> tokens;
  if (!tokenize(src, tokens, error)) return false;
  Parser p(tokens, cvars, error);
  return p.parse(out);
}

bool expr_truthy(const ExprValue& v) {
  return v.kind == ExprValue::Kind::text ? !v.text.empty() : v.num != 0;
}

std::string expr_text(const ExprValue& v) {
  return v.kind == ExprValue::Kind::text ? v.text : number_text(v.num);
}

std::string expr_describe(const ExprValue& v) {
  switch (v.kind) {
    case ExprValue::Kind::integer:
      return number_text(v.num) + " (integer)";
    case ExprValue::Kind::number:
      return number_text(v.num) + " (number)";
    case ExprValue::Kind::text:
      break;
  }
  return "\"" + v.text + "\" (text)";
}

}  // namespace ge::console
