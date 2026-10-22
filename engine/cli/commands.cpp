// The catalog's rows, the command-line split, and both forms of the help.
//
// ge:summary The ge catalog's rows and the parser and help printers that read them
// ge:keywords cli ge commands catalog rows parse_args check_args help_text help_json flags
// ge:skill agent-interface
// ge:test tests/cli_commands_test.cpp
#include "cli/commands.hpp"

#include <algorithm>

#include "cli/embedded.hpp"
#include "cli/json.hpp"

namespace ge::cli {

namespace {

constexpr FlagSpec kRepo{"repo", "<dir>", "the game repo (default: the current directory)"};
constexpr FlagSpec kEngine{"engine", "<dir>",
                           "the engine's source tree (default: the pinned release's, once "
                           "built, or the tree this ge was built from)"};
constexpr FlagSpec kPreset{"preset", "<debug|release>",
                           "the CMake preset (default debug: sanitizers on)"};
constexpr FlagSpec kDryRun{"dry-run", "", "say what would be written, write nothing"};

constexpr FlagSpec kNewFlags[] = {
    {"name", "<kebab>", "the game's name (default: the directory's)"},
    {"engine-sha256", "<hex>", "the release's -src.tar.gz checksum, pinned with it"},
    {"game", "<demo|racer>",
     "the game it starts as: demo (a body on a field) or racer (a car round a loop of gates)"},
};
constexpr FlagSpec kRepoOnly[] = {kRepo};
constexpr FlagSpec kUpgradeFlags[] = {
    {"engine-sha256", "<hex>", "the new release's -src.tar.gz checksum, pinned with it"}, kRepo};
constexpr FlagSpec kSyncFlags[] = {{"force", "", "rewrite engine skills edited by hand"}, kRepo};
constexpr FlagSpec kLessonsFlags[] = {
    {"scope", "<path>", "only lessons whose scope overlaps this path"}, kRepo};
constexpr FlagSpec kFindFlags[] = {
    kRepo, kEngine, {"limit", "<n>", "the most files to list (default 15)"}};
constexpr FlagSpec kExplainFlags[] = {kRepo, kEngine};
constexpr FlagSpec kAddFlags[] = {kRepo, kDryRun};
constexpr FlagSpec kAddLabFlags[] = {
    {"kind", "<sim|scenario|picture|bench>", "the lab's kind (default sim; skill: labs)"},
    kRepo,
    kDryRun};
constexpr FlagSpec kBuildFlags[] = {kPreset, kEngine, kRepo};
constexpr FlagSpec kTestFlags[] = {
    kPreset, {"filter", "<regex>", "only the tests whose names match"}, kEngine, kRepo};
constexpr FlagSpec kLabFlags[] = {
    kPreset,
    {"smoke", "", "run at the lab's smoke flags"},
    {"tag", "<name>", "keep this run under previews/<id>@<name>/ for a later --compare"},
    {"compare", "<name>", "set every metric beside the run tagged <name>"},
    kEngine,
    kRepo};

constexpr FlagSpec kPrecheckFlags[] = {
    {"preset", "<debug|release>", "only this preset (default: both, as CI runs them)"},
    kEngine,
    kRepo};
constexpr FlagSpec kCiFlags[] = {
    {"log", "<file>", "read this saved job log instead of asking GitHub"}, kRepo};
constexpr FlagSpec kDigestFlags[] = {
    {"write", "", "rewrite the lab's row of tests/labs_digests.txt (every row with no lab)"},
    kPreset,
    kEngine,
    kRepo};
constexpr FlagSpec kGoldenDiffFlags[] = {
    {"base", "<ref>", "the git ref to compare with (default HEAD)"}, kRepo};
constexpr FlagSpec kGoldenWriteFlags[] = {
    {"preset", "<debug|release>",
     "the CMake preset (default release: a golden's full run is the release build's)"},
    kEngine,
    kRepo};

constexpr FlagSpec kPlayFlags[] = {
    {"seed", "<n>", "the run's seed (default 1)"}, kPreset, kEngine, kRepo};
constexpr FlagSpec kSimFlags[] = {
    {"seeds", "<a,b,...>", "the seeds the pilot runs (default 1)"}, kPreset, kEngine, kRepo};
constexpr FlagSpec kFast{
    "preset", "<debug|release>",
    "the CMake preset (default release: a sanitizer's time is not the game's)"};
constexpr FlagSpec kSteps{"steps", "<n>", "only the first this many steps of each run"};
constexpr FlagSpec kBenchFlags[] = {
    {"seeds", "<a,b,...>", "the seeds the pilot plays (default 1)"},
    {"repeats", "<n>", "times each run is replayed and timed (default 5)"},
    kSteps,
    {"tag", "<name>", "keep the answer as runs/bench@<name>.json for a later --compare"},
    {"compare", "<name>", "set every number beside the run tagged <name>; 10% slower warns"},
    kFast,
    kEngine,
    kRepo};
constexpr FlagSpec kProfileFlags[] = {
    {"seeds", "<a,b,...>", "the seeds the pilot plays (default 1)"},
    {"repeats", "<n>", "times each run is replayed and timed (default 5)"},
    kSteps,
    {"render", "", "also run the game's `profile` lab: CPU and GPU ms per render pass"},
    kFast,
    kEngine,
    kRepo};
constexpr FlagSpec kMemFlags[] = {
    {"seeds", "<a,b,...>", "the seeds the pilot plays (default 1)"}, kSteps, kFast, kEngine, kRepo};
constexpr FlagSpec kLeakFlags[] = {
    {"seeds", "<a,b,...>", "the seeds the pilot plays (default 1)"},
    {"valgrind", "", "run under valgrind's memcheck instead of the sanitizers"},
    {"preset", "<debug|release>", "the CMake preset (default debug: its sanitizers find leaks)"},
    kEngine,
    kRepo};
constexpr FlagSpec kStateFlags[] = {
    {"step", "<n>", "after this many steps (default: the log's end)"}, kPreset, kEngine, kRepo};
constexpr FlagSpec kShotFlags[] = {
    {"seed", "<n>", "without a log: the pilot plays this seed first (default 1)"},
    {"step", "<n>", "after this many steps (default: the run's end)"},
    {"view", "<name>", "which picture (default: the game's first; ge shot lists them)"},
    kPreset,
    kEngine,
    kRepo};
constexpr FlagSpec kGraphFlags[] = {
    {"kind", "<auto|line|scatter|hist|box|heat|spans>",
     "the chart (default auto: spans for a spans file, line for the rest)"},
    {"columns", "<a,b>", "only these columns (default: every numeric column but x)"},
    {"x", "<column>", "the x column (default: the file's `# x:` or its first numeric column)"},
    {"out", "<dir>", "where the PNGs and SVGs go (default: graphs/ beside the first file)"},
    {"title", "<text>", "a title before each chart's own"},
    {"log", "", "a log scale for the y axis (line and scatter)"}};

const CommandSpec kCommands[] = {
    {"help",
     "[<command>]",
     0,
     -1,
     "learn",
     "the command catalog, or one command's help",
     {},
     "ge help add lab --json",
     "ge"},
    {"version",
     "",
     0,
     0,
     "learn",
     "the engine release this ge belongs to",
     {},
     "ge version",
     "engine"},
    {"find", "<word>...", 1, -1, "learn",
     "the files, skills and lessons that match any word, ranked: the repo's and the pinned "
     "engine's",
     kFindFlags, "ge find heightfield bilinear", "ge"},
    {"explain", "<file|symbol|word>", 1, 1, "learn",
     "what a file or a function is for: its header comment, its declaration, its skill and test",
     kExplainFlags, "ge explain core/prng.hpp", "ge"},
    {"lessons", "[<skill>]", 0, 1, "learn",
     "the skills' lessons: the inventory, or one skill's in full", kLessonsFlags,
     "ge lessons labs --scope=sim/", "skill-reflection"},
    {"new", "<dir>", 1, 1, "create",
     "a game repo against this engine release: the simulation, a lab, a test, the skills",
     kNewFlags, "ge new snow-racer --game=racer", "ge"},
    {"use", "<plugin>", 1, 1, "create",
     "link one of the engine's plugins into the game and install its skill", kRepoOnly,
     "ge use racing", "engine"},
    {"add system", "<name>", 1, 1, "create",
     "a simulation system (sim/<name>.hpp, .cpp) with a test that holds its bits", kAddFlags,
     "ge add system wind", "sim-system"},
    {"add lab", "<name>", 1, 1, "create", "a lab of one kind, registered and in the smoke pass",
     kAddLabFlags, "ge add lab wind --kind=sim", "labs"},
    {"add test", "<name>", 1, 1, "create", "a test file on the repo's harness", kAddFlags,
     "ge add test wind", "sim-system"},
    {"build", "", 0, 0, "build",
     "configure and build; compiler and CMake errors come back as findings", kBuildFlags,
     "ge build", "ge"},
    {"test", "", 0, 0, "build",
     "build, then run the tests and every lab's smoke pass; each failure is a finding", kTestFlags,
     "ge test --filter=sim", "ge"},
    {"check", "", 0, 0, "build",
     "the whole gate in one call: format, doctor, build, tests and labs", kBuildFlags,
     "ge check --json", "commit"},
    {"precheck", "", 0, 0, "build",
     "what CI runs, here, before the push: ge check, then the pilot's seeds and their replays, "
     "on each preset",
     kPrecheckFlags, "ge precheck --preset=release", "commit"},
    {"ci", "[<pr>]", 0, 1, "build",
     "a pull request's state: whether it conflicts with its base, and each failed job's log "
     "read back as findings (through gh, or a saved log)",
     kCiFlags, "ge ci 12", "commit"},
    {"digest", "[<lab>]", 0, 1, "build",
     "a lab (or every lab) at its smoke flags, twice: each digest beside the committed baseline; "
     "--write moves only that lab's row",
     kDigestFlags, "ge digest wind-sweep --write", "labs"},
    {"golden diff", "[<golden|test>...]", 0, -1, "build",
     "each golden file against git's --base: the rows that changed and, column by column, what "
     "moved in them",
     kGoldenDiffFlags, "ge golden diff sim_game_test --base=origin/main", "sim-system"},
    {"golden write", "<test>", 1, 1, "build",
     "rewrite a test's golden files (the test run with GE_GOLDEN_WRITE=1), hold them, and show "
     "what moved",
     kGoldenWriteFlags, "ge golden write sim_wind_test", "sim-system"},
    {"lab", "[<id>]", 0, 1, "see",
     "list the labs, or run one: its metrics, tables, captioned pictures and digest; its own "
     "flags go after --",
     kLabFlags, "ge lab run --json -- --seeds=1,2", "labs"},
    {"graph", "<file>...", 1, -1, "see",
     "chart series or spans files (a trace, a bench, a profile) to PNG and SVG; several files "
     "overlay; every outlier comes back as a note",
     kGraphFlags, "ge graph runs/before.tsv runs/after.tsv --columns=step_ms", "debug"},
    {"shot", "[<log>]", 0, 1, "see",
     "a still of a run at a step, drawn headless and captioned (seed, options, step, digest): "
     "a log played to --step, or the pilot's run of --seed; the game's options go after --",
     kShotFlags, "ge shot runs/fall_line-38.inputs --step=1200 --view=near", "play-test"},
    {"play", "", 0, 0, "play",
     "play the game over the line protocol on this terminal (observe, act, pilot, state, "
     "inputs, end); the game's options go after --",
     kPlayFlags, "ge play --seed=38 -- --rivals=3", "play-test"},
    {"sim", "", 0, 0, "play",
     "the game's pilot over each seed: each run's report and digest, its input log in runs/",
     kSimFlags, "ge sim --seeds=38,7", "play-test"},
    {"replay", "<log>", 1, 1, "play",
     "an input log played back; a digest it misses is a determinism break", kBuildFlags,
     "ge replay runs/fall_line-38.inputs", "play-test"},
    {"state", "<log>", 1, 1, "play",
     "everything that decides the next step, after a number of a log's steps", kStateFlags,
     "ge state runs/fall_line-38.inputs --step=600", "play-test"},
    {"diverge", "<log-a> <log-b>", 2, 2, "play",
     "where two runs part: the first input that differs, the first step after which the states "
     "differ, and the fields that do",
     kBuildFlags, "ge diverge runs/a.inputs runs/b.inputs", "play-test"},
    {"bench", "", 0, 0, "measure",
     "each step's time against its budget (median, p95, p99, worst), steps a second and the "
     "run's open, over the pilot's runs replayed; --tag and --compare for a before and after",
     kBenchFlags, "ge bench --tag=before -- --rivals=3", "perf"},
    {"profile", "", 0, 0, "measure",
     "where the step's time goes: each timing zone's calls, time and share, the time in no zone, "
     "the slowest steps; a timeline round the slowest; --render adds the frame's passes",
     kProfileFlags, "ge profile --seeds=38 --render", "perf"},
    {"mem", "", 0, 0, "measure",
     "what the run allocates: opening it, each step, the steps after the first second that "
     "still allocate and the zone they do it in; the peak resident set",
     kMemFlags, "ge mem --seeds=38", "perf"},
    {"leaks", "", 0, 0, "measure",
     "the pilot's run under the leak checker (the debug build's sanitizers, or valgrind); each "
     "block never freed a finding at its first frame in the repo",
     kLeakFlags, "ge leaks --seeds=38", "perf"},
    {"doctor", "", 0, 0, "maintain",
     "the repo against the engine's rules: guidance, skills, the pin, flags, sources, messages",
     kRepoOnly, "ge doctor", "ge"},
    {"skills sync", "", 0, 0, "maintain", "the engine's skills, rewritten to this engine release",
     kSyncFlags, "ge skills sync", "engine"},
    {"upgrade", "", 0, 0, "maintain",
     "move the repo onto this ge's engine release: the pin rewritten, the skills synced, the repo "
     "doctored; run it with the new release's ge",
     kUpgradeFlags, "ge upgrade --engine-sha256=<hex>", "engine"},
};

constexpr FlagSpec kGlobal[] = {
    {"json", "",
     "answer as one JSON object (command, ok, exit, findings, written, result, "
     "summary, next)"},
    {"help", "", "this command's help"},
};

const FlagSpec* find_flag(const CommandSpec& c, std::string_view name) {
  for (const FlagSpec& f : c.flags) {
    if (f.name == name) return &f;
  }
  for (const FlagSpec& f : kGlobal) {
    if (f.name == name) return &f;
  }
  return nullptr;
}

std::string usage_line(const CommandSpec& c) {
  std::string line = "ge " + std::string(c.name);
  if (!c.args.empty()) line += " " + std::string(c.args);
  for (const FlagSpec& f : c.flags) {
    line +=
        " [--" + std::string(f.name) + (f.value.empty() ? "" : "=" + std::string(f.value)) + "]";
  }
  return line;
}

std::string flag_json(const FlagSpec& f) {
  return json_obj({{"name", json_str("--" + std::string(f.name))},
                   {"value", f.value.empty() ? "null" : json_str(f.value)},
                   {"help", json_str(f.help)}});
}

std::string command_json(const CommandSpec& c) {
  std::vector<std::string> flags;
  for (const FlagSpec& f : c.flags) flags.push_back(flag_json(f));
  return json_obj({{"name", json_str(c.name)},
                   {"usage", json_str(usage_line(c))},
                   {"args", json_str(c.args)},
                   {"stage", json_str(c.stage)},
                   {"summary", json_str(c.summary)},
                   {"flags", json_arr(flags)},
                   {"example", json_str(c.example)},
                   {"skill", json_str(c.skill)}});
}

}  // namespace

std::span<const CommandSpec> commands() { return kCommands; }

const CommandSpec* find_command(std::string_view name) {
  for (const CommandSpec& c : kCommands) {
    if (c.name == name) return &c;
  }
  return nullptr;
}

Args parse_args(int argc, const char* const* argv) {
  Args a;
  bool rest = false;
  for (int i = 1; i < argc; i++) {
    const std::string arg = argv[i];
    if (rest) {
      a.passthrough.push_back(arg);
    } else if (arg == "--") {
      rest = true;
    } else if (arg.starts_with("--")) {
      const size_t eq = arg.find('=');
      a.flags.emplace_back(arg.substr(2, eq == std::string::npos ? std::string::npos : eq - 2),
                           eq == std::string::npos ? "" : arg.substr(eq + 1));
    } else {
      a.words.push_back(arg);
    }
  }
  // The longest command name the leading words spell: "skills sync" before "skills".
  if (a.words.size() >= 2) a.command = find_command(a.words[0] + " " + a.words[1]);
  if (a.command) {
    a.words.erase(a.words.begin(), a.words.begin() + 2);
  } else if (!a.words.empty()) {
    a.command = find_command(a.words[0]);
    if (a.command) a.words.erase(a.words.begin());
  }
  return a;
}

std::optional<std::string> check_args(const Args& a) {
  if (!a.command) return "no such command";
  const CommandSpec& c = *a.command;
  for (const auto& [name, value] : a.flags) {
    const FlagSpec* f = find_flag(c, name);
    if (!f) {
      std::string why = "unknown flag --" + name;
      why += " for ge ";
      why += c.name;
      return why;
    }
    if (f->value.empty() && !value.empty()) return "--" + name + " is a switch and takes no value";
    if (!f->value.empty() && value.empty()) {
      std::string why = "--" + name + " needs a value: --";
      why += name;
      why += '=';
      why += f->value;
      return why;
    }
  }
  const int n = static_cast<int>(a.words.size());
  if (n < c.min_args || (c.max_args >= 0 && n > c.max_args)) {
    return "ge " + std::string(c.name) + " takes " +
           (c.args.empty() ? std::string("no arguments") : std::string(c.args)) + ", got " +
           std::to_string(n) + " word(s)";
  }
  static constexpr std::string_view kPassing[] = {"lab",   "play",    "sim", "shot",
                                                  "bench", "profile", "mem", "leaks"};
  if (!a.passthrough.empty() &&
      std::find(std::begin(kPassing), std::end(kPassing), c.name) == std::end(kPassing)) {
    return "only ge lab, play, sim, shot, bench, profile, mem and leaks pass words after --";
  }
  return std::nullopt;
}

bool has_flag(const Args& a, std::string_view name) {
  return std::any_of(a.flags.begin(), a.flags.end(),
                     [&](const auto& f) { return f.first == name; });
}

std::string flag_value(const Args& a, std::string_view name, std::string_view fallback) {
  for (const auto& [n, v] : a.flags) {
    if (n == name) return v;
  }
  return std::string(fallback);
}

std::string help_text(const CommandSpec* one) {
  if (one) {
    std::string out = usage_line(*one) + "\n  " + std::string(one->summary) + "\n";
    for (const FlagSpec& f : one->flags) {
      out += "  --" + std::string(f.name) + (f.value.empty() ? "" : "=" + std::string(f.value)) +
             "  " + std::string(f.help) + "\n";
    }
    out += "  --json  the answer as one JSON object\n  example: " + std::string(one->example) +
           "\n  skill: " + std::string(one->skill) + "\n";
    return out;
  }
  std::string out = "ge " + std::string(engine_version()) +
                    ": the engine's command line, the one interface an agent makes a game "
                    "through.\nEvery command takes --json (one JSON object: command, ok, exit, "
                    "findings, written, result, summary, next) and --help.\nExit codes: 0 done, 1 "
                    "the work found a problem, 2 a usage error.\n";
  std::string_view stage;
  for (const CommandSpec& c : kCommands) {
    if (c.stage != stage) {
      stage = c.stage;
      out += "\n" + std::string(stage) + ":\n";
    }
    std::string head =
        "  ge " + std::string(c.name) + (c.args.empty() ? "" : " ") + std::string(c.args);
    if (head.size() < 34) head.resize(34, ' ');
    out += head + "  " + std::string(c.summary) + "\n";
  }
  out += "\n`ge help <command>` for its flags; `ge help --json` for the whole catalog.\n";
  return out;
}

std::string help_json(const CommandSpec* one) {
  if (one) return command_json(*one);
  std::vector<std::string> rows;
  for (const CommandSpec& c : kCommands) rows.push_back(command_json(c));
  std::vector<std::string> global;
  for (const FlagSpec& f : kGlobal) global.push_back(flag_json(f));
  return json_obj({{"version", json_str(engine_version())},
                   {"exit_codes", json_obj({{"0", json_str("done")},
                                            {"1", json_str("the work found a problem")},
                                            {"2", json_str("a usage error")}})},
                   {"global_flags", json_arr(global)},
                   {"commands", json_arr(rows)}});
}

}  // namespace ge::cli
