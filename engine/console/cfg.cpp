// Cfg files, defined: the startup order, cfg_save's text, help's description and the dump.
//
// ge:summary The cfg definitions: start, start_server, save_text, describe, dump_text
// ge:keywords cfg start save_text describe dump_text
// ge:skill console
// ge:test tests/console_cfg_test.cpp
#include "console/cfg.hpp"

#include "console/lex.hpp"

namespace ge::console {

namespace {

std::string saved(std::string_view value) {
  std::string out;
  for (char ch : value) {
    out += ch;
    if (ch == '$') out += '$';
  }
  return "\"" + out + "\"";
}

void exec_if_there(Console& c, std::string_view name) {
  std::string text;
  if (!c.read_file(name, text)) return;
  c.exec_file(name);
  for (int f = 0; f < kMaxFrames && c.pending(); ++f) c.run();
}

std::string range_text(const Cvar& v) {
  if (v.kind == CvarKind::text) return "text";
  if (v.kind == CvarKind::boolean) return "0 or 1";
  const bool lo = v.min > -1e308, hi = v.max < 1e308;
  const std::string kind = v.kind == CvarKind::integer ? "a whole number" : "a number";
  if (lo && hi) return kind + " " + number_text(v.min) + " to " + number_text(v.max);
  if (lo) return kind + " from " + number_text(v.min);
  if (hi) return kind + " up to " + number_text(v.max);
  return kind;
}

}  // namespace

void start(Console& c, const std::vector<std::string>& args) {
  exec_if_there(c, kDefaultFile);
  exec_if_there(c, kConfigFile);
  exec_if_there(c, kAutoexecFile);
  c.append_command_line(args);
  c.run();
}

void start_server(Console& c, const std::vector<std::string>& args) {
  exec_if_there(c, kServerFile);
  c.append_command_line(args);
  c.run();
}

std::string Console::save_text() const {
  std::string out = "// written by cfg_save: every archived cvar, alias and bind\n";
  out += "unbindall\n";
  for (int m = 0; m < kBindModes; ++m) {
    const BindMode mode = static_cast<BindMode>(m);
    const std::string flag = mode == BindMode::game ? "" : "-" + std::string(mode_name(mode)) + " ";
    for (const auto& [k, cmd] : binds_.table(mode))
      out += "bind " + flag + quote(k) + " \"" + cmd + "\"\n";
  }
  for (const auto& [n, a] : aliases_)
    if (!a.temp) out += "alias " + n + " \"" + a.body + "\"\n";
  for (const Cvar* v : cvars_.sorted())
    if ((v->flags & kArchive) != 0 && (v->flags & kUserInfo) == 0)
      out += "seta " + v->name + " " + saved(v->has_latched ? v->latched : v->value) + "\n";
  for (const Cvar* v : cvars_.sorted())
    if ((v->flags & kArchive) != 0 && (v->flags & kUserInfo) != 0)
      out += ((v->flags & kCreated) != 0 ? "setinfo " : "seta ") + v->name + " " + saved(v->value) +
             "\n";
  return out;
}

std::string describe(const Console& c, std::string_view name) {
  const std::string n = lower(name);
  if (const Cvar* v = c.cvars().find(n)) {
    std::string s = v->name + " \"" + v->value + "\" (default \"" + v->fallback + "\", " +
                    range_text(*v) + ", flags " + flags_text(v->flags) + ")";
    if (!v->help.empty()) s += ": " + v->help;
    return s;
  }
  if (const auto it = c.commands().find(n); it != c.commands().end())
    return n + " (command): " + it->second.help;
  if (const auto it = c.aliases().find(n); it != c.aliases().end())
    return n + " (alias): " + it->second.body;
  const std::string near = c.suggest(n);
  return "nothing is called '" + n + "'" + (near.empty() ? "" : "; did you mean '" + near + "'?");
}

std::string dump_text(const Console& c) {
  std::string out =
      "// every setting a player may change, at its default. Copy the lines you want into\n"
      "// autoexec.cfg and make them yours; `help <name>` in the console says the same.\n";
  for (const Cvar* v : c.cvars().sorted()) {
    if ((v->flags & (kServer | kReadOnly | kCreated | kCheat)) != 0) continue;
    out += "\n// " + (v->help.empty() ? std::string("(no help)") : v->help) + " [" +
           range_text(*v) + "]\n";
    out += ((v->flags & kArchive) != 0 ? "seta " : "set ") + v->name + " " + saved(v->value) + "\n";
  }
  return out;
}

}  // namespace ge::console
