// The console's built-in commands for variables, aliases, files and listing: echo, set, seta,
// setinfo, serverinfo, reset, cvarlist, cmdlist, aliaslist, alias, tempalias, unalias,
// unaliasall, exec, cfg_load, cfg_save, wait, help. Each command's syntax is docs/console.md's.
//
// ge:summary The console's built-ins: set, seta, setinfo, alias, exec, cfg_save, cvarlist, help
// ge:keywords console builtins echo set seta setinfo serverinfo reset cvarlist cmdlist aliaslist
// ge:keywords alias tempalias unalias unaliasall exec cfg_load cfg_save wait help flags_text
// ge:skill console
// ge:test tests/console_console_test.cpp
#include "console/cfg.hpp"
#include "console/console.hpp"
#include "console/lex.hpp"

namespace ge::console {

namespace {

void set_cmd(Console& c, const Args& a, uint32_t extra, std::string_view cmd) {
  if (a.size() < 3) {
    c.error(std::string(cmd) + " needs a name and a value",
            "write `" + std::string(cmd) + " <name> <value>`");
    return;
  }
  // The value is every word after the name, as `set name a b` sets "a b".
  std::string value = a[2];
  for (size_t i = 3; i < a.size(); ++i) value += " " + a[i];
  const bool known = c.cvars().find(a[1]) != nullptr;
  const SetResult r = c.cvars().set(a[1], value, a.source, true, extra);
  if (!known && r == SetResult::changed) c.note_created(a[1], a.where);
  if (r == SetResult::latched) {
    const Cvar* v = c.cvars().find(a[1]);
    c.print(v->name + " will be \"" + v->latched + "\" after a restart (it is latched)");
  }
  const std::string why = set_result_text(r, lower(a[1]));
  if (!why.empty()) c.error(why, "");
}

void info_cmd(Console& c, const Args& a, uint32_t flag, uint32_t extra) {
  if (a.size() == 1) {
    c.print(c.cvars().info_string(flag));
    return;
  }
  if (a.size() < 3) {
    c.error(a[0] + " needs a key and a value", "write `" + a[0] + " <key> <value>`");
    return;
  }
  Cvar* v = c.cvars().find(a[1]);
  if (v == nullptr) {
    c.cvars().set(a[1], a[2], a.source, true, flag | extra);
    return;
  }
  if ((v->flags & flag) == 0) {
    c.error(v->name + " is not part of the " + std::string(flag == kUserInfo ? "user" : "server") +
                " info",
            "use `set " + v->name + "`");
    return;
  }
  const std::string why = set_result_text(c.cvars().set(*v, a[2], a.source), v->name);
  if (!why.empty()) c.error(why, "");
}

void alias_cmd(Console& c, const Args& a, bool temp) {
  if (a.size() == 1) {
    for (const auto& [n, al] : c.aliases()) c.print(n + " : " + al.body);
    return;
  }
  const std::string name = lower(a[1]);
  if (!valid_name(name)) {
    c.error("'" + name + "' is not an alias name", "use 1-63 of a-z 0-9 _ + - . ");
    return;
  }
  if (a.size() == 2) {
    const auto it = c.aliases().find(name);
    if (it == c.aliases().end()) {
      c.error("no alias '" + name + "'", "see `aliaslist`");
    } else {
      c.print(name + " : " + it->second.body);
    }
    return;
  }
  if (c.has_command(name)) {
    c.error("'" + name + "' is a command and cannot be an alias", "pick another name");
    return;
  }
  if (c.cvars().find(name) != nullptr) {
    c.error("'" + name + "' is a cvar and cannot be an alias", "pick another name");
    return;
  }
  c.set_alias(name, a.rest(1).front() == '"' ? a[2] : a.rest(1), temp);
}

}  // namespace

// The flags as letters: A archived, U user info, S server info, V server's, C cheat, L latched,
// R read-only, * created at run time.
std::string flags_text(uint32_t f) {
  std::string s;
  s += (f & kArchive) ? 'A' : '.';
  s += (f & kUserInfo) ? 'U' : '.';
  s += (f & kServerInfo) ? 'S' : '.';
  s += (f & kServer) ? 'V' : '.';
  s += (f & kCheat) ? 'C' : '.';
  s += (f & kLatch) ? 'L' : '.';
  s += (f & kReadOnly) ? 'R' : '.';
  s += (f & kCreated) ? '*' : '.';
  return s;
}

void register_builtin_commands(Console& con) {
  con.add_command(
      "echo", "echo <words>: print them, one blank between each",
      [](Console& c, const Args& a) {
        std::string line;
        for (size_t i = 1; i < a.size(); ++i) line += (i > 1 ? " " : "") + a[i];
        c.print(line);
      },
      kStuffable);
  con.add_command(
      "set", "set <name> <value>: set a cvar, creating it if no game reads it",
      [](Console& c, const Args& a) { set_cmd(c, a, 0, "set"); }, kStuffable);
  con.add_command("seta", "seta <name> <value>: set a cvar and archive it (cfg_save writes it)",
                  [](Console& c, const Args& a) { set_cmd(c, a, kArchive, "seta"); });
  con.add_command("setinfo", "setinfo [<key> <value>]: a key of this player's user info",
                  [](Console& c, const Args& a) { info_cmd(c, a, kUserInfo, kArchive); });
  con.add_command("serverinfo", "serverinfo [<key> <value>]: a key of the server's info",
                  [](Console& c, const Args& a) {
                    if (a.size() > 1 && !c.cvars().is_server()) {
                      c.error("serverinfo is set on the server", "put it in the server's cfg");
                      return;
                    }
                    info_cmd(c, a, kServerInfo, kServer);
                  });
  con.add_command("reset", "reset <name>: a cvar back to its default",
                  [](Console& c, const Args& a) {
                    Cvar* v = c.cvars().find(a[1]);
                    if (v == nullptr) {
                      c.error(set_result_text(SetResult::unknown, lower(a[1])), "");
                      return;
                    }
                    const std::string why = set_result_text(c.cvars().reset(*v, a.source), v->name);
                    if (!why.empty()) c.error(why, "");
                  });
  con.add_command("cvarlist",
                  "cvarlist [<prefix>]: every cvar, its value, flags (AUSVCLR*) and help",
                  [](Console& c, const Args& a) {
                    const std::string p = lower(a[1]);
                    int n = 0;
                    for (const Cvar* v : c.cvars().sorted()) {
                      if (v->name.compare(0, p.size(), p) != 0) continue;
                      c.print(flags_text(v->flags) + " " + v->name + " \"" + v->value + "\"" +
                              (v->help.empty() ? std::string() : "  " + v->help));
                      ++n;
                    }
                    c.print(std::to_string(n) + " cvars");
                  });
  con.add_command("cmdlist", "cmdlist [<prefix>]: every command and what it does",
                  [](Console& c, const Args& a) {
                    const std::string p = lower(a[1]);
                    int n = 0;
                    for (const auto& [name, cmd] : c.commands()) {
                      if (name.compare(0, p.size(), p) != 0) continue;
                      c.print(name + "  " + cmd.help);
                      ++n;
                    }
                    c.print(std::to_string(n) + " commands");
                  });
  con.add_command("aliaslist", "aliaslist: every alias and its body", [](Console& c, const Args&) {
    for (const auto& [n, al] : c.aliases())
      c.print(n + (al.temp ? " (temp)" : "") + " : " + al.body);
  });
  con.add_command(
      "alias", "alias <name> \"<commands>\": a new command made of others ($1..$9, $*)",
      [](Console& c, const Args& a) { alias_cmd(c, a, false); }, kNoExpand);
  con.add_command(
      "tempalias", "tempalias <name> \"<commands>\": an alias cfg_save does not keep",
      [](Console& c, const Args& a) { alias_cmd(c, a, true); }, kNoExpand);
  con.add_command("unalias", "unalias <name>: remove an alias", [](Console& c, const Args& a) {
    if (!c.remove_alias(a[1])) c.error("no alias '" + lower(a[1]) + "'", "see `aliaslist`");
  });
  con.add_command("unaliasall", "unaliasall: remove every alias",
                  [](Console& c, const Args&) { c.clear_aliases(); });
  con.add_command("exec", "exec <file>: run a cfg file's commands", [](Console& c, const Args& a) {
    if (a.size() < 2) {
      c.error("exec needs a file", "write `exec <name>.cfg`");
      return;
    }
    c.exec_file(a[1], a.source, a.where);
  });
  con.add_command("cfg_load", "cfg_load [<file>]: exec a saved config (default config.cfg)",
                  [](Console& c, const Args& a) {
                    c.exec_file(a.size() > 1 ? a[1] : std::string(kConfigFile), a.source, a.where);
                  });
  con.add_command(
      "cfg_save", "cfg_save [<file>]: write every archived cvar, alias and bind (config.cfg)",
      [](Console& c, const Args& a) {
        std::string file = a.size() > 1 ? a[1] : std::string(kConfigFile);
        if (std::filesystem::path(file).extension().empty()) file += ".cfg";
        if (c.write_file(file, c.save_text())) {
          c.print("wrote " + file);
        } else {
          c.error("couldn't write " + file, "check the directory exists and is writable");
        }
      });
  con.add_command(
      "wait", "wait: the rest of the buffer runs on the next frame",
      [](Console& c, const Args&) { c.wait(); }, kStuffable);
  con.add_command("help", "help <name>: what a cvar, command or alias is",
                  [](Console& c, const Args& a) { c.print(describe(c, a[1])); });
}

}  // namespace ge::console
