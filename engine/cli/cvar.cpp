// ge cvar and ge cfg, defined: the play executable's console commands run and read back.
//
// ge:summary ge cvar and ge cfg's definitions: the asks, the answers as text and findings
// ge:keywords cli cvar cfg run_cvar run_cfg console_answer
// ge:skill console agent-interface
// ge:test tests/cli_cvar_test.cpp
#include "cli/cvar.hpp"

#include <fstream>

#include "cli/play.hpp"

namespace ge::cli {

namespace {

std::string arg(const std::string& w) { return " \"" + w + "\""; }

// One cvar's line: name "value" (default "d") flags  help.
std::string cvar_line(const play::FlatJson& m, const std::string& k) {
  const std::string value = text_of(m, k + "value");
  const std::string fallback = text_of(m, k + "default");
  std::string s = "  " + text_of(m, k + "name") + " \"" + value + "\"";
  if (value != fallback) s += " (default \"" + fallback + "\")";
  s += "  " + text_of(m, k + "flags");
  const std::string lo = text_of(m, k + "min"), hi = text_of(m, k + "max");
  if (lo != "null" && hi != "null" && text_of(m, k + "kind") != "boolean")
    s += " [" + lo + ".." + hi + "]";
  const std::string help = text_of(m, k + "help");
  if (!help.empty()) s += "  " + help;
  return s + "\n";
}

}  // namespace

Outcome console_answer(const BuildOptions& o, const ConsoleAsk& ask, const std::string& command,
                       const play::FlatJson& m, const std::string& line) {
  Outcome out;
  out.result = line;
  if (command == "cvar") {
    std::string text;
    int n = 0;
    for (; m.contains("cvars." + std::to_string(n) + ".name"); n++)
      text += cvar_line(m, "cvars." + std::to_string(n) + ".");
    out.text = text;
    out.summary = std::to_string(n) + " cvars of " + text_of(m, "game") +
                  " (flags: A archived, U user info, S server info, V the server's, C cheat, L "
                  "latched, R read-only, * made by a cfg)";
    out.next = "ge cfg dump --out=my.cfg writes every player setting to start a cfg from";
    return out;
  }
  if (ask.op == "dump") {
    const std::string text = text_of(m, "text");
    if (ask.out.empty()) {
      out.text = text;
    } else {
      const fs::path path = o.repo / ask.out;
      std::ofstream(path, std::ios::binary) << text;
      out.written.push_back(rel_path(o.repo, path));
    }
    out.summary = "every setting a player of " + text_of(m, "game") + " may change, commented";
    out.next = "keep the lines you want in autoexec.cfg; ge cfg check autoexec.cfg";
    return out;
  }
  for (int i = 0; m.contains("problems." + std::to_string(i) + ".what"); i++) {
    const std::string k = "problems." + std::to_string(i) + ".";
    std::string where = text_of(m, k + "file");
    // The executable names a file as it exec'd it (its name); name it as the agent gave it.
    for (const std::string& w : ask.words)
      if (!where.empty() && fs::path(w).filename().string() == where) where = w;
    if (!where.empty() && text_of(m, k + "line") != "0") where += ":" + text_of(m, k + "line");
    const std::string next = text_of(m, k + "next");
    out.findings.push_back(
        {text_of(m, k + "severity") == "error" ? Severity::error : Severity::warn,
         text_of(m, k + "what"), where,
         next.empty() ? "ge cvar ls lists the game's cvars (skill: console)" : next});
  }
  if (ask.op == "exec") {
    std::string text = "  changed:\n";
    int n = 0;
    for (; m.contains("changed." + std::to_string(n) + ".name"); n++)
      text += "  " + cvar_line(m, "changed." + std::to_string(n) + ".");
    text += "  printed:\n";
    for (int p = 0; m.contains("printed." + std::to_string(p)); p++)
      text += "    " + text_of(m, "printed." + std::to_string(p)) + "\n";
    out.text = text;
    out.summary = std::to_string(n) + " cvars off their defaults after the files ran";
  } else {
    out.summary = text_of(m, "errors") == "0" ? "the cfg runs clean in " + text_of(m, "game")
                                              : text_of(m, "errors") + " errors in the cfg";
  }
  out.next = "ge cfg exec <file> shows what it changes";
  return out;
}

Outcome run_cvar(const BuildOptions& o, const ConsoleAsk& ask) {
  std::string args = " cvar" + arg(ask.op);
  for (const std::string& w : ask.words) args += arg(w);
  if (ask.server) args += " --server";
  play::FlatJson m;
  std::string line;
  Outcome out;
  if (!play_command(o, args, m, line, out)) return out;
  return console_answer(o, ask, "cvar", m, line);
}

Outcome run_cfg(const BuildOptions& o, const ConsoleAsk& ask) {
  std::string args = " cfg" + arg(ask.op);
  for (const std::string& w : ask.words) args += arg(fs::absolute(fs::path(w)).string());
  if (ask.server) args += " --server";
  if (!ask.cmd.empty()) args += arg("--cmd=" + ask.cmd);
  play::FlatJson m;
  std::string line;
  Outcome out;
  if (!play_command(o, args, m, line, out)) return out;
  return console_answer(o, ask, "cfg", m, line);
}

}  // namespace ge::cli
