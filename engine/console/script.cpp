// The console's scripting commands, the part of the language that makes a cfg a program:
//
//   if <a> [<op> <b>] [then] <commands> [else <commands>]
//       op: == = != <> < <= > >= (numbers when both sides are; else text), isin and !isin (a is
//       a substring of b), =~ and !~ (b a regular expression found in a). With no op, a is true
//       unless it is empty or 0. `then` may be left out after `a op b`, as old configs write it.
//       The condition is expanded when `if` runs; the branch runs (and is expanded) after it,
//       before what followed.
//   if (<expression>) then <commands> [else <commands>]     expr.hpp's language
//   if_exists <cvar|alias|trigger|command> <name> <commands> [<else commands>]
//   eval <expression>   set_eval <name> <expression>
//   set_calc <name> <a> <op> <b>    op: + - * / % div (whole numbers) and or xor (bits) and the
//       comparisons (1 or 0); a division by zero stores 999, which old configs test for
//   set_calc <name> strlen <s> | substr <s> <from> [<count>] | strstr <s> <find> | pos <s> <find>
//       | int <x> | tobrown <s> | towhite <s> | <a>
//   inc <name> [<by>]   dec <name> [<by>]   mul <name> [<by>]   div <name> [<by>]
//   toggle <name>   cycle <name> <v1> <v2> ...
//   msg_trigger [<alias> [<match>]]     a line from the game containing match runs the alias
//   msg_trigger_remove <alias>
//
// ge:summary The console's scripting: if/else, if_exists, eval, set_calc, inc, cycle, triggers
// ge:keywords console script if else then set_calc calc arithmetic strlen substr strstr inc dec
// ge:keywords if_exists eval set_eval div xor tobrown towhite pos regex
// ge:keywords mul div toggle cycle msg_trigger trigger conditional compare isin
// ge:skill console
// ge:test tests/console_script_test.cpp
#include <cmath>
#include <cstdint>
#include <regex>

#include "console/console.hpp"
#include "console/expr.hpp"
#include "console/lex.hpp"

namespace ge::console {

namespace {

// 1 when the comparison holds, 0 when not, -1 when `op` is not one.
int compare(const std::string& a, const std::string& op, const std::string& b) {
  double x = 0, y = 0;
  const bool nums = parse_number(a, x) && parse_number(b, y);
  if (op == "isin") return b.find(a) != std::string::npos ? 1 : 0;
  if (op == "!isin") return b.find(a) == std::string::npos ? 1 : 0;
  if (op == "==" || op == "=") return (nums ? x == y : a == b) ? 1 : 0;
  if (op == "!=" || op == "<>") return (nums ? x != y : a != b) ? 1 : 0;
  if (op == "<") return (nums ? x < y : a < b) ? 1 : 0;
  if (op == "<=") return (nums ? x <= y : a <= b) ? 1 : 0;
  if (op == ">") return (nums ? x > y : a > b) ? 1 : 0;
  if (op == ">=") return (nums ? x >= y : a >= b) ? 1 : 0;
  if (op == "=~" || op == "!~") {
    try {
      return std::regex_search(a, std::regex(b)) == (op == "=~") ? 1 : 0;
    } catch (const std::regex_error&) {
      return -2;
    }
  }
  return -1;
}

bool is_compare_op(const std::string& op) {
  static const char* const kOps[] = {
      "==", "=", "!=", "<>", "<", "<=", ">", ">=", "isin", "!isin", "=~", "!~"};
  for (const char* o : kOps)
    if (op == o) return true;
  return false;
}

bool truthy(const std::string& a) {
  double x = 0;
  if (parse_number(a, x)) return x != 0;
  return !a.empty();
}

// The words from..to-1 joined by blanks.
std::string join_words(const Args& a, size_t from, size_t to) {
  std::string s;
  for (size_t i = from; i < to; ++i) {
    if (i > from) s += ' ';
    s += a[i];
  }
  return s;
}

// The expression form's condition as written: the text before the first `then` that stands as a
// word outside quotes.
std::string condition_text(const std::string& raw) {
  char quote_ch = 0;
  for (size_t i = 0; i < raw.size(); ++i) {
    const char ch = raw[i];
    if (quote_ch != 0) {
      if (ch == quote_ch) quote_ch = 0;
      continue;
    }
    if (ch == '"' || ch == '\'') {
      quote_ch = ch;
      continue;
    }
    const bool starts = i == 0 || raw[i - 1] == ' ' || raw[i - 1] == '\t';
    const bool ends = i + 4 == raw.size() || raw[i + 4] == ' ' || raw[i + 4] == '\t';
    if (starts && i + 4 <= raw.size() && ends && lower(raw.substr(i, 4)) == "then")
      return raw.substr(0, i);
  }
  return raw;
}

// THE TWO FORMS OF `if`. The classic one tests `a op b` (or one word's truth); `then` may be
// left out when the condition is three words with a comparison in the middle, as old configs
// write it. The expression form starts with `(` and runs to `then` (expr.hpp).
void if_cmd(Console& c, const Args& a) {
  size_t then = 0;
  for (size_t i = 1; i < a.size(); ++i) {
    if (lower(a[i]) == "then") {
      then = i;
      break;
    }
  }
  const bool expression = a.size() > 1 && !a[1].empty() && a[1][0] == '(';
  size_t cond_end = then, branch = then + 1;
  if (then == 0) {
    if (expression || a.size() < 5 || !is_compare_op(a[2])) {
      c.error("if needs \"then\"", "usage: if <a> <op> <b> [then] <commands> [else <commands>]");
      return;
    }
    cond_end = 4;
    branch = 4;
  }
  size_t els = 0;
  for (size_t i = branch; i < a.size(); ++i) {
    if (lower(a[i]) == "else") {
      els = i;
      break;
    }
  }
  bool yes = false;
  if (expression) {
    ExprValue v;
    std::string why;
    if (!eval_expr(c.expand_text(condition_text(a.rest(0))), &c.cvars(), v, why)) {
      c.error("if: " + why, "check the expression between ( and then");
      return;
    }
    yes = expr_truthy(v);
  } else {
    std::vector<std::string> cond;
    for (size_t i = 1; i < cond_end; ++i) cond.push_back(c.expand_text(a[i]));
    if (cond.size() == 1) {
      yes = truthy(cond[0]);
    } else if (cond.size() == 3) {
      const int r = compare(cond[0], cond[1], cond[2]);
      if (r == -1) {
        c.error("if: '" + cond[1] + "' is not a comparison",
                "use == = != <> < <= > >= isin !isin =~ or !~");
        return;
      }
      if (r == -2) {
        c.error("if: '" + cond[2] + "' is not a regular expression",
                "check the pattern after " + cond[1]);
        return;
      }
      yes = r == 1;
    } else {
      c.error("if: the condition is " + std::to_string(cond.size()) + " words",
              "usage: if <a> then ... or if <a> <op> <b> then ...; quote a value with spaces");
      return;
    }
  }
  const std::string body = yes ? join_words(a, branch, els == 0 ? a.size() : els)
                               : (els == 0 ? "" : join_words(a, els + 1, a.size()));
  if (!body.empty()) c.insert(body, a.source, a.where);
}

// if_exists <cvar|alias|trigger|command> <name> <then> [<else>]
void if_exists(Console& c, const Args& a) {
  if (a.size() < 4) {
    c.error("if_exists needs a kind, a name and a command",
            "usage: if_exists <cvar|alias|trigger|command> <name> <command> [<else command>]");
    return;
  }
  const std::string kind = lower(a[1]), name = lower(a[2]);
  bool yes = false;
  if (kind == "cvar") {
    yes = c.cvars().find(name) != nullptr;
  } else if (kind == "alias") {
    yes = c.aliases().count(name) != 0;
  } else if (kind == "trigger") {
    for (const auto& t : c.triggers()) yes = yes || t.alias == name;
  } else if (kind == "command") {
    yes = c.has_command(name);
  } else {
    c.error("if_exists: '" + kind + "' is not a kind", "use cvar, alias, trigger or command");
    return;
  }
  const std::string& body = yes ? a[3] : a[4];
  if (!body.empty()) c.insert(body, a.source, a.where);
}

void store(Console& c, const Args& a, const std::string& name, const std::string& value) {
  const SetResult r = c.cvars().set(name, value, a.source, true);
  const std::string why = set_result_text(r, lower(name));
  if (!why.empty()) c.error(why, "");
}

std::string value_of(Console& c, const std::string& name) {
  const Cvar* v = c.cvars().find(name);
  return v == nullptr ? std::string() : v->value;
}

void set_calc(Console& c, const Args& a) {
  if (a.size() < 3) {
    c.error("set_calc needs a name and an expression", "usage: set_calc <name> <a> <op> <b>");
    return;
  }
  const std::string fn = lower(a[2]);
  auto num = [&](const std::string& s, double& x) {
    if (parse_number(s, x)) return true;
    c.error("set_calc: '" + s + "' is not a number", "check the cvar expanded to a number");
    return false;
  };
  if (fn == "strlen" && a.size() == 4) return store(c, a, a[1], std::to_string(a[3].size()));
  if ((fn == "strstr" || fn == "pos") && a.size() == 5) {
    const size_t at = a[3].find(a[4]);
    return store(c, a, a[1], at == std::string::npos ? "-1" : std::to_string(at));
  }
  if (fn == "substr" && (a.size() == 5 || a.size() == 6)) {
    double from = 0, count = 1;
    if (!num(a[4], from) || (a.size() == 6 && !num(a[5], count))) return;
    const size_t f = from < 0 ? 0 : static_cast<size_t>(from);
    const size_t n = count < 0 ? 0 : static_cast<size_t>(count);
    return store(c, a, a[1], f >= a[3].size() ? std::string() : a[3].substr(f, n));
  }
  if (fn == "int" && a.size() == 4) {
    double x = 0;
    if (!num(a[3], x)) return;
    return store(c, a, a[1], number_text(std::trunc(x)));
  }
  if ((fn == "tobrown" || fn == "towhite") && a.size() == 4) {
    // The font's two halves: brown sets each character's top bit, white clears it.
    std::string t = a[3];
    for (char& ch : t) {
      const auto u = static_cast<unsigned char>(ch);
      ch = static_cast<char>(fn == "tobrown" ? (u | 0x80u) : (u & 0x7fu));
    }
    return store(c, a, a[1], t);
  }
  if (a.size() == 3) return store(c, a, a[1], a[2]);
  if (a.size() != 5) {
    c.error("set_calc: cannot read '" + a.rest(1) + "'",
            "usage: set_calc <name> <a> <op> <b>, or strlen, substr, strstr or int");
    return;
  }
  const std::string& op = a[3];
  const int cmp = compare(a[2], op, a[4]);
  if (cmp >= 0 && op != "isin" && op != "!isin") return store(c, a, a[1], cmp ? "1" : "0");
  double x = 0, y = 0;
  if (!num(a[2], x) || !num(a[4], y)) return;
  double r = 0;
  if (op == "+") {
    r = x + y;
  } else if (op == "-") {
    r = x - y;
  } else if (op == "*") {
    r = x * y;
  } else if (op == "/" || op == "%" || op == "div") {
    if (y == 0) {
      // Old configs test for the 999 a division by zero has always left.
      c.error("set_calc: division by zero", "check the divisor's cvar; " + a[1] + " is set to 999");
      return store(c, a, a[1], "999");
    }
    if (op == "/") {
      r = x / y;
    } else if (op == "div") {
      r = std::trunc(std::trunc(x) / std::trunc(y));
    } else {
      r = std::fmod(x, y);
    }
  } else if (op == "and" || op == "or" || op == "xor") {
    const auto i = static_cast<int64_t>(x), j = static_cast<int64_t>(y);
    r = static_cast<double>(op == "and" ? (i & j) : op == "or" ? (i | j) : (i ^ j));
  } else {
    c.error("set_calc: '" + op + "' is not an operator",
            "use + - * / % div and or xor or a comparison");
    return;
  }
  store(c, a, a[1], number_text(r));
}

void step(Console& c, const Args& a, double sign) {
  if (a.size() < 2) {
    c.error(a[0] + " needs a cvar", "write `" + a[0] + " <name> [<by>]`");
    return;
  }
  double x = 0, by = 1;
  parse_number(value_of(c, a[1]), x);
  if (a.size() > 2 && !parse_number(a[2], by)) {
    c.error(a[0] + ": '" + a[2] + "' is not a number", "write `" + a[0] + " <name> [<by>]`");
    return;
  }
  store(c, a, a[1], number_text(x + sign * by));
}

// `mul` and `div`: a cvar times or over a number (1 when left out).
void scale(Console& c, const Args& a, bool over) {
  if (a.size() < 2) {
    c.error(a[0] + " needs a cvar", "write `" + a[0] + " <name> [<by>]`");
    return;
  }
  double x = 0, by = 1;
  parse_number(value_of(c, a[1]), x);
  if (a.size() > 2 && !parse_number(a[2], by)) {
    c.error(a[0] + ": '" + a[2] + "' is not a number", "write `" + a[0] + " <name> [<by>]`");
    return;
  }
  if (over && by == 0) {
    c.error("div: can't divide by 0", a[1] + " is kept");
    return;
  }
  store(c, a, a[1], number_text(over ? x / by : x * by));
}

}  // namespace

void register_script(Console& con) {
  con.add_command("if", "if <a> [<op> <b>] then <commands> [else <commands>]", if_cmd, kNoExpand);
  con.add_command("if_exists", "if_exists <cvar|alias|trigger|command> <name> <cmd> [<else>]",
                  if_exists);
  con.add_command("set_calc", "set_calc <name> <a> <op> <b>: a calculation into a cvar", set_calc);
  con.add_command("eval", "eval <expression>: prints the expression's value and kind",
                  [](Console& c, const Args& a) {
                    ExprValue v;
                    std::string why;
                    if (!eval_expr(a.rest(0), &c.cvars(), v, why)) {
                      c.error("eval: " + why, "check the expression");
                      return;
                    }
                    c.print("Result: " + expr_describe(v));
                  });
  con.add_command("set_eval", "set_eval <name> <expression>: an expression's value into a cvar",
                  [](Console& c, const Args& a) {
                    if (a.size() < 3) {
                      c.error("set_eval needs a name and an expression",
                              "usage: set_eval <name> <expression>");
                      return;
                    }
                    ExprValue v;
                    std::string why;
                    if (!eval_expr(a.rest(1), &c.cvars(), v, why)) {
                      c.error("set_eval: " + why, a[1] + " is kept; check the expression");
                      return;
                    }
                    store(c, a, a[1], expr_text(v));
                  });
  con.add_command("inc", "inc <name> [<by>]: add to a cvar (1)",
                  [](Console& c, const Args& a) { step(c, a, 1); });
  con.add_command("dec", "dec <name> [<by>]: take from a cvar (1)",
                  [](Console& c, const Args& a) { step(c, a, -1); });
  con.add_command("mul", "mul <name> [<by>]: a cvar times a number",
                  [](Console& c, const Args& a) { scale(c, a, false); });
  con.add_command("div", "div <name> [<by>]: a cvar over a number",
                  [](Console& c, const Args& a) { scale(c, a, true); });
  con.add_command("toggle", "toggle <name>: a cvar between 0 and 1", [](Console& c, const Args& a) {
    if (a.size() < 2) {
      c.error("toggle needs a cvar", "usage: toggle <name>");
      return;
    }
    store(c, a, a[1], truthy(value_of(c, a[1])) ? "0" : "1");
  });
  con.add_command("cycle", "cycle <name> <v1> <v2> ...: a cvar to the value after its own",
                  [](Console& c, const Args& a) {
                    if (a.size() < 3) {
                      c.error("cycle needs a cvar and values", "usage: cycle <name> <v1> <v2>");
                      return;
                    }
                    const std::string now = value_of(c, a[1]);
                    size_t next = 2;
                    for (size_t i = 2; i < a.size(); ++i) {
                      if (compare(now, "==", a[i]) == 1) {
                        next = i + 1 < a.size() ? i + 1 : 2;
                        break;
                      }
                    }
                    store(c, a, a[1], a[next]);
                  });
  con.add_command(
      "msg_trigger", "msg_trigger <alias> <match>: a game line holding match runs the alias",
      [](Console& c, const Args& a) {
        auto& t = c.triggers();
        if (a.size() < 3) {
          for (const auto& tr : t)
            if (a.size() < 2 || tr.alias == lower(a[1]))
              c.print(tr.alias + " : \"" + tr.match + "\"");
          return;
        }
        for (auto& tr : t) {
          if (tr.alias == lower(a[1])) {
            tr.match = a[2];
            return;
          }
        }
        t.push_back({lower(a[1]), a[2]});
      },
      kNoExpand);
  con.add_command("msg_trigger_remove", "msg_trigger_remove <alias>: stop a trigger",
                  [](Console& c, const Args& a) {
                    auto& t = c.triggers();
                    const auto before = t.size();
                    std::erase_if(
                        t, [&](const Console::Trigger& tr) { return tr.alias == lower(a[1]); });
                    if (t.size() == before)
                      c.error("no trigger '" + lower(a[1]) + "'", "type msg_trigger to list them");
                  });
}

}  // namespace ge::console
