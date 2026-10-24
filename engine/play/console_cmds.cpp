// The play executable's cvar and cfg commands, defined: a fresh console a command, the game's
// cvars on it, and the answers as JSON.
//
// ge:summary The cvar and cfg commands' definitions: the console made, the answers as JSON
// ge:keywords play cvar cfg check exec dump json
// ge:skill console agent-interface
// ge:test tests/play_console_test.cpp
#include "play/console_cmds.hpp"

#include <filesystem>
#include <memory>

#include "console/cfg.hpp"
#include "play/session.hpp"

namespace ge::play {

namespace {

namespace fs = std::filesystem;
namespace con = console;

int problem(std::ostream& out, int code, std::string_view what, std::string_view where,
            std::string_view next) {
  out << "{\"error\":" << quote(what) << ",\"where\":" << quote(where)
      << ",\"next\":" << quote(next) << "}\n";
  return code;
}

std::string kind_name(con::CvarKind k) {
  switch (k) {
    case con::CvarKind::text:
      return "text";
    case con::CvarKind::number:
      return "number";
    case con::CvarKind::integer:
      return "integer";
    case con::CvarKind::boolean:
      return "boolean";
  }
  return "number";
}

std::string bound(double v) {
  if (v > 1e308 || v < -1e308) return "null";
  return quote(con::number_text(v));
}

std::string cvar_json(const con::Cvar& v) {
  return "{\"name\":" + quote(v.name) + ",\"value\":" + quote(v.value) +
         ",\"default\":" + quote(v.fallback) + ",\"kind\":" + quote(kind_name(v.kind)) +
         ",\"min\":" + bound(v.min) + ",\"max\":" + bound(v.max) +
         ",\"flags\":" + quote(con::flags_text(v.flags)) + ",\"help\":" + quote(v.help) + "}";
}

std::unique_ptr<con::Console> make_console(const GameSpec& spec, bool server) {
  auto c = std::make_unique<con::Console>(server);
  if (spec.cvars != nullptr) spec.cvars(*c);
  return c;
}

std::string problems_json(const con::Console& c) {
  std::string s;
  for (const con::Problem& p : c.problems()) {
    s += std::string(s.empty() ? "" : ",") + "{\"file\":" + quote(p.where.file) +
         ",\"line\":" + std::to_string(p.where.line) + ",\"severity\":" +
         quote(p.severity == con::Problem::Severity::error ? "error" : "warning") +
         ",\"what\":" + quote(p.what) + ",\"next\":" + quote(p.next) + "}";
  }
  return "[" + s + "]";
}

// Each file exec'd in turn, from its own directory first; false (with the answer written) when a
// file cannot be read at all.
bool exec_files(con::Console& c, const std::vector<std::string>& files, std::ostream& out,
                int& code) {
  for (const std::string& f : files) {
    const fs::path path(f);
    if (!fs::is_regular_file(path)) {
      code = problem(out, 1, "no cfg file at " + f, f, "check the path; ge cfg takes cfg files");
      return false;
    }
    c.set_search_path(
        {path.parent_path().empty() ? fs::path(".") : path.parent_path(), fs::path(".")});
    c.exec_file(path.filename().string());
    for (int frame = 0; frame < con::kMaxFrames && c.pending(); ++frame) c.run();
  }
  // A name set that no code reads: a typo, or a setting of a game this is not.
  for (const con::Cvar* v : c.cvars().sorted()) {
    if ((v->flags & con::kCreated) == 0 || (v->flags & (con::kUserInfo | con::kServerInfo)) != 0)
      continue;
    const std::string near = c.suggest(v->name);
    con::Args at;
    if (const con::Origin* o = c.created_at(v->name)) at.where = *o;
    c.set_current(&at);
    c.error("'" + v->name + "' is set but nothing in this game reads it",
            near.empty() ? "a mod's own setting is fine; else check the name"
                         : "did you mean '" + near + "'?",
            con::Problem::Severity::warning);
    c.set_current(nullptr);
  }
  return true;
}

}  // namespace

int cvar_cmd(const GameSpec& spec, const std::vector<std::string>& words,
             const std::map<std::string, std::string>& flags, std::ostream& out) {
  const bool server = flags.count("server") != 0;
  auto c = make_console(spec, server);
  const std::string op = words.empty() ? "" : words[0];
  std::string list;
  int n = 0;
  if (op == "ls") {
    const std::string prefix = con::lower(words.size() > 1 ? words[1] : "");
    for (const con::Cvar* v : c->cvars().sorted()) {
      if (v->name.compare(0, prefix.size(), prefix) != 0) continue;
      list += std::string(n++ ? "," : "") + cvar_json(*v);
    }
  } else if (op == "get" && words.size() > 1) {
    for (size_t i = 1; i < words.size(); ++i) {
      const con::Cvar* v = c->cvars().find(words[i]);
      if (v == nullptr) {
        const std::string near = c->suggest(words[i]);
        return problem(out, 1, "no cvar '" + words[i] + "' in " + spec.name, "the command line",
                       near.empty() ? "ge cvar ls lists every cvar"
                                    : "did you mean '" + near + "'? (ge cvar ls lists them)");
      }
      list += std::string(n++ ? "," : "") + cvar_json(*v);
    }
  } else {
    return problem(out, 2, "cvar needs `ls [<prefix>]` or `get <name>...`", "the command line",
                   "run `play cvar ls`");
  }
  out << "{\"game\":" << quote(spec.name) << ",\"count\":" << n << ",\"cvars\":[" << list << "]}\n";
  return 0;
}

int cfg_cmd(const GameSpec& spec, const std::vector<std::string>& words,
            const std::map<std::string, std::string>& flags, std::ostream& out) {
  const bool server = flags.count("server") != 0;
  auto c = make_console(spec, server);
  const std::string op = words.empty() ? "" : words[0];
  if (op == "dump") {
    out << "{\"game\":" << quote(spec.name) << ",\"text\":" << quote(con::dump_text(*c)) << "}\n";
    return 0;
  }
  if ((op != "check" && op != "exec") || (words.size() < 2 && flags.count("cmd") == 0)) {
    return problem(out, 2, "cfg needs `check <file>...`, `exec <file>...` or `dump`",
                   "the command line", "run `play cfg check autoexec.cfg`");
  }
  int code = 0;
  if (!exec_files(*c, {words.begin() + 1, words.end()}, out, code)) return code;
  const auto cmd = flags.find("cmd");
  if (cmd != flags.end()) c->execute(cmd->second);
  if (op == "exec") c->cvars().apply_latched();
  int errors = 0;
  for (const con::Problem& p : c->problems())
    if (p.severity == con::Problem::Severity::error) ++errors;
  out << "{\"game\":" << quote(spec.name) << ",\"errors\":" << errors
      << ",\"problems\":" << problems_json(*c);
  if (op == "exec") {
    std::string changed, printed;
    int n = 0;
    for (const con::Cvar* v : c->cvars().sorted())
      if (!v->is_default() || v->has_latched)
        changed += std::string(n++ ? "," : "") + cvar_json(*v);
    n = 0;
    for (const std::string& l : c->output()) printed += std::string(n++ ? "," : "") + quote(l);
    out << ",\"changed\":[" << changed << "],\"printed\":[" << printed << "]";
  }
  out << "}\n";
  return errors > 0 ? 1 : 0;
}

}  // namespace ge::play
