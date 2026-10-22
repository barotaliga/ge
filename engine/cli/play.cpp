// ge play, sim, replay, state, diverge and shot (play.hpp): the play executable run and its JSON
// answer turned into an outcome.
//
// ge:summary The play commands: build, run the play executable, its JSON answer as an outcome
// ge:keywords cli ge play sim replay state diverge shot play_answer play_exe
// ge:skill agent-interface
// ge:test tests/cli_play_test.cpp
#include "cli/play.hpp"

#include <algorithm>

#include "cli/json.hpp"
#include "play/json_read.hpp"

namespace ge::cli {

namespace {

std::string quoted_path(const fs::path& p) { return "\"" + p.string() + "\""; }

// A path the agent typed, against the directory ge was run in.
std::string absolute(const std::string& p) {
  std::error_code ec;
  return fs::absolute(fs::path(p), ec).string();
}

// The last line of `output` that holds a JSON object: the play executable's answer (a
// sanitizer's report or a game's own stderr may come before it).
std::string answer_line(const std::string& output) {
  std::string last;
  for (const auto line : split_lines(output)) {
    if (!line.empty() && line.front() == '{') last = std::string(line);
  }
  return last;
}

Outcome run_exe(const BuildOptions& o, const std::string& command, const std::string& args) {
  Outcome out;
  if (!build_into(o, out)) return out;
  const fs::path log = build_log(o, "ge-play");
  std::error_code ec;
  fs::remove(log, ec);
  const Logged r = run_to_log(quoted_path(play_exe(o)) + args, log);
  return play_answer(o, command, r.code, r.output);
}

}  // namespace

std::string text_of(const play::FlatJson& m, const std::string& key) {
  const auto it = m.find(key);
  if (it == m.end()) return "";
  return it->second.kind == play::JsonValue::Kind::string ? it->second.text
                                                          : play::json_text(it->second);
}

bool read_answer(const BuildOptions& o, int code, const std::string& output, play::FlatJson& m,
                 std::string& line, Outcome& out) {
  line = answer_line(output);
  std::string why;
  if (line.empty() || !play::read_flat(line, m, why, true)) {
    out.findings.push_back(
        {Severity::error,
         "the play executable gave no JSON answer (exit " + std::to_string(code) + ")",
         rel_path(o.repo, build_log(o, "ge-play")),
         "read the log; a game without a play target needs one (skill: play-test)"});
    return false;
  }
  if (m.contains("error")) {
    // The executable names the absolute paths ge handed it; the agent reads them from the repo.
    // The repo is stripped as given and as canonical, in native and generic spelling (on
    // Windows those differ in separators and in short against long directory names).
    std::error_code ec;
    const fs::path canonical = fs::weakly_canonical(o.repo, ec);
    std::vector<std::string> roots;
    for (const fs::path& p : {o.repo, canonical}) {
      roots.push_back((p / "").string());
      roots.push_back((p / "").generic_string());
    }
    // Longest first: on macOS the canonical /private/var/... holds the given /var/... in it.
    std::ranges::sort(
        roots, [](const std::string& a, const std::string& b) { return a.size() > b.size(); });
    auto in_repo = [&](std::string s) {
      for (const std::string& root : roots) {
        for (size_t at = s.find(root); !root.empty() && at != std::string::npos;
             at = s.find(root)) {
          s.erase(at, root.size());
        }
      }
      return s;
    };
    out.findings.push_back({Severity::error, in_repo(text_of(m, "error")),
                            fs::path(in_repo(text_of(m, "where"))).generic_string(),
                            text_of(m, "next")});
    if (code == 2) out.exit = 2;
    return false;
  }
  return true;
}

bool play_command(const BuildOptions& o, const std::string& args, play::FlatJson& m,
                  std::string& line, Outcome& out) {
  if (!build_into(o, out)) return false;
  const fs::path log = build_log(o, "ge-play");
  std::error_code ec;
  fs::remove(log, ec);
  const Logged r = run_to_log(quoted_path(play_exe(o)) + args, log);
  return read_answer(o, r.code, r.output, m, line, out);
}

fs::path play_exe(const BuildOptions& o) {
  fs::path exe = o.repo / "build" / o.preset / "play";
#ifdef _WIN32
  exe += ".exe";
#endif
  return exe;
}

Outcome play_answer(const BuildOptions& o, const std::string& command, int code,
                    const std::string& output) {
  Outcome out;
  play::FlatJson m;
  std::string line;
  if (!read_answer(o, code, output, m, line, out)) return out;
  out.result = line;
  if (command == "sim") {
    std::string text;
    std::string first_log;
    for (int i = 0; m.contains("runs." + std::to_string(i) + ".seed"); i++) {
      const std::string k = "runs." + std::to_string(i) + ".";
      const std::string log = rel_path(o.repo, fs::path(text_of(m, k + "inputs")));
      if (first_log.empty()) first_log = log;
      text += "  seed " + text_of(m, k + "seed") + ": ";
      text += text_of(m, k + "over") == "true" ? "over" : "stopped at the longest run";
      text += " after " + text_of(m, k + "steps") + " steps, digest " + text_of(m, k + "digest");
      text += ", " + log + "\n";
    }
    out.text = text;
    out.summary = "the pilot's runs above; each input log replays to its digest";
    out.next = "ge replay " + first_log;
  } else if (command == "replay") {
    const std::string got = text_of(m, "digest");
    const std::string want = text_of(m, "recorded");
    if (text_of(m, "matches") != "true") {
      out.findings.push_back(
          {Severity::error,
           "the replay's digest " + got + " differs from the recorded " + want +
               ": the run read something that is not in its seed, options or inputs",
           text_of(m, "inputs"),
           "find the first step it parts: ge diverge " + text_of(m, "inputs") +
               " <a fresh log of the same inputs> (skill: play-test)"});
      return out;
    }
    out.summary = "replayed " + text_of(m, "steps") + " steps to digest " + got +
                  (want == "null" ? " (the log recorded none)" : ", the one recorded");
  } else if (command == "state") {
    out.text = line.substr(line.find("\"state\":") + 8);
    out.text.pop_back();
    out.text += "\n";
    out.summary =
        "the state after " + text_of(m, "step") + " steps (every field that decides the next)";
    out.next = "ge diverge <log-a> <log-b> compares two runs field by field";
  } else if (command == "diverge") {
    if (text_of(m, "same") == "true") {
      out.summary = "the same run: every step of both, digest " + text_of(m, "digest_a");
      return out;
    }
    std::string text;
    if (text_of(m, "setup_differs") == "true") text += "  the logs' seeds or options differ\n";
    const std::string input = text_of(m, "input_step");
    if (input != "-1") text += "  inputs first differ at step " + input + " (0-based)\n";
    const std::string state = text_of(m, "state_step");
    if (state != "-1") {
      text += "  states first differ after " + state + " steps:\n";
      for (int i = 0; m.contains("fields." + std::to_string(i) + ".key"); i++) {
        const std::string k = "fields." + std::to_string(i) + ".";
        text += "    " + text_of(m, k + "key") + ": " + text_of(m, k + "a") + " vs " +
                text_of(m, k + "b") + "\n";
      }
      if (text_of(m, "more_fields") != "0")
        text += "    and " + text_of(m, "more_fields") + " more\n";
    }
    if (text_of(m, "steps_a") != text_of(m, "steps_b")) {
      text += "  the logs are " + text_of(m, "steps_a") + " and " + text_of(m, "steps_b") +
              " steps long\n";
    }
    out.text = text;
    out.summary = state == "-1" ? "the runs never part in their common steps"
                                : "the runs part after step " + state;
    if (state != "-1") out.next = "ge state <log> --step=" + state + " for either side";
  } else if (command == "shot") {
    const std::string png = rel_path(o.repo, fs::path(text_of(m, "png")));
    out.written.push_back(png);
    std::string views;
    for (int i = 0; m.contains("views." + std::to_string(i) + ".name"); i++) {
      const std::string k = "views." + std::to_string(i) + ".";
      views += "  " + text_of(m, k + "name") + ": " + text_of(m, k + "help") + "\n";
    }
    out.text = "  " + text_of(m, "caption") + "\n  views:\n" + views;
    out.summary = "the " + text_of(m, "view") + " view after " + text_of(m, "step") +
                  " steps (digest " + text_of(m, "digest") + "): " + png;
    out.next = "open " + png + "; ge state <log> --step=" + text_of(m, "step") +
               " gives the numbers behind it";
  }
  return out;
}

Outcome run_play(const BuildOptions& o, const std::string& seed,
                 const std::vector<std::string>& options, bool& played) {
  played = false;
  Outcome out;
  if (!build_into(o, out)) return out;
  std::string cmd = quoted_path(play_exe(o));
  if (!seed.empty()) cmd += " \"--seed=" + seed + "\"";
  for (const std::string& w : options) cmd += " \"" + w + "\"";
  played = true;
  const int code = run_passthrough(cmd);
  out.exit = code < 0 ? 1 : code;
  return out;
}

Outcome run_sim(const BuildOptions& o, const std::string& seeds,
                const std::vector<std::string>& options) {
  std::string args = " sim --out=" + quoted_path(o.repo / "runs");
  if (!seeds.empty()) args += " \"--seeds=" + seeds + "\"";
  for (const std::string& w : options) args += " \"" + w + "\"";
  return run_exe(o, "sim", args);
}

Outcome run_replay(const BuildOptions& o, const std::string& log) {
  return run_exe(o, "replay", " replay \"" + absolute(log) + "\"");
}

Outcome run_state(const BuildOptions& o, const std::string& log, const std::string& step) {
  std::string args = " state \"" + absolute(log) + "\"";
  if (!step.empty()) args += " \"--step=" + step + "\"";
  return run_exe(o, "state", args);
}

Outcome run_diverge(const BuildOptions& o, const std::string& a, const std::string& b) {
  return run_exe(o, "diverge", " diverge \"" + absolute(a) + "\" \"" + absolute(b) + "\"");
}

Outcome run_shot(const BuildOptions& o, const std::string& log, const std::string& seed,
                 const std::string& step, const std::string& view,
                 const std::vector<std::string>& options) {
  std::string args = " shot --out=" + quoted_path(o.repo / "runs");
  if (!log.empty()) args += " \"" + absolute(log) + "\"";
  if (log.empty() && !seed.empty()) args += " \"--seed=" + seed + "\"";
  if (!step.empty()) args += " \"--step=" + step + "\"";
  if (!view.empty()) args += " \"--view=" + view + "\"";
  for (const std::string& w : options) args += " \"" + w + "\"";
  return run_exe(o, "shot", args);
}

}  // namespace ge::cli
