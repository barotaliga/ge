// The console's scripting commands, the part of the language that makes a cfg a program:
//
//   if <a> [<op> <b>] then <commands> [else <commands>]
//       op: == != < <= > >= (numbers when both sides are; else text), isin and !isin (a is a
//       substring of b). With no op, a is true unless it is empty or 0. The condition is expanded
//       when `if` runs; the branch runs (and is expanded) after it, before what followed.
//   set_calc <name> <a> <op> <b>    op: + - * / % and the comparisons (1 or 0)
//   set_calc <name> strlen <s> | substr <s> <from> <count> | strstr <s> <find> | int <x> | <a>
//   inc <name> [<by>]   dec <name> [<by>]   toggle <name>   cycle <name> <v1> <v2> ...
//   msg_trigger [<alias> [<match>]]     a line from the game containing match runs the alias
//   msg_trigger_remove <alias>
//
// ge:summary The console's scripting: if/else, set_calc, inc, dec, toggle, cycle, triggers
// ge:keywords console script if else then set_calc calc arithmetic strlen substr strstr inc dec
// ge:keywords toggle cycle msg_trigger trigger conditional compare isin
// ge:skill console
// ge:test tests/console_script_test.cpp
#include <cmath>

#include "console/console.hpp"
#include "console/lex.hpp"

namespace ge::console {

namespace {

// 1 when the comparison holds, 0 when not, -1 when `op` is not one.
int compare(const std::string& a, const std::string& op, const std::string& b) {
  double x = 0, y = 0;
  const bool nums = parse_number(a, x) && parse_number(b, y);
  if (op == "isin") return b.find(a) != std::string::npos ? 1 : 0;
  if (op == "!isin") return b.find(a) == std::string::npos ? 1 : 0;
  if (op == "==") return (nums ? x == y : a == b) ? 1 : 0;
  if (op == "!=") return (nums ? x != y : a != b) ? 1 : 0;
  if (op == "<") return (nums ? x < y : a < b) ? 1 : 0;
  if (op == "<=") return (nums ? x <= y : a <= b) ? 1 : 0;
  if (op == ">") return (nums ? x > y : a > b) ? 1 : 0;
  if (op == ">=") return (nums ? x >= y : a >= b) ? 1 : 0;
  return -1;
}

bool truthy(const std::string& a) {
  double x = 0;
  if (parse_number(a, x)) return x != 0;
  return !a.empty();
}

void if_cmd(Console& c, const Args& a) {
  size_t then = 0, els = 0;
  for (size_t i = 1; i < a.size(); ++i) {
    if (then == 0 && lower(a[i]) == "then") then = i;
    if (then != 0 && els == 0 && i > then && lower(a[i]) == "else") els = i;
  }
  if (then == 0) {
    c.error("if needs `then`", "write `if <a> <op> <b> then <commands> [else <commands>]`");
    return;
  }
  std::vector<std::string> cond;
  for (size_t i = 1; i < then; ++i) cond.push_back(c.expand_text(a[i]));
  bool yes = false;
  if (cond.size() == 1) {
    yes = truthy(cond[0]);
  } else if (cond.size() == 3) {
    const int r = compare(cond[0], cond[1], cond[2]);
    if (r < 0) {
      c.error("if: '" + cond[1] + "' is not a comparison", "use == != < <= > >= isin or !isin");
      return;
    }
    yes = r == 1;
  } else {
    c.error("if: the condition is " + std::to_string(cond.size()) + " words",
            "write `if <a> then ...` or `if <a> <op> <b> then ...`; quote a value with spaces");
    return;
  }
  auto join = [&](size_t from, size_t to) {
    std::string s;
    for (size_t i = from; i < to; ++i) s += (i > from ? " " : "") + a[i];
    return s;
  };
  const std::string branch =
      yes ? join(then + 1, els == 0 ? a.size() : els) : (els == 0 ? "" : join(els + 1, a.size()));
  if (!branch.empty()) c.insert(branch, a.source, a.where);
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
    c.error("set_calc needs a name and an expression", "write `set_calc <name> <a> <op> <b>`");
    return;
  }
  const std::string fn = lower(a[2]);
  auto num = [&](const std::string& s, double& x) {
    if (parse_number(s, x)) return true;
    c.error("set_calc: '" + s + "' is not a number", "check the cvar expanded to a number");
    return false;
  };
  if (fn == "strlen" && a.size() == 4) return store(c, a, a[1], std::to_string(a[3].size()));
  if (fn == "strstr" && a.size() == 5) {
    const size_t at = a[3].find(a[4]);
    return store(c, a, a[1], at == std::string::npos ? "-1" : std::to_string(at));
  }
  if (fn == "substr" && a.size() == 6) {
    double from = 0, count = 0;
    if (!num(a[4], from) || !num(a[5], count)) return;
    const size_t f = from < 0 ? 0 : static_cast<size_t>(from);
    const size_t n = count < 0 ? 0 : static_cast<size_t>(count);
    return store(c, a, a[1], f >= a[3].size() ? std::string() : a[3].substr(f, n));
  }
  if (fn == "int" && a.size() == 4) {
    double x = 0;
    if (!num(a[3], x)) return;
    return store(c, a, a[1], number_text(std::trunc(x)));
  }
  if (a.size() == 3) return store(c, a, a[1], a[2]);
  if (a.size() != 5) {
    c.error("set_calc: cannot read '" + a.rest(1) + "'",
            "write `<a> <op> <b>`, or strlen, substr, strstr or int");
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
  } else if (op == "/" || op == "%") {
    if (y == 0) {
      c.error("set_calc: division by zero", "check the divisor's cvar; " + a[1] + " is kept");
      return;
    }
    r = op == "/" ? x / y : std::fmod(x, y);
  } else {
    c.error("set_calc: '" + op + "' is not an operator", "use + - * / % or a comparison");
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

}  // namespace

void register_script(Console& con) {
  con.add_command("if", "if <a> [<op> <b>] then <commands> [else <commands>]", if_cmd, kNoExpand);
  con.add_command("set_calc", "set_calc <name> <a> <op> <b>: a calculation into a cvar", set_calc);
  con.add_command("inc", "inc <name> [<by>]: add to a cvar (1)",
                  [](Console& c, const Args& a) { step(c, a, 1); });
  con.add_command("dec", "dec <name> [<by>]: take from a cvar (1)",
                  [](Console& c, const Args& a) { step(c, a, -1); });
  con.add_command("toggle", "toggle <name>: a cvar between 0 and 1", [](Console& c, const Args& a) {
    if (a.size() < 2) {
      c.error("toggle needs a cvar", "write `toggle <name>`");
      return;
    }
    store(c, a, a[1], truthy(value_of(c, a[1])) ? "0" : "1");
  });
  con.add_command("cycle", "cycle <name> <v1> <v2> ...: a cvar to the value after its own",
                  [](Console& c, const Args& a) {
                    if (a.size() < 3) {
                      c.error("cycle needs a cvar and values", "write `cycle <name> <v1> <v2>`");
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
                      c.error("no trigger '" + lower(a[1]) + "'", "see `msg_trigger`");
                  });
}

}  // namespace ge::console
