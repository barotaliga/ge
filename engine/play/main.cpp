// The `play` command line (main.hpp).
//
// ge:summary play_main: parses the play command line and prints each command's JSON answer
// ge:keywords play main command-line describe sim replay state diverge json
// ge:skill agent-interface
// ge:test tests/play_session_test.cpp
#include "play/main.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "play/session.hpp"

namespace ge::play {

namespace {

namespace fs = std::filesystem;

struct Line {
  std::string op;
  std::vector<std::string> words;
  std::map<std::string, std::string> flags;
};

int problem(std::ostream& out, int code, std::string_view what, std::string_view where,
            std::string_view next) {
  out << "{\"error\":" << quote(what) << ",\"where\":" << quote(where)
      << ",\"next\":" << quote(next) << "}\n";
  return code;
}

bool read_log(const std::string& path, InputLog& log, std::string& error) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    error = "cannot read " + path;
    return false;
  }
  std::stringstream ss;
  ss << f.rdbuf();
  auto parsed = parse_inputs(ss.str(), error);
  if (!parsed) {
    error = path + ": " + error;
    return false;
  }
  log = std::move(*parsed);
  return true;
}

std::string describe(const GameSpec& spec) {
  using core::num::to_string;
  std::string axes;
  for (const Axis& a : spec.axes) {
    const char* kind = a.kind == AxisKind::signed_unit ? "signed"
                       : a.kind == AxisKind::lever     ? "lever"
                       : a.kind == AxisKind::toggle    ? "toggle"
                                                       : "press";
    axes += std::string(axes.empty() ? "" : ",") + "{\"name\":" + quote(a.name) +
            ",\"kind\":" + quote(kind) + ",\"help\":" + quote(a.help) + "}";
  }
  std::string options;
  for (const Option& o : spec.options) {
    options += std::string(options.empty() ? "" : ",") + "{\"name\":" + quote(o.name) +
               ",\"default\":" + quote(o.fallback) + ",\"help\":" + quote(o.help) + "}";
  }
  return "{\"game\":" + quote(spec.name) + ",\"summary\":" + quote(spec.summary) +
         ",\"hz\":" + to_string(spec.hz) + ",\"most_seconds\":" + to_string(spec.most_seconds) +
         ",\"axes\":[" + axes + "],\"options\":[" + options + "]}";
}

std::string played_json(const Played& p) {
  return "\"steps\":" + std::to_string(p.steps) + ",\"over\":" + (p.over ? "true" : "false") +
         ",\"digest\":" + quote(hex64(p.digest)) + ",\"report\":" + p.report;
}

// The options among the flags: every flag the command does not take itself.
std::map<std::string, std::string> options_of(const Line& l,
                                              std::initializer_list<const char*> own) {
  std::map<std::string, std::string> out;
  for (const auto& [k, v] : l.flags) {
    bool mine = false;
    for (const char* o : own) mine = mine || k == o;
    if (!mine) out[k] = v;
  }
  return out;
}

bool number_flag(const Line& l, const std::string& name, double fallback, double& out) {
  const auto it = l.flags.find(name);
  if (it == l.flags.end()) {
    out = fallback;
    return true;
  }
  char* end = nullptr;
  out = std::strtod(it->second.c_str(), &end);
  return !it->second.empty() && end == it->second.c_str() + it->second.size();
}

int protocol(const GameSpec& spec, const Line& l, std::istream& in, std::ostream& out) {
  double seed = 0;
  if (!number_flag(l, "seed", 1, seed)) {
    return problem(out, 2, "--seed is not a number", "the command line", "pass --seed=38");
  }
  Setup setup;
  std::string error;
  if (!make_setup(spec, seed, options_of(l, {"seed", "inputs", "stdio"}), setup, error)) {
    return problem(out, 2, error, "the command line", "play describe lists the options");
  }
  Session s(spec, setup);
  if (!s.ok()) return problem(out, 1, s.error(), "the setup", "fix the options and run again");
  out << s.hello() << "\n" << std::flush;
  std::string line;
  while (!s.ended() && std::getline(in, line)) {
    if (line.find_first_not_of(" \t\r") == std::string::npos) continue;
    out << s.handle(line) << "\n" << std::flush;
  }
  if (const auto it = l.flags.find("inputs"); it != l.flags.end()) {
    std::ofstream f(it->second, std::ios::binary);
    f << to_text(s.inputs());
    if (!f) return problem(out, 1, "cannot write " + it->second, it->second, "check the path");
  }
  return 0;
}

int sim(const GameSpec& spec, const Line& l, std::ostream& out) {
  std::vector<double> seeds;
  const auto it = l.flags.find("seeds");
  std::stringstream list(it == l.flags.end() ? "1" : it->second);
  std::string word;
  while (std::getline(list, word, ',')) {
    char* end = nullptr;
    const double v = std::strtod(word.c_str(), &end);
    if (word.empty() || end != word.c_str() + word.size()) {
      return problem(out, 2, "--seeds holds `" + word + "`, not a number", "the command line",
                     "pass --seeds=38,7");
    }
    seeds.push_back(v);
  }
  const auto out_it = l.flags.find("out");
  const fs::path dir = out_it == l.flags.end() ? fs::path("runs") : fs::path(out_it->second);
  std::error_code ec;
  fs::create_directories(dir, ec);
  std::string runs;
  for (const double seed : seeds) {
    Setup setup;
    std::string error;
    if (!make_setup(spec, seed, options_of(l, {"seeds", "out"}), setup, error)) {
      return problem(out, 2, error, "the command line", "play describe lists the options");
    }
    const auto p = pilot_run(spec, setup, error);
    if (!p) return problem(out, 1, error, "seed " + core::num::to_string(seed), "fix the game");
    const fs::path file =
        dir / (std::string(spec.name) + "-" + core::num::to_string(seed) + ".inputs");
    std::ofstream f(file, std::ios::binary);
    f << to_text(p->log);
    if (!f) return problem(out, 1, "cannot write " + file.string(), file.string(), "check --out");
    runs += std::string(runs.empty() ? "" : ",") + "{\"seed\":" + core::num::to_string(seed) +
            ",\"inputs\":" + quote(file.generic_string()) + "," + played_json(*p) + "}";
  }
  out << "{\"game\":" << quote(spec.name) << ",\"runs\":[" << runs << "]}\n";
  return 0;
}

int replay_cmd(const GameSpec& spec, const Line& l, std::ostream& out) {
  if (l.words.size() != 1) {
    return problem(out, 2, "replay takes one input log", "the command line",
                   "play replay runs/x.inputs");
  }
  InputLog log;
  std::string error;
  if (!read_log(l.words[0], log, error))
    return problem(out, 1, error, l.words[0], "pass an input log");
  const auto p = replay(spec, log, error);
  if (!p) return problem(out, 1, error, l.words[0], "fix the log's header or the options");
  const bool matches = !log.digest || *log.digest == p->digest;
  out << "{\"inputs\":" << quote(l.words[0])
      << ",\"recorded\":" << (log.digest ? quote(hex64(*log.digest)) : "null")
      << ",\"matches\":" << (matches ? "true" : "false") << "," << played_json(*p) << "}\n";
  return matches ? 0 : 1;
}

int state_cmd(const GameSpec& spec, const Line& l, std::ostream& out) {
  if (l.words.size() != 1) {
    return problem(out, 2, "state takes one input log", "the command line",
                   "play state runs/x.inputs --step=600");
  }
  InputLog log;
  std::string error;
  if (!read_log(l.words[0], log, error))
    return problem(out, 1, error, l.words[0], "pass an input log");
  double step = 0;
  if (!number_flag(l, "step", static_cast<double>(log.steps()), step)) {
    return problem(out, 2, "--step is not a number", "the command line", "pass --step=600");
  }
  const auto st = state_at(spec, log, static_cast<int64_t>(step), error);
  if (!st) return problem(out, 1, error, l.words[0], "pass a --step inside the log");
  out << "{\"inputs\":" << quote(l.words[0]) << ",\"step\":" << static_cast<int64_t>(step)
      << ",\"state\":" << *st << "}\n";
  return 0;
}

int diverge_cmd(const GameSpec& spec, const Line& l, std::ostream& out) {
  if (l.words.size() != 2) {
    return problem(out, 2, "diverge takes two input logs", "the command line",
                   "play diverge runs/a.inputs runs/b.inputs");
  }
  InputLog a, b;
  std::string error;
  if (!read_log(l.words[0], a, error))
    return problem(out, 1, error, l.words[0], "pass an input log");
  if (!read_log(l.words[1], b, error))
    return problem(out, 1, error, l.words[1], "pass an input log");
  const auto d = diverge(spec, a, b, error);
  if (!d) return problem(out, 1, error, "the logs", "fix the logs' headers");
  std::string fields;
  for (const FieldDiff& f : d->fields) {
    fields += std::string(fields.empty() ? "" : ",") + "{\"key\":" + quote(f.key) +
              ",\"a\":" + quote(f.a) + ",\"b\":" + quote(f.b) + "}";
  }
  out << "{\"same\":" << (d->same ? "true" : "false")
      << ",\"setup_differs\":" << (d->setup_differs ? "true" : "false")
      << ",\"input_step\":" << d->input_step << ",\"state_step\":" << d->state_step
      << ",\"fields\":[" << fields << "],\"more_fields\":" << d->more_fields
      << ",\"steps_a\":" << d->steps_a << ",\"steps_b\":" << d->steps_b
      << ",\"digest_a\":" << quote(hex64(d->digest_a))
      << ",\"digest_b\":" << quote(hex64(d->digest_b)) << "}\n";
  return d->same ? 0 : 1;
}

}  // namespace

int play_main(int argc, char** argv, std::istream& in, std::ostream& out) {
  const GameSpec* spec = registered_game();
  if (!spec) {
    return problem(out, 1, "no game is registered in this executable", "the play target",
                   "add a file with GE_GAME(spec) to it (skill: play-test)");
  }
  Line l;
  for (int i = 1; i < argc; i++) {
    const std::string a = argv[i];
    if (a.rfind("--", 0) == 0) {
      const size_t eq = a.find('=');
      l.flags[a.substr(2, eq == std::string::npos ? std::string::npos : eq - 2)] =
          eq == std::string::npos ? "" : a.substr(eq + 1);
    } else if (l.op.empty()) {
      l.op = a;
    } else {
      l.words.push_back(a);
    }
  }
  if (l.op.empty()) return protocol(*spec, l, in, out);
  if (l.op == "describe") {
    out << describe(*spec) << "\n";
    return 0;
  }
  if (l.op == "sim") return sim(*spec, l, out);
  if (l.op == "replay") return replay_cmd(*spec, l, out);
  if (l.op == "state") return state_cmd(*spec, l, out);
  if (l.op == "diverge") return diverge_cmd(*spec, l, out);
  return problem(out, 2, "unknown command `" + l.op + "`", "the command line",
                 "run describe, sim, replay, state or diverge, or no command for the protocol");
}

int play_main(int argc, char** argv) {
  std::ios::sync_with_stdio(false);
  return play_main(argc, argv, std::cin, std::cout);
}

}  // namespace ge::play
