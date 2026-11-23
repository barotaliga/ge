// Cfg files, defined: the startup order, cfg_save's text, help's description and the dump.
//
// ge:summary The cfg definitions: start, start_server, save_text, describe, dump_text
// ge:keywords cfg start save_text describe dump_text
// ge:skill console
// ge:test tests/console_cfg_test.cpp
#include "console/cfg.hpp"

#include "console/lex.hpp"
#include "console/words.hpp"

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
  std::string kind = v.kind == CvarKind::integer ? "a whole number" : "a number";
  if (lo && hi) return kind + " " + number_text(v.min) + " to " + number_text(v.max);
  if (lo) return kind + " from " + number_text(v.min);
  if (hi) return kind + " up to " + number_text(v.max);
  return kind;
}

}  // namespace

void start(Console& c, const std::vector<std::string>& args) {
  exec_if_there(c, kDefaultFile);
  exec_if_there(c, kConfigFile);  // a session's own file when one is set (set_config_file)
  if (c.config_file().empty()) exec_if_there(c, kAutoexecFile);
  c.append_command_line(args);
  c.run();
}

void start_server(Console& c, const std::vector<std::string>& args) {
  exec_if_there(c, kServerFile);
  c.append_command_line(args);
  c.run();
}

std::string Console::save_text() const { return save_text(SaveScope{}); }

namespace {

// The value `v` has in the layers `keep` accepts (SAVING ONE PART), and its flags there; false when
// no accepted layer defines it.
bool value_in(const Cvar& v, const std::function<bool(std::string_view)>& keep,
              const std::string& scope, std::string* value, uint32_t* flags) {
  // A meta's value is not the player's: the value under it is saved.
  const auto live = [&v, scope] {
    // Under the loaded scope's own value, the common one is the setting's (cvar.hpp § SCOPES).
    if (!v.scoped.empty() && v.scoped.count(scope) != 0) return v.common;
    if (v.lock) return v.lock->wanted;  // the player's own value under a lock (cvar.hpp § LOCKS)
    if ((v.flags & kUserInfo) != 0) return v.value;
    return !v.implied_by.empty() ? v.below : v.has_latched ? v.latched : v.value;
  };
  if (!keep) {
    *value = live();
    *flags = v.flags;
    return true;
  }
  const Cvar* def = &v;  // the definition looked at, the live one first
  bool current = true;   // its value is the live one (no `own` crossed)
  bool met_same = false;
  for (size_t i = v.earlier.size();; --i) {
    // A definition that met an earlier one as `same` is that one's setting: its home is below.
    const bool same_over = i > 0 && v.earlier[i - 1].how == Overlap::same;
    if (!same_over && keep(def->layer)) {
      *flags = def->flags;
      *value = current && (!met_same || v.explicit_set) ? live() : def->value;
      return true;
    }
    if (i == 0) return false;
    const CvarEarlier& e = v.earlier[i - 1];
    if (e.how == Overlap::own) current = false;
    if (e.how == Overlap::same) met_same = true;
    def = e.was.get();
  }
}

}  // namespace

std::string Console::save_text(const SaveScope& scope) const {
  std::string out = "// written by cfg_save: every archived cvar, alias and bind\n";
  if (scope.binds) {
    out += "unbindall\n";
    for (int m = 0; m < kBindModes; ++m) {
      const BindMode mode = static_cast<BindMode>(m);
      const std::string flag =
          mode == BindMode::game ? "" : "-" + std::string(mode_name(mode)) + " ";
      for (const auto& [k, cmd] : binds_.table(mode))
        out.append("bind ").append(flag).append(quote(k)).append(" \"").append(cmd).append("\"\n");
    }
    for (const auto& [n, a] : aliases_)
      if (!a.temp) out += "alias " + n + " \"" + a.body + "\"\n";
  }
  std::string info;
  for (const Cvar* v : cvars_.sorted()) {
    std::string value;
    uint32_t flags = 0;
    if (!value_in(*v, scope.layers, cvars_.scope(), &value, &flags) || (flags & kArchive) == 0)
      continue;
    if ((flags & kUserInfo) == 0)
      out += "seta " + v->name + " " + saved(value) + "\n";
    else
      info +=
          ((flags & kCreated) != 0 ? "setinfo " : "seta ") + v->name + " " + saved(value) + "\n";
  }
  // Each game's own values (cvar.hpp § SCOPES), after the common ones they stand over.
  std::string scoped;
  for (const Cvar* v : cvars_.sorted())
    for (const auto& [game, value] : v->scoped)
      if ((v->flags & kArchive) != 0 && (!scope.scopes || scope.scopes(game)))
        scoped += "seta " + game + ":" + v->name + " " + saved(value) + "\n";
  return out + info + scoped;
}

std::string describe(const Console& c, std::string_view name) {
  const std::string n = lower(name);
  if (const Cvar* v = c.cvars().find(n)) {
    std::string s = v->name + " \"" + v->value + "\" (default \"" + v->fallback + "\", " +
                    range_text(*v) + ", flags " + flags_text(v->flags) + ")";
    if (!v->help.empty()) s += ": " + v->help;
    if (!v->implied_by.empty()) {
      const Cvar* m = c.cvars().find(v->implied_by);
      s += "\n  set by " + v->implied_by + " " + (m ? m->value : "") + " (under it: \"" + v->below +
           "\")";
    }
    std::vector<ValueEntry> values;
    if (parse_values(v->values, values))
      for (const ValueEntry& e : values) s += "\n  " + e.pattern + ": " + e.meaning;
    if (const Meta* m = c.cvars().meta(v->name)) {
      s += "\n  a meta-cvar: each level sets the others below unless something set them";
      for (const MetaLevel& l : m->levels) {
        std::string sets;
        for (const MetaSet& e : l.sets)
          sets.append(sets.empty() ? "" : "; ").append(e.name + " " + e.value);
        s += "\n  level " + l.value + ": " +
             (sets.empty() ? "nothing (each member as under it)" : sets);
      }
    }
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
      "// autoexec.cfg and make them yours; \"help <name>\" in the console says the same.\n";
  for (const Cvar* v : c.cvars().sorted()) {
    if ((v->flags & (kServer | kReadOnly | kCreated | kCheat)) != 0) continue;
    out += "\n// " + (v->help.empty() ? std::string("(no help)") : v->help) + " [" +
           range_text(*v) + "]\n";
    std::vector<ValueEntry> values;
    if (parse_values(v->values, values))
      for (const ValueEntry& e : values) out += "//   " + e.pattern + ": " + e.meaning + "\n";
    out += ((v->flags & kArchive) != 0 ? "seta " : "set ") + v->name + " " + saved(v->value) + "\n";
  }
  return out;
}

}  // namespace ge::console
