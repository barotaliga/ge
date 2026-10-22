// THE ge COMMAND LINE: the engine's tool for the game repos agents build on it (skill:
// agent-interface). Every message it prints is a finding (what, where, next), and its exit
// code says what happened: 0 done, 1 the repo breaks a rule, 2 a usage error.
//
//   ge new <dir> [--name=<kebab>] [--engine-sha256=<hex>]   scaffold a game repo
//   ge skills sync [--force] [--repo=<dir>]                 the engine's skills, to this engine
//   ge doctor [--repo=<dir>]                                the repo against the engine's rules
//   ge lessons [<skill>] [--scope=<path>] [--repo=<dir>]    the skills' lessons, read back
//   ge version                                              the engine version ge belongs to
//
// In a game repo ge runs through its Makefile (`make agents-check`, `make skills`,
// `make lessons`, `make ge ARGS=...`), which builds the ge of the engine the repo pins.
//
// ge:summary The ge executable: parses the command line and runs new, skills, doctor, lessons
// ge:keywords cli ge main command-line usage new skills sync doctor lessons version help
// ge:skill agent-interface
// ge:test tests/cli_scaffold_test.cpp
#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "cli/doctor.hpp"
#include "cli/embedded.hpp"
#include "cli/scaffold.hpp"
#include "cli/skills.hpp"

namespace {

using namespace ge::cli;

constexpr std::string_view kUsage =
    "usage:\n"
    "  ge new <dir> [--name=<kebab>] [--engine-sha256=<hex>]   scaffold a game repo\n"
    "  ge skills sync [--force] [--repo=<dir>]                 the engine's skills, to this "
    "engine\n"
    "  ge doctor [--repo=<dir>]                                the repo against the engine's "
    "rules\n"
    "  ge lessons [<skill>] [--scope=<path>] [--repo=<dir>]    the skills' lessons, read back\n"
    "  ge version                                              the engine version ge belongs "
    "to\n";

struct Args {
  std::vector<std::string> words;                          // positional, command first
  std::vector<std::pair<std::string, std::string>> flags;  // --name=value (value "" for a switch)
};

Args parse(int argc, char** argv) {
  Args a;
  for (int i = 1; i < argc; i++) {
    const std::string arg = argv[i];
    if (arg.starts_with("--")) {
      const size_t eq = arg.find('=');
      a.flags.emplace_back(arg.substr(2, eq == std::string::npos ? std::string::npos : eq - 2),
                           eq == std::string::npos ? "" : arg.substr(eq + 1));
    } else {
      a.words.push_back(arg);
    }
  }
  return a;
}

int usage_error(std::string_view command, const std::string& what) {
  std::cerr << format_finding(command, {Severity::error, what, "the command line",
                                        "run `ge help` for the commands and their flags"})
            << kUsage;
  return 2;
}

// Unknown flags fail: a check run with a mistyped flag reports a confident wrong answer.
bool only_flags(const Args& a, std::initializer_list<std::string_view> known, std::string& bad) {
  for (const auto& [name, value] : a.flags) {
    if (std::find(known.begin(), known.end(), name) == known.end()) {
      bad = "--" + name;
      return false;
    }
  }
  return true;
}

std::string flag(const Args& a, std::string_view name) {
  for (const auto& [n, v] : a.flags) {
    if (n == name) return v;
  }
  return "";
}

bool has_flag(const Args& a, std::string_view name) {
  for (const auto& [n, v] : a.flags) {
    if (n == name) return true;
  }
  return false;
}

// Prints every finding, then one summary line; returns the exit code.
int report(std::string_view command, const std::vector<Finding>& findings, std::string_view clean) {
  for (const Finding& f : findings) std::cout << format_finding(command, f);
  const int errors = count_of(findings, Severity::error);
  const int warns = count_of(findings, Severity::warn);
  const int notes = count_of(findings, Severity::note);
  if (errors == 0) {
    std::cout << command << ": " << clean;
    if (warns + notes > 0) {
      std::cout << " (" << warns << " warn, " << notes << " note)";
    }
    std::cout << "\n";
    return 0;
  }
  std::cout << command << ": " << errors << " error(s), " << warns << " warn, " << notes
            << " note. Fix each error above from its next line, then rerun.\n";
  return 1;
}

int cmd_new(const Args& a) {
  std::string bad;
  if (!only_flags(a, {"name", "engine-sha256"}, bad)) {
    return usage_error("ge new", "unknown flag " + bad);
  }
  if (a.words.size() != 2) return usage_error("ge new", "ge new takes one directory");
  NewOptions o{a.words[1], flag(a, "name"), flag(a, "engine-sha256")};
  const NewResult r = new_game(o);
  for (const std::string& path : r.written) std::cout << "  wrote " << path << "\n";
  const std::string done = "scaffolded " + a.words[1] + " against engine " +
                           std::string(engine_version()) +
                           ". Next: cd into it, `git init`, then `make test` and `make "
                           "agents-check`; AGENTS.md is the router";
  return report("ge new", r.findings, done);
}

int cmd_skills(const Args& a) {
  std::string bad;
  if (!only_flags(a, {"force", "repo"}, bad)) {
    return usage_error("ge skills", "unknown flag " + bad);
  }
  if (a.words.size() != 2 || a.words[1] != "sync") {
    return usage_error("ge skills", "the only skills command is `ge skills sync`");
  }
  const fs::path repo = has_flag(a, "repo") ? fs::path(flag(a, "repo")) : fs::current_path();
  const SyncResult r = sync_skills(repo, has_flag(a, "force"));
  for (const std::string& path : r.written) std::cout << "  wrote " << path << "\n";
  return report("ge skills sync", r.findings,
                "the engine's skills match engine " + std::string(engine_version()));
}

int cmd_doctor(const Args& a) {
  std::string bad;
  if (!only_flags(a, {"repo"}, bad)) return usage_error("ge doctor", "unknown flag " + bad);
  if (a.words.size() != 1) return usage_error("ge doctor", "ge doctor takes no arguments");
  const fs::path repo = has_flag(a, "repo") ? fs::path(flag(a, "repo")) : fs::current_path();
  return report("ge doctor", doctor(repo), "ok against engine " + std::string(engine_version()));
}

int cmd_lessons(const Args& a) {
  std::string bad;
  if (!only_flags(a, {"scope", "repo"}, bad)) {
    return usage_error("ge lessons", "unknown flag " + bad);
  }
  if (a.words.size() > 2) return usage_error("ge lessons", "ge lessons takes at most one skill");
  const fs::path repo = has_flag(a, "repo") ? fs::path(flag(a, "repo")) : fs::current_path();
  return print_lessons(repo, a.words.size() == 2 ? a.words[1] : "", flag(a, "scope"), std::cout);
}

}  // namespace

int main(int argc, char** argv) {
  const Args a = parse(argc, argv);
  const std::string command = a.words.empty() ? "help" : a.words[0];
  if (command == "help" || has_flag(a, "help")) {
    std::cout << "ge " << engine_version() << ": the engine's command line for game repos.\n"
              << kUsage;
    return 0;
  }
  if (command == "version" || has_flag(a, "version")) {
    std::cout << engine_version() << "\n";
    return 0;
  }
  if (command == "new") return cmd_new(a);
  if (command == "skills") return cmd_skills(a);
  if (command == "doctor") return cmd_doctor(a);
  if (command == "lessons") return cmd_lessons(a);
  return usage_error("ge", "unknown command \"" + command + "\"");
}
