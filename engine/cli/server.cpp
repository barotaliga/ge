// `ge server`, defined (server.hpp): the repo's dedicated binaries found in its CMake files, a run
// or a replay read from what the binary printed, its settings explained.
//
// ge:summary ge server, defined: the binaries found, run or replayed headless, settings explained
// ge:keywords cli ge server run_server run_server_cfg server_binaries read_server_run
// ge:skill agent-interface net
// ge:test tests/cli_server_test.cpp
#include "cli/server.hpp"

#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>

#include "core/json_write.hpp"

namespace ge::cli {

namespace fs = std::filesystem;

namespace {

// One argument as the shell takes it, quoted.
std::string quoted_arg(const std::string& a) {
  std::string out = "\"";
  for (char c : a) {
#ifndef _WIN32
    if (c == '"' || c == '\\' || c == '$' || c == '`') out += '\\';
#else
    if (c == '"') out += '\\';
#endif
    out += c;
  }
  return out + "\"";
}

std::string command_line(const fs::path& exe, const std::vector<std::string>& argv) {
  std::string cmd = quoted_arg(exe.string());
  for (const std::string& a : argv) cmd += " " + quoted_arg(a);
  return cmd;
}

fs::path exe_path(const BuildOptions& o, const std::string& name) {
  fs::path exe = o.repo / "build" / o.preset / name;
#ifdef _WIN32
  exe += ".exe";
#endif
  return exe;
}

// The last `n` lines of `text`.
std::string last_lines(const std::string& text, int n) {
  size_t at = text.size();
  for (int i = 0; i <= n && at > 0; i++) {
    const size_t nl = text.rfind('\n', at - 1);
    if (nl == std::string::npos) return text;
    at = nl;
    if (i == n) return text.substr(at + 1);
  }
  return text;
}

std::string rel(const fs::path& repo, const fs::path& p) {
  std::error_code ec;
  const fs::path r = fs::relative(p, repo, ec);
  return (ec || r.empty() || r.native().starts_with("..") ? p : r).generic_string();
}

// The binary named or the repo's one; "" with a finding in `out` when that cannot be told.
std::string pick(const BuildOptions& o, const ServerOptions& s, Outcome& out) {
  if (!s.game.empty()) return s.game;
  const std::vector<std::string> names = server_binaries(o.repo);
  if (names.size() == 1) return names[0];
  std::string list;
  for (const std::string& n : names) list.append(list.empty() ? "" : ", ").append(n);
  out.findings.push_back(
      {Severity::error,
       names.empty()
           ? "this repo makes no dedicated server (no ge_add_server in its CMake files)"
           : "this repo makes " + std::to_string(names.size()) + " dedicated servers: " + list,
       "",
       names.empty() ? "add one with ge_add_server (cmake/Server.cmake; skill: net)"
                     : "name one: ge server --game=" + names[0]});
  return "";
}

// Only the server's own target built (its CMake target, from ge_add_server); the whole tree when
// the name is not one the repo makes.
bool build_server(const BuildOptions& o, const std::string& name, Outcome& out) {
  for (const auto& [binary, target] : server_targets(o.repo))
    if (binary == name) return build_targets(o, out, target);
  return build_into(o, out);
}

}  // namespace

std::vector<std::string> server_binaries(const fs::path& repo) {
  std::vector<std::string> out;
  for (const auto& [name, target] : server_targets(repo)) out.push_back(name);
  return out;
}

std::vector<std::pair<std::string, std::string>> server_targets(const fs::path& repo) {
  static const std::regex kServer(R"(ge_add_server\s*\(\s*([\w-]+)[^)]*?OUTPUT_NAME\s+([\w.-]+))");
  std::vector<std::pair<std::string, std::string>> out;
  std::error_code ec;
  fs::recursive_directory_iterator it(repo, fs::directory_options::skip_permission_denied, ec);
  for (; !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
    const std::string name = it->path().filename().string();
    if (it->is_directory(ec)) {
      if (name == "build" || name == "node_modules" || name == "previews" || name == "runs" ||
          (!name.empty() && name[0] == '.'))
        it.disable_recursion_pending();
      continue;
    }
    if (name != "CMakeLists.txt" && !name.ends_with(".cmake")) continue;
    std::ifstream f(it->path());
    std::stringstream text;
    text << f.rdbuf();
    const std::string s = text.str();
    for (std::sregex_iterator m(s.begin(), s.end(), kServer), end; m != end; ++m)
      out.emplace_back((*m)[2], (*m)[1]);
  }
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

std::vector<std::string> server_argv(const ServerOptions& s, const std::string& log) {
  std::vector<std::string> argv;
  if (!s.replay.empty()) {
    argv.push_back("--replay=" + s.replay);
  } else {
    if (s.port.empty())
      argv.push_back("--offline");
    else
      argv.push_back("--port=" + s.port);
    argv.push_back("--for=" + (s.seconds.empty() ? std::string("60") : s.seconds));
    if (!s.map.empty()) argv.push_back("--map=" + s.map);
    if (!s.bots.empty()) argv.push_back("--bots=" + s.bots);
    if (!log.empty()) argv.push_back("--log=" + log);
  }
  if (!s.root.empty()) argv.push_back("--root=" + s.root);
  if (!s.mod.empty()) argv.push_back("--mod=" + s.mod);
  if (!s.cfg.empty()) argv.push_back("--cfg=" + s.cfg);
  argv.insert(argv.end(), s.passthrough.begin(), s.passthrough.end());
  return argv;
}

bool read_server_run(const std::string& output, ServerRun& out) {
  static const std::regex kDone(R"(done: (\d+) frames, .* digest ([0-9a-f]+))");
  static const std::regex kLog(R"(input log: (.+?) \(\d+ bytes\))");
  static const std::regex kAgrees(R"(replay: .* agrees on all (\d+) frames; digest ([0-9a-f]+))");
  static const std::regex kLeaves(R"(replay: .* leaves the log at frame (\d+) of (\d+))");
  std::smatch m;
  bool found = false;
  std::istringstream in(output);
  for (std::string line; std::getline(in, line);) {
    if (std::regex_search(line, m, kDone) || std::regex_search(line, m, kAgrees)) {
      out.frames = m[1];
      out.digest = m[2];
      found = true;
    } else if (std::regex_search(line, m, kLeaves)) {
      out.diverged = m[1];
      out.frames = m[2];
      found = true;
    } else if (std::regex_search(line, m, kLog)) {
      out.log = m[1];
    }
  }
  return found;
}

Outcome run_server_binary(const fs::path& repo, const fs::path& exe, const ServerOptions& s,
                          const fs::path& log_file) {
  Outcome out;
  std::string inputs;
  if (s.replay.empty()) {
    inputs = s.log.empty() ? (repo / "runs" / (exe.stem().string() + ".inputs")).string() : s.log;
    std::error_code ec;
    fs::create_directories(fs::path(inputs).parent_path(), ec);
  }
  std::error_code ec;
  fs::remove(log_file, ec);
  const Logged r = run_to_log(command_line(exe, server_argv(s, inputs)), log_file);
  ServerRun run;
  const bool read = read_server_run(r.output, run);
  const std::string name = exe.stem().string();
  if (!run.diverged.empty()) {
    out.findings.push_back(
        {Severity::error,
         s.replay + " leaves the log at frame " + run.diverged + " of " + run.frames +
             ": the server read something that is not in its settings or its log",
         rel(repo, s.replay),
         "compare the state at that frame with the run that wrote the log (skill: determinism)"});
    out.exit = 2;
  } else if (read && r.code != 0 && r.output.find("no input log to write") != std::string::npos) {
    // A game that keeps no input logs still ran: its digest stands, with nothing to replay.
    out.findings.push_back({Severity::note,
                            name + " keeps no input log, so this run cannot be replayed", "",
                            "its digest above is the run's record"});
    out.exit = 0;
  } else if (r.code != 0 || !read) {
    out.findings.push_back(
        {Severity::error,
         name + " stopped with exit " + std::to_string(r.code) + ": " + last_lines(r.output, 6),
         rel(repo, log_file), "fix what it says; " + name + " --help lists its flags"});
    return out;
  }
  const std::string log = run.log.empty() ? std::string() : rel(repo, run.log);
  out.result = core::json_obj({{"server", core::json_str(name)},
                               {"frames", core::json_str(run.frames)},
                               {"digest", core::json_str(run.digest)},
                               {"inputs", core::json_str(log)},
                               {"replay", core::json_bool(!s.replay.empty())}});
  out.text = r.output;
  if (!run.diverged.empty()) return out;
  if (s.replay.empty()) {
    out.summary = name + " ran " + run.frames + " frames, digest " + run.digest +
                  (log.empty() ? std::string() : ", input log " + log);
    if (!log.empty() && out.findings.empty())
      out.next = "ge server --game=" + name + " --replay=" + log;
  } else {
    out.summary = s.replay + " replays to the bit: " + run.frames + " frames, digest " + run.digest;
  }
  return out;
}

Outcome run_server(const BuildOptions& o, const ServerOptions& s) {
  Outcome out;
  const std::string name = pick(o, s, out);
  if (name.empty()) return out;
  if (!build_server(o, name, out)) return out;
  const fs::path exe = exe_path(o, name);
  if (!fs::exists(exe)) {
    out.findings.push_back({Severity::error, "the build made no " + name, rel(o.repo, exe),
                            "check the name against ge_add_server's OUTPUT_NAME; ge server "
                            "without --game names the repo's"});
    return out;
  }
  return run_server_binary(o.repo, exe, s, build_log(o, "ge-server"));
}

Outcome run_server_cfg(const BuildOptions& o, const ServerOptions& s) {
  Outcome out;
  const std::string name = pick(o, s, out);
  if (name.empty()) return out;
  if (!build_server(o, name, out)) return out;
  const fs::path exe = exe_path(o, name);
  std::vector<std::string> argv = {"--explain" + (s.prefix.empty() ? "" : "=" + s.prefix)};
  if (!s.root.empty()) argv.push_back("--root=" + s.root);
  if (!s.mod.empty()) argv.push_back("--mod=" + s.mod);
  if (!s.cfg.empty()) argv.push_back("--cfg=" + s.cfg);
  argv.insert(argv.end(), s.passthrough.begin(), s.passthrough.end());
  const fs::path log = build_log(o, "ge-server-cfg");
  std::error_code ec;
  fs::remove(log, ec);
  const Logged r = run_to_log(command_line(exe, argv), log);
  if (r.code != 0) {
    out.findings.push_back({Severity::error,
                            name + " refused its settings: " + last_lines(r.output, 4),
                            rel(o.repo, log), "fix the file and line it names"});
    return out;
  }
  out.text = r.output;
  out.result =
      core::json_obj({{"server", core::json_str(name)}, {"text", core::json_str(r.output)}});
  out.summary = "each value above with the file and line that set it, and what it overrode";
  out.next = "ge server --game=" + name;
  return out;
}

}  // namespace ge::cli
