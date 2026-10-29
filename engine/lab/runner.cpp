// The runner's flag parsing, its listing and help, the smoke pass, the repeat check, the digest
// baseline, and the sink that prints the engine's log while a lab runs (and, with --log, keeps
// every kept line for the lab's log.txt).
//
// ge:summary The labs command line's parsing, listing, smoke pass, repeat and digest checks
// ge:keywords lab runner cli usage smoke repeat digests baseline list help log-dump log-spec
// ge:skill labs
// ge:test tests/lab_runner_test.cpp
#include "lab/runner.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "core/output.hpp"
#include "lab/diff.hpp"
#include "lab/digests.hpp"
#include "lab/lab.hpp"
#include "lab/report.hpp"

namespace ge::lab {

namespace {

using Args = std::vector<std::pair<std::string, std::string>>;

// The runner's own flags; every lab takes them.
const LabFlag kRunnerFlags[] = {
    {"out", "previews", "the directory a lab's report goes under (previews/<id>/)"},
    {"tag", "", "write to <out>/<id>@<tag>/ instead, to keep a run for --compare"},
    {"compare", "", "print every metric beside the run tagged <tag>, and diff every picture"},
    {"diff", "",
     "run nothing: diff the pictures of the last run (or --tag's) against the run tagged <tag>; "
     "exit 1 when a band is under its floor"},
    {"diff-bands", "25,100,400", "the diff's distance bands' edges, m"},
    {"diff-psnr", "40,36,32,28,28",
     "the diff's floor for each band, dB, nearest first and the sky last"},
    {"repeat", "1", "run each lab N times in this process; different digests fail the run"},
    {"smoke", "", "run at the lab's smoke flags (what CI runs)", true},
    {"all", "", "run every registered lab", true},
    {"shard", "",
     "with --all, run only slice I of N (I/N, from 1/N): every Nth lab, so CTest runs them in "
     "parallel"},
    {"check-digests", "",
     "compare each lab's digest with this baseline (a directory of <lab>.txt, or a .txt of rows)"},
    {"write-digests", "",
     "rewrite this baseline from this run's digests (only the labs run, unless --all)"},
    {"quiet", "", "print only failures and the summary", true},
    {"debug", "", "let the engine's debug lines through", true},
    {"log", "",
     "a log spec (\"skier=trace,course=debug,*=warn\"): every line kept goes to "
     "<out>/<id>/log.txt, debug and trace only there (docs/debugging.md)"},
    {"json", "", "with no lab named, list the labs as one JSON array", true},
};

// What the sink keeps while a lab runs under --log: every line as a dump writes it, and how many
// of each channel and level, for the report's note.
struct LogDump {
  bool on = false;
  std::string text;
  std::map<std::string, int> counts;  // "skier debug" -> lines
};

void print_sink(const core::output::LogLine& line, void* user) {
  namespace out = core::output;
  auto* dump = static_cast<LogDump*>(user);
  const bool detail = line.level == out::Level::debug || line.level == out::Level::trace;
  if (dump && dump->on) {
    dump->text += out::format_line(line);
    dump->text.push_back('\n');
    std::string key(out::channel_name(line.channel));
    key += " ";
    key += out::level_name(line.level);
    dump->counts[key]++;
    if (detail) return;  // the firehose goes to the file alone
  }
  const std::string_view level = out::level_name(line.level);
  if (line.channel == out::kEngine) {
    std::fprintf(stderr, "  [%.*s] %.*s\n", static_cast<int>(level.size()), level.data(),
                 static_cast<int>(line.message.size()), line.message.data());
  } else {
    const std::string_view ch = out::channel_name(line.channel);
    std::fprintf(stderr, "  [%.*s %.*s] %.*s\n", static_cast<int>(level.size()), level.data(),
                 static_cast<int>(ch.size()), ch.data(), static_cast<int>(line.message.size()),
                 line.message.data());
  }
}

std::string usage(const LabSpec* spec) {
  std::string out;
  if (spec) {
    out += "labs " + std::string(spec->id) + ": " + spec->question + "\n  kind " +
           std::string(kind_name(spec->kind)) + ", owner " + spec->owner + ", subject " +
           spec->subject + ", skill " + spec->skill + "\n  smoke: " + spec->smoke +
           "\n\nits flags:\n";
    for (const LabFlag& f : spec->flags) {
      out += "  --" + std::string(f.name) + (f.boolean ? "" : "=" + std::string(f.value)) + "  " +
             f.help + "\n";
    }
  } else {
    out +=
        "labs: measure and LOOK at the engine and its games (docs/labs.md, skill: labs)\n"
        "  labs                       list every lab\n"
        "  labs <id> [flags]          run one; `labs <id> --help` for its flags\n"
        "  labs --all --smoke         every lab at its smoke flags\n";
  }
  out += "\nthe runner's flags:\n";
  for (const LabFlag& f : kRunnerFlags) {
    out += "  --" + std::string(f.name) + (f.boolean ? "" : "=" + std::string(f.value)) + "  " +
           f.help + "\n";
  }
  return out;
}

std::string list_text() {
  std::string out = "labs (run one with `make lab LAB=<id>`; docs/labs.md):\n";
  for (const LabSpec* s : registered_labs()) {
    char line[64];
    std::snprintf(line, sizeof line, "  %-14s %-9s ", s->id, kind_name(s->kind).data());
    out += line + std::string(s->owner) + "  [" + s->skill + "]\n      " + s->question + "\n";
  }
  return out;
}

// Split `--name=value` / `--name value` / `--switch` words against the known flags. Returns
// false with `error` set on an unknown flag or a missing value.
bool parse(const std::vector<std::string>& words, const LabSpec* spec, Args& runner, Args& lab,
           std::vector<std::string>& positional, std::string& error) {
  auto known = [&](const std::string& name, const LabFlag*& out, bool& is_runner) {
    for (const LabFlag& f : kRunnerFlags) {
      if (name == f.name) return out = &f, is_runner = true;
    }
    if (spec) {
      for (const LabFlag& f : spec->flags) {
        if (name == f.name) return out = &f, is_runner = false, true;
      }
    }
    return false;
  };
  for (size_t i = 0; i < words.size(); i++) {
    const std::string& w = words[i];
    if (w.rfind("--", 0) != 0) {
      positional.push_back(w);
      continue;
    }
    const size_t eq = w.find('=');
    const std::string name = w.substr(2, eq == std::string::npos ? std::string::npos : eq - 2);
    const LabFlag* flag = nullptr;
    bool is_runner = false;
    if (!known(name, flag, is_runner)) {
      error = "unknown flag --" + name +
              (spec ? " for lab " + std::string(spec->id) : std::string(" (no lab named yet)")) +
              ". `labs " + (spec ? std::string(spec->id) + " " : "") + "--help` lists the flags.";
      return false;
    }
    std::string value;
    if (eq != std::string::npos) {
      value = w.substr(eq + 1);
    } else if (flag->boolean) {
      value = "1";
    } else if (i + 1 < words.size() && words[i + 1].rfind("--", 0) != 0) {
      value = words[++i];
    } else {
      error = "--" + name + " needs a value";
      return false;
    }
    Args& into = is_runner ? runner : lab;
    bool replaced = false;
    for (auto& [k, v] : into) {
      if (k == name) v = value, replaced = true;
    }
    if (!replaced) into.emplace_back(name, value);
  }
  return true;
}

std::string get(const Args& args, const char* name) {
  for (const auto& [k, v] : args) {
    if (k == name) return v;
  }
  for (const LabFlag& f : kRunnerFlags) {
    if (std::string(name) == f.name) return f.value;
  }
  return "";
}

// A lab's flags: its defaults, then `smoke` when asked, then what was typed.
Args lab_args(const LabSpec& spec, bool smoke, const Args& typed, std::string& error) {
  Args args;
  for (const LabFlag& f : spec.flags) args.emplace_back(f.name, f.boolean ? "0" : f.value);
  std::vector<std::string> words;
  if (smoke) {
    std::istringstream in(spec.smoke);
    for (std::string w; in >> w;) words.push_back(w);
  }
  Args runner_ignored, smoke_args;
  std::vector<std::string> pos;
  if (!parse(words, &spec, runner_ignored, smoke_args, pos, error)) {
    error = "lab " + std::string(spec.id) + "'s smoke flags do not parse: " + error;
    return {};
  }
  for (const Args* layer : {static_cast<const Args*>(&smoke_args), &typed}) {
    for (const auto& [k, v] : *layer) {
      for (auto& [ak, av] : args) {
        if (ak == k) av = v;
      }
    }
  }
  return args;
}

}  // namespace

int run_labs(int argc, const char* const* argv) {
  std::vector<std::string> words(argv + 1, argv + argc);
  // The first positional word, if it names a lab, scopes the flags.
  const LabSpec* spec = nullptr;
  for (const std::string& w : words) {
    if (w.rfind("--", 0) == 0) continue;
    spec = find_lab(w);
    if (!spec) {
      std::fprintf(stderr, "labs: no lab named '%s'.\n%s", w.c_str(), list_text().c_str());
      return 2;
    }
    break;
  }
  for (const std::string& w : words) {
    if (w == "--help" || w == "-h") {
      std::printf("%s", usage(spec).c_str());
      return 0;
    }
  }
  Args runner, typed;
  std::vector<std::string> positional;
  std::string error;
  if (!parse(words, spec, runner, typed, positional, error)) {
    std::fprintf(stderr, "labs: %s\n", error.c_str());
    return 2;
  }
  // One lab a run: a second name would be ignored, and a digest written for one lab only.
  if (positional.size() > 1) {
    std::string names;
    for (const std::string& w : positional) names.append(names.empty() ? "" : " ").append(w);
    std::fprintf(stderr,
                 "labs: one lab a run, got %zu words (%s). Run each lab on its own (labs %s "
                 "...), or every lab with --all.\n",
                 positional.size(), names.c_str(), positional[0].c_str());
    return 2;
  }
  const bool all = get(runner, "all") == "1";
  if (!spec && !all) {
    if (get(runner, "json") == "1") {
      std::printf("%s\n", list_json().c_str());
      return 0;
    }
    std::printf("%s", list_text().c_str());
    return 0;
  }
  if (all && !typed.empty()) {
    std::fprintf(stderr,
                 "labs: --all takes only the runner's flags; name one lab to pass its "
                 "own.\n");
    return 2;
  }

  const bool smoke = get(runner, "smoke") == "1";
  const bool quiet = get(runner, "quiet") == "1";
  const int repeat = std::max(1, std::atoi(get(runner, "repeat").c_str()));
  const std::string out = get(runner, "out");
  const std::string tag = get(runner, "tag");
  const std::string compare = get(runner, "compare");
  const std::string check = get(runner, "check-digests");
  const std::string write = get(runner, "write-digests");
  DiffOptions diff_options;
  if (!parse_diff_options(get(runner, "diff-bands"), get(runner, "diff-psnr"), diff_options,
                          error)) {
    std::fprintf(stderr, "labs: %s\n", error.c_str());
    return 2;
  }
  if (const std::string against = get(runner, "diff"); !against.empty()) {
    if (all) {
      std::fprintf(stderr, "labs: --diff takes one lab: labs <id> --diff=%s\n", against.c_str());
      return 2;
    }
    const std::string after = out + "/" + spec->id + (tag.empty() ? "" : "@" + tag);
    const RunDiff run =
        diff_runs(out + "/" + spec->id + "@" + against, after, after + "/diff", diff_options);
    std::printf("%s", format_run_diff(run).c_str());
    return run.pass ? 0 : 1;
  }
  core::output::set_debug_enabled(get(runner, "debug") == "1");
  LogDump dump;
  if (const std::string spec_text = get(runner, "log"); !spec_text.empty()) {
    if (const std::string why = core::output::configure(spec_text); !why.empty()) {
      std::fprintf(stderr, "labs: --log: %s.\n", why.c_str());
      return 2;
    }
    dump.on = true;
  }
  core::output::set_output_sink(print_sink, &dump);

  std::vector<const LabSpec*> labs;
  if (all)
    labs = registered_labs();
  else
    labs.push_back(spec);
  if (const std::string shard = get(runner, "shard"); !shard.empty()) {
    const auto slash = shard.find('/');
    const int index = slash == std::string::npos ? 0 : std::atoi(shard.substr(0, slash).c_str());
    const int count = slash == std::string::npos ? 0 : std::atoi(shard.substr(slash + 1).c_str());
    if (!all || count < 1 || index < 1 || index > count) {
      std::fprintf(stderr,
                   "labs: --shard=%s: want --all --shard=I/N with 1 <= I <= N, e.g. --shard=2/4.\n",
                   shard.c_str());
      return 2;
    }
    std::vector<const LabSpec*> slice;
    for (size_t k = static_cast<size_t>(index - 1); k < labs.size();
         k += static_cast<size_t>(count))
      slice.push_back(labs[k]);
    labs = std::move(slice);
  }
  const auto baseline = check.empty() ? std::map<std::string, std::string>{} : read_digests(check);
  if (!check.empty()) {
    const std::vector<std::string> doubled = doubled_digests(check);
    for (const std::string& id : doubled) {
      std::fprintf(stderr,
                   "labs: %s holds two rows for %s: a merge kept both branches' digests. Run the "
                   "lab and keep the one it makes: ge digest %s --write (labs %s --smoke "
                   "--write-digests=%s).\n",
                   check.c_str(), id.c_str(), id.c_str(), id.c_str(), check.c_str());
    }
    if (!doubled.empty()) return 1;
  }
  if (!check.empty() && baseline.empty()) {
    std::fprintf(stderr, "labs: %s holds no digests; write one with --write-digests=%s.\n",
                 check.c_str(), check.c_str());
    return 1;
  }

  int failed = 0;
  // --write-digests rewrites the rows of the labs it runs and keeps every other row, so one lab's
  // baseline moves without running the others. Over every lab, the rows it keeps are the labs
  // this build does not register (a GPU-less build has no GPU labs), each named.
  const bool every = all && get(runner, "shard").empty();
  std::map<std::string, std::string> digests =
      write.empty() ? std::map<std::string, std::string>{} : read_digests(write);
  if (every) {
    for (const auto& [id, hex] : digests) {
      bool ran = false;
      for (const LabSpec* s : labs) ran = ran || id == s->id;
      if (ran) continue;
      std::fprintf(stderr,
                   "labs: kept %s's digest row: this build registers no such lab (a GPU lab on a "
                   "GPU-less build). A lab deleted from the tree: delete its row from %s.\n",
                   id.c_str(), write.c_str());
    }
  }
  for (const LabSpec* s : labs) {
    const Args args = lab_args(*s, smoke, typed, error);
    if (!error.empty()) {
      std::fprintf(stderr, "labs: %s\n", error.c_str());
      return 2;
    }
    const std::string dir = out + "/" + s->id + (tag.empty() ? "" : "@" + tag);
    uint64_t first = 0;
    bool ok = true;
    for (int r = 0; r < repeat; r++) {
      Lab lab(*s, args);
      dump.text.clear();
      dump.counts.clear();
      s->run(lab);
      if (r == 0 && dump.on) {
        std::string counts;
        for (const auto& [key, n] : dump.counts) {
          counts += (counts.empty() ? "" : ", ") + key + " " + std::to_string(n);
        }
        lab.note("log.txt: every line the log spec kept, \"[step] level channel: message\" (" +
                 (counts.empty() ? std::string("no lines") : counts) + ")");
        lab.attach("log.txt", std::vector<uint8_t>(dump.text.begin(), dump.text.end()),
                   Det::measured);
      }
      if (r == 0) {
        first = lab.digest();
        if (!write_report(lab, dir, error)) {
          lab.fail(error, "next: fix the path, or rerun with --out=DIR.");
          error.clear();
        }
        if (!quiet) std::printf("%s", format_report(lab, dir).c_str());
        if (!compare.empty()) {
          std::string path = out;
          path.append("/").append(s->id).append("@").append(compare).append("/metrics.tsv");
          const auto before = read_metrics(path);
          if (before.empty()) {
            std::fprintf(stderr,
                         "labs: nothing to compare: %s is missing. Run the lab with "
                         "--tag=%s first.\n",
                         path.c_str(), compare.c_str());
          } else {
            std::printf("%s", format_compare(before, lab.metrics(), compare).c_str());
          }
          if (!lab.shots().empty()) {
            std::string kept = out;
            kept.append("/").append(s->id).append("@").append(compare);
            const RunDiff run = diff_runs(kept, dir, dir + "/diff", diff_options);
            std::printf("%s", format_run_diff(run).c_str());
          }
        }
        for (const std::string& f : lab.failures()) {
          if (quiet) std::fprintf(stderr, "FAIL %s\n", f.c_str());
          ok = false;
        }
      } else if (lab.digest() != first) {
        std::fprintf(stderr,
                     "FAIL %s: run %d's digest %s differs from run 1's %s in the same process.\n"
                     "    The lab (or the code it runs) is not deterministic: look for state kept "
                     "between runs or an unseeded draw. Skill: determinism.\n",
                     s->id, r + 1, hex64(lab.digest()).c_str(), hex64(first).c_str());
        ok = false;
      }
    }
    digests[s->id] = hex64(first);
    if (!check.empty()) {
      const auto it = baseline.find(s->id);
      if (it == baseline.end()) {
        std::fprintf(stderr,
                     "FAIL %s: not in %s. A new lab: add its digest with "
                     "`ge digest %s --write` and commit the file.\n",
                     s->id, check.c_str(), s->id);
        ok = false;
      } else if (it->second != hex64(first)) {
        std::fprintf(stderr,
                     "FAIL %s: digest %s, the baseline %s says %s.\n"
                     "    If this change meant to move the lab's output, run `ge digest %s "
                     "--write` and say why in the PR.\n    If not, the code it runs moved: rerun "
                     "it with --tag/--compare against main to see what. Skill: labs, "
                     "determinism.\n",
                     s->id, hex64(first).c_str(), check.c_str(), it->second.c_str(), s->id);
        ok = false;
      }
    }
    if (!ok) failed++;
  }
  if (!write.empty()) {
    if (!write_digests(write, digests)) {
      std::fprintf(stderr, "labs: could not write %s.\n", write.c_str());
      return 1;
    }
  }
  core::output::set_output_sink(nullptr);
  core::output::configure("");
  if (all || labs.size() > 1 || failed) {
    std::printf("labs: %zu run, %d failed\n", labs.size(), failed);
  }
  return failed ? 1 : 0;
}

}  // namespace ge::lab
