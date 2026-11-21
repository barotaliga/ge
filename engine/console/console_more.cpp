// The console's further built-ins, the ones Quake II's players type: _echo (options for escapes,
// no line end), text (a line run next), complete (a name forwarded to the game), setu and sets
// (a setting made part of the user or server info), resetall, writeconfig (the binds, aliases and
// settings a player picks, to a file of their own) and lasterror. docs/console.md lists
// each with its syntax.
//
// ge:summary The console's further built-ins: _echo, text, complete, setu, sets, resetall,
// ge:summary writeconfig
// ge:keywords console builtins echo-escapes underscore-echo text complete forwarded setu sets
// ge:keywords userinfo
// ge:keywords serverinfo resetall writeconfig lasterror last-error userinfo
// ge:skill console
// ge:test tests/console_more_test.cpp
#include <algorithm>
#include <cstdio>
#include <filesystem>

#include "console/cfg.hpp"
#include "console/console.hpp"
#include "console/lex.hpp"

namespace ge::console {

namespace {

// `_echo -e`'s escapes: \a \b \t \n \v \f \r \\, \xHH, \ooo, and \ before anything else that
// character.
std::string unescape(std::string_view s) {
  std::string out;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] != '\\' || i + 1 >= s.size()) {
      out += s[i];
      continue;
    }
    const char c = s[++i];
    const auto hex = [](char h) {
      if (h >= '0' && h <= '9') return h - '0';
      if (h >= 'a' && h <= 'f') return h - 'a' + 10;
      if (h >= 'A' && h <= 'F') return h - 'A' + 10;
      return -1;
    };
    switch (c) {
      case 'a':
        out += '\a';
        break;
      case 'b':
        out += '\b';
        break;
      case 't':
        out += '\t';
        break;
      case 'n':
        out += '\n';
        break;
      case 'v':
        out += '\v';
        break;
      case 'f':
        out += '\f';
        break;
      case 'r':
        out += '\r';
        break;
      case 'x': {
        int v = 0, n = 0;
        while (n < 2 && i + 1 < s.size() && hex(s[i + 1]) >= 0) v = v * 16 + hex(s[++i]), ++n;
        out += static_cast<char>(v);
        break;
      }
      default:
        if (c >= '0' && c <= '7') {
          int v = c - '0', n = 1;
          while (n < 3 && i + 1 < s.size() && s[i + 1] >= '0' && s[i + 1] <= '7')
            v = v * 8 + (s[++i] - '0'), ++n;
          out += static_cast<char>(v);
        } else {
          out += c;
        }
    }
  }
  return out;
}

// One option of a command's usage block: `-e | --escapes        | enable ...`.
std::string option_line(char letter, std::string_view name, std::string_view help) {
  std::string n(name);
  n.resize(std::max<size_t>(n.size(), 15), ' ');
  return std::string("-") + letter + " | --" + n + " | " + std::string(help);
}

void echo_raw(Console& c, const Args& a) {
  bool escapes = false;
  size_t i = 1;
  for (; i < a.size(); ++i) {
    const std::string& w = a[i];
    if (w.empty() || w[0] != '-' || w == "-") break;
    if (w == "-e" || w == "--escapes") {
      escapes = true;
    } else if (w == "-n" || w == "--no-newline") {
      // A console line always ends; the option is taken so scripts written for it run.
    } else if (w == "-c" || w == "-l") {
      if (i + 1 >= a.size()) {
        c.print("Missing argument to " + w + ".");
        c.print("Try '_echo --help' for more information.");
        return;
      }
      if (w == "-l" &&
          std::string("atdwen").find(lower(a[i + 1]).substr(0, 1)) == std::string::npos) {
        c.print("Bad print level: " + a[i + 1]);
        return;
      }
      ++i;  // a colour or a level: the console's lines have one look
    } else if (w.starts_with("--color=") || w.starts_with("--level=")) {
      if (w.starts_with("--level=") &&
          std::string("atdwen").find(lower(w.substr(8, 1))) == std::string::npos) {
        c.print("Bad print level: " + w.substr(8));
        return;
      }
    } else if (w == "-h" || w == "--help") {
      c.print("Usage: _echo [-hec:l:n] [text]");
      c.print("Print a line of text into the console.");
      c.print("");
      c.print("Available options:");
      c.print(option_line('h', "help", "display this message"));
      c.print(option_line('e', "escapes", "enable interpretation of backslash escapes"));
      c.print(option_line('c', "color=<color>", "print text in this color"));
      c.print(option_line('l', "level=<level>", "print text at this message level"));
      c.print(option_line('n', "no-newline", "do not output the trailing newline"));
      c.print("");
      return;
    } else {
      c.print("Unknown option: " + w + ".");
      c.print("Try '_echo --help' for more information.");
      return;
    }
  }
  // The rest of the line as written, its spacing kept, one pair of quotes round it dropped.
  std::string text = i < a.size() ? a.rest(i - 1) : std::string();
  if (text.size() >= 2 && text.front() == '"' && text.back() == '"')
    text = text.substr(1, text.size() - 2);
  if (escapes) text = unescape(text);
  size_t at = 0;  // an escaped line end splits the print
  for (size_t nl; (nl = text.find('\n', at)) != std::string::npos; at = nl + 1)
    c.print(text.substr(at, nl - at));
  if (at < text.size() || at == 0) c.print(text.substr(at));
}

// setu and sets: the setting made (or kept) part of the user or the server info, the other mark
// taken away, every word after the name its value.
void info_set(Console& c, const Args& a, uint32_t flag) {
  if (a.size() < 3) {
    c.print("Usage: " + a[0] + " <variable> <value>");
    return;
  }
  std::string value = a[2];
  for (size_t i = 3; i < a.size(); ++i) value += " " + a[i];
  const bool known = c.cvars().find(a[1]) != nullptr;
  const SetResult r = c.cvars().set(a[1], value, a.source, true, flag);
  if (!known && r == SetResult::changed) c.note_created(a[1], a.where);
  if (Cvar* v = c.cvars().find(a[1]); v != nullptr && r != SetResult::read_only) {
    v->flags = (v->flags & ~(kUserInfo | kServerInfo)) | flag;
    ++v->revision;  // the info is sent again even when the value is the same
  }
  const std::string why = set_result_text(r, lower(a[1]));
  if (!why.empty()) c.error(why, "");
}

// A value as writeconfig writes it: in quotes.
std::string in_quotes(std::string_view v) { return "\"" + std::string(v) + "\""; }

// A name made safe as a file's: no folder climbed out of, unsafe letters as underscores.
std::string file_name(std::string_view name) {
  std::string out;
  for (char ch : name) {
    const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                    (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.';
    out += ok ? ch : '_';
  }
  while (out.starts_with('.')) out.erase(0, 1);
  return out;
}

void write_config(Console& c, const Args& a) {
  bool aliases = false, binds = false, archived = false, modified = false, any = false;
  std::string name;
  for (size_t i = 1; i < a.size(); ++i) {
    const std::string& w = a[i];
    if (w.size() > 1 && w[0] == '-' && w[1] != '-') {
      for (size_t k = 1; k < w.size(); ++k) {
        switch (w[k]) {
          case 'a':
            aliases = any = true;
            break;
          case 'b':
            binds = any = true;
            break;
          case 'c':
            archived = any = true;
            break;
          case 'm':
            modified = any = true;
            break;
          case 'h':
            c.print("Usage: writeconfig [-abchm] <filename>");
            c.print("Save current configuration into file.");
            c.print("");
            c.print("Available options:");
            c.print(option_line('a', "aliases", "write aliases"));
            c.print(option_line('b', "bindings", "write bindings"));
            c.print(option_line('c', "cvars", "write archived cvars"));
            c.print(option_line('h', "help", "display this help message"));
            c.print(option_line('m', "modified", "write modified cvars"));
            c.print("");
            return;
          default:
            c.print("Unknown option: " + w + ".");
            c.print("Try 'writeconfig --help' for more information.");
            return;
        }
      }
    } else if (w == "--aliases") {
      aliases = any = true;
    } else if (w == "--bindings") {
      binds = any = true;
    } else if (w == "--cvars") {
      archived = any = true;
    } else if (w == "--modified") {
      modified = any = true;
    } else if (w == "--help") {
      c.execute("writeconfig -h");
      return;
    } else {
      name = w;
    }
  }
  if (!any) binds = archived = true;  // the binds and the saved settings
  name = file_name(name);
  if (name.empty()) {
    c.print("Missing filename argument.");
    c.print("Try 'writeconfig --help' for more information.");
    return;
  }
  if (!name.ends_with(".cfg")) name += ".cfg";
  std::string out = "// generated by writeconfig\n";
  if (binds) {
    out += "\n// key bindings\n";
    for (const auto& [k, cmd] : c.binds().table(BindMode::game))
      if (!cmd.empty()) out += "bind " + k + " " + in_quotes(cmd) + "\n";
  }
  if (aliases) {
    out += "\n// command aliases\n";
    for (const auto& [n, al] : c.aliases())
      out += "alias " + in_quotes(n) + " " + in_quotes(al.body) + "\n";
  }
  if (archived || modified) {
    out += modified ? "\n//modified cvars\n" : "\n//archived cvars\n";
    for (const Cvar* v : c.cvars().sorted()) {
      if ((v->flags & kReadOnly) != 0) continue;
      const std::string& value = v->has_latched ? v->latched : v->value;
      if (modified) {
        if (value != v->fallback) out += "set " + v->name + " " + in_quotes(value) + "\n";
      } else if ((v->flags & kArchive) != 0) {
        out += "seta " + v->name + " " + in_quotes(value) + "\n";
      }
    }
  }
  if (c.write_file(name, out))
    c.print("Wrote " + name + ".");
  else
    c.print("Error writing " + name);
}

}  // namespace

void register_more_commands(Console& con) {
  con.add_command("_echo",
                  "_echo [-e] [-n] [-c <colour>] [-l <level>] <text>: the text as written, its "
                  "spacing kept; -e reads backslash escapes",
                  echo_raw, kStuffable);
  con.add_command(
      "text", "text <words>: the words run as the next command, before the rest",
      [](Console& c, const Args& a) {
        if (a.size() > 1) c.insert(a.rest(0), a.source, a.where);
      },
      kStuffable);
  con.add_command(
      "complete",
      "complete <name>: a command the game knows, completed with Tab and sent to it when typed",
      [](Console& c, const Args& a) {
        if (a.size() < 2) {
          c.print("Usage: complete <command>");
          return;
        }
        const std::string name = lower(a[1]);
        if (c.cvars().find(name) != nullptr) {
          c.print(name + " is already defined as a cvar");
          return;
        }
        if (c.has_command(name)) return;
        c.add_command(name, name + ": sent to the game", [name](Console& cc, const Args& args) {
          if (!cc.unknown() || !cc.unknown()(args))
            cc.print("Can't \"" + name + "\", not connected");
        });
      });
  con.add_command("setu", "setu <name> <value>: set a cvar and send it in the user info",
                  [](Console& c, const Args& a) { info_set(c, a, kUserInfo); });
  con.add_command("sets", "sets <name> <value>: set a cvar and send it in the server info",
                  [](Console& c, const Args& a) { info_set(c, a, kServerInfo); });
  con.add_command(
      "resetall", "resetall: every setting back to its default", [](Console& c, const Args& a) {
        for (Cvar* v : c.cvars().sorted()) {
          if ((v->flags & kReadOnly) != 0 || v->name == "game") continue;
          const std::string why = set_result_text(c.cvars().reset(*v, a.source), v->name);
          if (!why.empty()) c.print(why);
        }
      });
  con.add_command("writeconfig",
                  "writeconfig [-a] [-b] [-c] [-m] <file>: the binds and saved settings (or the "
                  "aliases, -a; every changed setting, -m) written to a cfg of your own",
                  write_config);
  con.add_command("userinfo", "userinfo: the settings sent to the server in the user info",
                  [](Console& c, const Args&) {
                    c.print("User info settings:");
                    for (const Cvar* v : c.cvars().sorted()) {
                      if ((v->flags & kUserInfo) == 0 || v->value.empty()) continue;
                      std::string key = v->name;
                      key.resize(std::max<size_t>(key.size(), 20), ' ');
                      c.print(key + " " + v->value);
                    }
                  });
  con.add_command("lasterror", "lasterror: the last error printed again",
                  [](Console& c, const Args&) {
                    c.print(c.problems().empty() ? std::string() : c.problems().back().what);
                  });
}

}  // namespace ge::console
