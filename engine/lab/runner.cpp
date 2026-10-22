// The runner's flag parsing, its listing and help, the smoke pass, the repeat check, the digest
// baseline, and the sink that prints the engine's log while a lab runs.
//
// ge:summary The labs command line's parsing, listing, smoke pass, repeat and digest checks
// ge:keywords lab runner cli usage smoke repeat digests baseline list help
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
#include "lab/lab.hpp"
#include "lab/report.hpp"

namespace ge::lab {

namespace {

using Args = std::vector<std::pair<std::string, std::string>>;

// The runner's own flags; every lab takes them.
const LabFlag kRunnerFlags[] = {
    {"out", "previews", "the directory a lab's report goes under (previews/<id>/)"},
    {"tag", "", "write to <out>/<id>@<tag>/ instead, to keep a run for --compare"},
    {"compare", "", "print every metric beside the run tagged <tag>"},
    {"repeat", "1", "run each lab N times in this process; different digests fail the run"},
    {"smoke", "", "run at the lab's smoke flags (what CI runs)", true},
    {"all", "", "run every registered lab", true},
    {"shard", "",
     "with --all, run only slice I of N (I/N, from 1/N): every Nth lab, so CTest runs them in "
     "parallel"},
    {"check-digests", "", "compare each lab's digest with this baseline file"},
    {"write-digests", "", "rewrite this baseline file from this run's digests"},
    {"quiet", "", "print only failures and the summary", true},
    {"debug", "", "let the engine's debug lines through", true},
    {"json", "", "with no lab named, list the labs as one JSON array", true},
};

void print_sink(core::output::Level level, std::string_view message, void*) {
  std::fprintf(stderr, "  [%.*s] %.*s\n", static_cast<int>(core::output::level_name(level).size()),
               core::output::level_name(level).data(), static_cast<int>(message.size()),
               message.data());
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

std::map<std::string, std::string> read_digests(const std::string& path) {
  std::map<std::string, std::string> out;
  std::ifstream f(path);
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream row(line);
    std::string id, hex;
    row >> id >> hex;
    out[id] = hex;
  }
  return out;
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
  core::output::set_debug_enabled(get(runner, "debug") == "1");
  core::output::set_output_sink(print_sink);

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
  if (!check.empty() && baseline.empty()) {
    std::fprintf(stderr, "labs: %s holds no digests; write one with --write-digests=%s.\n",
                 check.c_str(), check.c_str());
    return 1;
  }

  int failed = 0;
  std::string digests = "# lab digests at their smoke flags: labs --all --smoke --write-digests\n";
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
      s->run(lab);
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
    digests += std::string(s->id) + " " + hex64(first) + "\n";
    if (!check.empty()) {
      const auto it = baseline.find(s->id);
      if (it == baseline.end()) {
        std::fprintf(stderr,
                     "FAIL %s: not in %s. A new lab: add its digest with "
                     "`make labs-digests` and commit the file.\n",
                     s->id, check.c_str());
        ok = false;
      } else if (it->second != hex64(first)) {
        std::fprintf(stderr,
                     "FAIL %s: digest %s, the baseline %s says %s.\n"
                     "    If this change meant to move the lab's output, run `make labs-digests` "
                     "and say why in the PR.\n    If not, the code it runs moved: rerun it with "
                     "--tag/--compare against main to see what. Skill: labs, determinism.\n",
                     s->id, hex64(first).c_str(), check.c_str(), it->second.c_str());
        ok = false;
      }
    }
    if (!ok) failed++;
  }
  if (!write.empty()) {
    std::ofstream f(write, std::ios::binary);
    f << digests;
    if (!f) {
      std::fprintf(stderr, "labs: could not write %s.\n", write.c_str());
      return 1;
    }
  }
  core::output::set_output_sink(nullptr);
  if (all || labs.size() > 1 || failed) {
    std::printf("labs: %zu run, %d failed\n", labs.size(), failed);
  }
  return failed ? 1 : 0;
}

}  // namespace ge::lab
