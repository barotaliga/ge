// `ge digest`, `ge golden diff` and `ge golden write` (golden.hpp).
//
// ge:summary ge digest and ge golden's definitions: the runs, the git reads, the column diff
// ge:keywords cli ge digest golden diff write baseline git-show columns moved double-bits
// ge:skill agent-interface
// ge:test tests/cli_golden_test.cpp
#include "cli/golden.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <regex>
#include <sstream>

#include "cli/json.hpp"

namespace ge::cli {

namespace {

constexpr size_t kMostRows = 8;  // rows shown a file; the count says how many more moved

std::string quoted(const fs::path& p) { return "\"" + p.string() + "\""; }

std::vector<std::string> words_of(std::string_view line) {
  std::vector<std::string> out;
  std::istringstream in{std::string(line)};
  std::string w;
  while (in >> w) out.push_back(w);
  return out;
}

struct Table {
  std::vector<std::string> header;  // the last '#' line's words before the rows, '#' dropped
  std::vector<std::vector<std::string>> rows;
};

Table table_of(std::string_view text) {
  Table t;
  for (std::string_view line : split_lines(text)) {
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    if (!line.empty() && line[0] == '#') {
      if (t.rows.empty()) t.header = words_of(line.substr(1));
      continue;
    }
    auto w = words_of(line);
    if (!w.empty()) t.rows.push_back(std::move(w));
  }
  // Provenance after the names ("(fall_line_sim_test, the bot)") is not a column.
  const auto paren = std::find_if(t.header.begin(), t.header.end(),
                                  [](const std::string& w) { return w.starts_with("("); });
  t.header.erase(paren, t.header.end());
  return t;
}

std::string column_name(const std::vector<std::string>& names, size_t c) {
  return c < names.size() ? names[c] : "column " + std::to_string(c + 1);
}

// The double a 16-hex-digit word's bits are, as text; "" when the word is not one.
std::string as_double(const std::string& word, const std::string& column) {
  if (word.size() != 16 || word.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
    return "";
  }
  for (const char* skip : {"digest", "hash", "fingerprint", "seed"}) {
    if (column.find(skip) != std::string::npos) return "";
  }
  const uint64_t bits = std::strtoull(word.c_str(), nullptr, 16);
  double v;
  std::memcpy(&v, &bits, sizeof v);
  char buf[40];
  std::snprintf(buf, sizeof buf, "%.17g", v);
  return buf;
}

std::string shown(const std::string& word, const std::string& column) {
  const std::string d = as_double(word, column);
  return d.empty() ? word : word + " (" + d + ")";
}

void set_env(const char* name, const char* value) {
#ifdef _WIN32
  _putenv_s(name, value ? value : "");
#else
  if (value)
    setenv(name, value, 1);
  else
    unsetenv(name);
#endif
}

fs::path git_log(const fs::path& repo) { return repo / "build" / "ge-golden.log"; }

// A file's text at a git ref, or nothing when git has none there.
std::optional<std::string> at_ref(const fs::path& repo, const std::string& ref,
                                  const std::string& rel) {
  const Logged r =
      run_to_log("git -C " + quoted(repo) + " show \"" + ref + ":" + rel + "\"", git_log(repo));
  if (r.code != 0) return std::nullopt;
  return r.output;
}

std::vector<std::string> lines_of(const std::string& text) {
  std::vector<std::string> out;
  for (std::string_view l : split_lines(text)) {
    while (!l.empty() && (l.back() == '\r' || l.back() == ' ')) l.remove_suffix(1);
    if (!l.empty()) out.emplace_back(l);
  }
  return out;
}

std::string last_line(const std::string& text) {
  const auto lines = lines_of(text);
  return lines.empty() ? std::string("(no output)") : lines.back();
}

// What a golden argument names: a file, a stem under tests/golden/, or a test's goldens.
std::vector<std::string> resolve_goldens(const fs::path& repo, const std::string& target,
                                         std::vector<Finding>& findings) {
  std::error_code ec;
  if (fs::is_regular_file(repo / target, ec) && target.ends_with(".txt")) return {target};
  const std::string stem = "tests/golden/" + target + ".txt";
  if (fs::is_regular_file(repo / stem, ec)) return {stem};
  std::string test = fs::path(target).stem().string();
  if (!test.ends_with("_test")) test += "_test";
  if (const auto source = read_text(repo / "tests" / (test + ".cpp"))) {
    auto named = goldens_named_in(*source);
    if (named.empty()) {
      findings.push_back({Severity::warn, "tests/" + test + ".cpp names no golden file",
                          "tests/" + test + ".cpp", "name a file under tests/golden/ instead"});
    }
    return named;
  }
  findings.push_back({Severity::error, "no golden file or test \"" + target + "\"", "the argument",
                      "name tests/golden/<x>.txt, its stem <x>, or a test (<topic>_test)"});
  return {};
}

}  // namespace

GoldenDiff diff_golden(std::string_view file, std::string_view before, std::string_view after) {
  GoldenDiff d;
  d.file = std::string(file);
  const Table a = table_of(before), b = table_of(after);
  d.rows_before = a.rows.size();
  d.rows_after = b.rows.size();
  d.header_changed = a.header != b.header;
  // The header names the columns only when it has one word a column; prose names none.
  const Table& named = b.rows.empty() ? a : b;
  std::vector<std::string> names = named.header;
  if (named.rows.empty() || names.size() != named.rows[0].size()) names.clear();
  std::map<size_t, int> moved;  // column → rows it moved in
  const size_t rows = std::max(a.rows.size(), b.rows.size());
  for (size_t r = 0; r < rows; r++) {
    static const std::vector<std::string> kNone;
    const auto& wa = r < a.rows.size() ? a.rows[r] : kNone;
    const auto& wb = r < b.rows.size() ? b.rows[r] : kNone;
    if (wa == wb) continue;
    GoldenRow row;
    row.row = r + 1;
    row.key = !wa.empty() ? wa[0] : wb[0];
    for (size_t c = 0; c < std::max(wa.size(), wb.size()); c++) {
      const std::string x = c < wa.size() ? wa[c] : "-";
      const std::string y = c < wb.size() ? wb[c] : "-";
      if (x == y) continue;
      row.cells.push_back({column_name(names, c), x, y});
      moved[c]++;
    }
    d.rows.push_back(std::move(row));
  }
  for (const auto& [c, n] : moved) d.moved.emplace_back(column_name(names, c), n);
  return d;
}

std::string format_golden_diff(const GoldenDiff& d, size_t most_rows) {
  std::string out = d.file + ": " + std::to_string(d.rows.size()) + " of " +
                    std::to_string(std::max(d.rows_before, d.rows_after)) + " rows changed";
  if (d.rows_before != d.rows_after) {
    out += " (" + std::to_string(d.rows_before) + " rows before, " + std::to_string(d.rows_after) +
           " after)";
  }
  if (d.header_changed) out += "; the header changed";
  out += "\n";
  for (const auto& [name, n] : d.moved) {
    out += "  " + name + " moved in " + std::to_string(n) + (n == 1 ? " row\n" : " rows\n");
  }
  for (size_t i = 0; i < std::min(most_rows, d.rows.size()); i++) {
    const GoldenRow& r = d.rows[i];
    out += "  row " + std::to_string(r.row) + " (" + r.key + "):";
    for (size_t k = 0; k < r.cells.size(); k++) {
      const GoldenCell& c = r.cells[k];
      out += std::string(k ? ";" : "") + " " + c.column + " " + shown(c.before, c.column) + " -> " +
             shown(c.after, c.column);
    }
    out += "\n";
  }
  if (d.rows.size() > most_rows) {
    out += "  ... and " + std::to_string(d.rows.size() - most_rows) + " more rows\n";
  }
  return out;
}

std::string golden_diff_json(const GoldenDiff& d, size_t most_rows) {
  std::vector<std::string> moved, rows;
  for (const auto& [name, n] : d.moved) {
    moved.push_back(json_obj({{"column", json_str(name)}, {"rows", json_num(n)}}));
  }
  for (size_t i = 0; i < std::min(most_rows, d.rows.size()); i++) {
    const GoldenRow& r = d.rows[i];
    std::vector<std::string> cells;
    for (const GoldenCell& c : r.cells) {
      std::vector<std::pair<std::string, std::string>> m = {{"column", json_str(c.column)},
                                                            {"before", json_str(c.before)},
                                                            {"after", json_str(c.after)}};
      const std::string x = as_double(c.before, c.column), y = as_double(c.after, c.column);
      if (!x.empty() && !y.empty()) {
        m.emplace_back("before_value", json_str(x));
        m.emplace_back("after_value", json_str(y));
      }
      cells.push_back(json_obj(m));
    }
    rows.push_back(json_obj({{"row", json_num(static_cast<long long>(r.row))},
                             {"key", json_str(r.key)},
                             {"cells", json_arr(cells)}}));
  }
  return json_obj({{"file", json_str(d.file)},
                   {"rows_changed", json_num(static_cast<long long>(d.rows.size()))},
                   {"rows_before", json_num(static_cast<long long>(d.rows_before))},
                   {"rows_after", json_num(static_cast<long long>(d.rows_after))},
                   {"header_changed", json_bool(d.header_changed)},
                   {"moved", json_arr(moved)},
                   {"rows", json_arr(rows)}});
}

std::vector<std::string> goldens_named_in(std::string_view source) {
  static const std::regex kGolden(R"(tests/golden/[A-Za-z0-9_]+\.txt)");
  std::vector<std::string> out;
  const std::string text(source);
  for (auto it = std::sregex_iterator(text.begin(), text.end(), kGolden);
       it != std::sregex_iterator(); ++it) {
    const std::string f = it->str();
    if (std::find(out.begin(), out.end(), f) == out.end()) out.push_back(f);
  }
  return out;
}

Outcome run_golden_diff(const fs::path& repo, const std::vector<std::string>& targets,
                        const std::string& base) {
  Outcome out;
  std::vector<std::string> files;
  // A repo with no commit yet has nothing at HEAD: every golden file is new.
  const bool unborn =
      base == "HEAD" &&
      run_to_log("git -C " + quoted(repo) + " rev-parse --verify --quiet HEAD", git_log(repo))
              .code != 0 &&
      run_to_log("git -C " + quoted(repo) + " rev-parse --git-dir", git_log(repo)).code == 0;
  if (targets.empty()) {
    const Logged changed =
        unborn ? run_to_log("git -C " + quoted(repo) + " ls-files -- tests/golden", git_log(repo))
               : run_to_log(
                     "git -C " + quoted(repo) + " diff --name-only " + base + " -- tests/golden",
                     git_log(repo));
    if (changed.code != 0) {
      out.findings.push_back(
          {Severity::error,
           "git could not compare tests/golden with " + base + ": " + last_line(changed.output),
           rel_path(repo, git_log(repo)),
           "run it in a git checkout, with a --base=<ref> git knows"});
      return out;
    }
    files = lines_of(changed.output);
    for (const std::string& f :
         lines_of(run_to_log("git -C " + quoted(repo) +
                                 " ls-files --others --exclude-standard -- tests/golden",
                             git_log(repo))
                      .output)) {
      files.push_back(f);
    }
  } else {
    for (const std::string& t : targets) {
      for (const std::string& f : resolve_goldens(repo, t, out.findings)) files.push_back(f);
    }
  }
  std::vector<std::string> answers;
  size_t moved = 0;
  for (const std::string& f : files) {
    const std::string before = at_ref(repo, base, f).value_or("");
    const std::string after = read_text(repo / f).value_or("");
    const GoldenDiff d = diff_golden(f, before, after);
    if (d.rows.empty() && !d.header_changed) continue;
    moved++;
    out.text += format_golden_diff(d, kMostRows);
    answers.push_back(golden_diff_json(d, kMostRows));
  }
  out.result = json_obj({{"base", json_str(base)}, {"files", json_arr(answers)}});
  out.summary = moved == 0 ? "no golden row differs from " + base
                           : std::to_string(moved) + " golden file" +
                                 (moved == 1 ? " differs" : "s differ") + " from " + base +
                                 "; say why each column moved in the PR";
  out.next = moved == 0 ? "" : "ge test";
  return out;
}

Outcome run_golden_write(const BuildOptions& o, const std::string& test_arg) {
  Outcome out;
  std::string test = fs::path(test_arg).stem().string();
  if (!test.ends_with("_test")) test += "_test";
  const std::string source_rel = "tests/" + test + ".cpp";
  const auto source = read_text(o.repo / source_rel);
  if (!source) {
    out.findings.push_back({Severity::error, "no test " + source_rel, source_rel,
                            "name a test in tests/: `ge golden write <topic>_test`"});
    out.exit = 2;
    return out;
  }
  const std::vector<std::string> goldens = goldens_named_in(*source);
  if (!build_into(o, out)) return out;
  fs::path exe;
  for (const fs::path& dir : {o.repo / "build" / o.preset / "tests", o.repo / "build" / o.preset}) {
    for (const char* ext : {"", ".exe"}) {
      std::error_code ec;
      if (fs::is_regular_file(dir / (test + ext), ec)) exe = dir / (test + ext);
    }
  }
  if (exe.empty()) {
    out.findings.push_back({Severity::error, "the build made no " + test + " executable",
                            "build/" + o.preset, "check the test's name, then `ge build`"});
    return out;
  }
  std::map<std::string, std::string> before;
  for (const std::string& g : goldens) before[g] = read_text(o.repo / g).value_or("");
  const fs::path log = build_log(o, "ge-golden");
  std::error_code ec;
  fs::remove(log, ec);
  set_env("GE_GOLDEN_WRITE", "1");
  const Logged wrote = run_to_log(quoted(exe), log);
  set_env("GE_GOLDEN_WRITE", nullptr);
  if (wrote.code != 0) {
    out.findings.push_back(
        {Severity::error,
         test + " failed while it wrote its golden (exit " + std::to_string(wrote.code) + ")",
         rel_path(o.repo, log),
         "read its FAIL lines in the log: a golden written from a failing run "
         "is not a baseline"});
  }
  const Logged held = run_to_log(quoted(exe), log);
  if (wrote.code == 0 && held.code != 0) {
    out.findings.push_back({Severity::error, test + " fails against the golden it just wrote",
                            rel_path(o.repo, log),
                            "the run is not deterministic, or the test writes rows it does not "
                            "read back: skill: determinism"});
  }
  std::vector<std::string> answers;
  for (const std::string& g : goldens) {
    const std::string after = read_text(o.repo / g).value_or("");
    const GoldenDiff d = diff_golden(g, before[g], after);
    if (d.rows.empty() && !d.header_changed && before[g] == after) continue;
    out.written.push_back(g);
    out.text += format_golden_diff(d, kMostRows);
    answers.push_back(golden_diff_json(d, kMostRows));
  }
  if (out.written.empty() && source->find("GE_GOLDEN_WRITE") == std::string::npos) {
    out.findings.push_back({Severity::warn,
                            test + " does not write its golden under GE_GOLDEN_WRITE", source_rel,
                            "its golden is written another way (its header says how), or give "
                            "it the GE_GOLDEN_WRITE branch (skill: testing)"});
  }
  out.result = json_obj({{"test", json_str(test)},
                         {"preset", json_str(o.preset)},
                         {"goldens", json_strs(goldens)},
                         {"files", json_arr(answers)},
                         {"log", json_str(rel_path(o.repo, log))}});
  out.summary = out.written.empty()
                    ? test + "'s goldens already hold what it computes"
                    : "rewrote " + std::to_string(out.written.size()) + " golden file" +
                          (out.written.size() == 1 ? " and the test passes on it"
                                                   : "s and the test passes on them");
  out.next = out.written.empty() ? "" : "say why each column moved in the PR, then ge test";
  return out;
}

Outcome run_digest(const BuildOptions& o, const std::string& lab, bool write) {
  Outcome out;
  if (!build_into(o, out)) return out;
  const fs::path baseline = o.repo / kDigestBaseline;
  const std::string rel(kDigestBaseline);
  std::error_code ec;
  const bool had = fs::is_regular_file(baseline, ec);
  const auto read_rows = [&] {
    std::map<std::string, std::string> rows;
    for (const std::string& line : lines_of(read_text(baseline).value_or(""))) {
      if (line[0] == '#') continue;
      const auto w = words_of(line);
      if (w.size() >= 2) rows[w[0]] = w[1];
    }
    return rows;
  };
  const auto was = read_rows();
  const fs::path exe = o.repo / "build" / o.preset / "labs";
  const fs::path previews = o.repo / "previews";
  const fs::path log = build_log(o, "ge-digest");
  fs::remove(log, ec);
  std::string cmd = quoted(exe) + " " + (lab.empty() ? std::string("--all") : lab) +
                    " --smoke --repeat=2 --quiet --out=" + quoted(previews);
  if (write) {
    fs::create_directories(baseline.parent_path(), ec);
    cmd += " --write-digests=" + quoted(baseline);
  } else if (had) {
    cmd += " --check-digests=" + quoted(baseline);
  }
  const Logged r = run_to_log(cmd, log);
  if (r.code == 2) {
    out.findings.push_back({Severity::error, "the labs refused the run: " + last_line(r.output),
                            rel_path(o.repo, log), "`ge lab` lists the labs; name one of them"});
    out.exit = 2;
    return out;
  }
  // The runner's FAIL lines, each with its indented advice as the next step.
  const auto lines = lines_of(r.output);
  for (size_t i = 0; i < lines.size(); i++) {
    if (!lines[i].starts_with("FAIL ")) continue;
    const std::string what = lines[i].substr(5);
    std::string next;
    while (i + 1 < lines.size() && lines[i + 1].starts_with("    ")) {
      next += (next.empty() ? "" : " ") + lines[++i].substr(4);
    }
    if (next.starts_with("next: ")) next = next.substr(6);
    out.findings.push_back({Severity::error, what, rel_path(o.repo, log),
                            next.empty() ? "read the log; skill: labs" : next});
  }
  if (r.code != 0 && out.findings.empty() && !write) {
    out.findings.push_back({Severity::error,
                            "the labs failed (exit " + std::to_string(r.code) + ")",
                            rel_path(o.repo, log), "read the log"});
  }
  // Each lab's digest from its report; the baseline's row beside it.
  std::vector<std::string> ids;
  if (lab.empty()) {
    static const std::regex kId("\"id\":\"([^\"]+)\"");
    const std::string listing = run_to_log(quoted(exe) + " --json", log).output;
    for (auto it = std::sregex_iterator(listing.begin(), listing.end(), kId);
         it != std::sregex_iterator(); ++it) {
      ids.push_back((*it)[1].str());
    }
    std::sort(ids.begin(), ids.end());
  } else {
    ids.push_back(lab);
  }
  static const std::regex kDigest("\"digest\": \"([0-9a-f]{16})\"");
  std::vector<std::string> rows;
  int moved = 0;
  for (const std::string& id : ids) {
    const std::string report = read_text(previews / id / "report.json").value_or("");
    std::smatch m;
    if (!std::regex_search(report, m, kDigest)) continue;
    const std::string digest = m[1].str();
    const auto it = was.find(id);
    const std::string before = it == was.end() ? "" : it->second;
    const std::string state = !had && !write     ? "no baseline"
                              : before.empty()   ? (write ? "added" : "not in the baseline")
                              : before == digest ? "same"
                              : write            ? "moved from " + before
                                                 : "baseline says " + before;
    if (!before.empty() && before != digest) moved++;
    out.text.append(id).append("  ").append(digest).append("  ").append(state).append("\n");
    rows.push_back(json_obj({{"lab", json_str(id)},
                             {"digest", json_str(digest)},
                             {"baseline", before.empty() ? "null" : json_str(before)},
                             {"state", json_str(state)}}));
  }
  if (write && read_rows() != was) out.written.push_back(rel);
  if (!had && !write) {
    out.findings.push_back({Severity::note, "the repo has no lab digest baseline", rel,
                            "ge digest --write writes one; commit it and `ge test` holds every "
                            "lab to it"});
  }
  out.result = json_obj({{"baseline", json_str(rel)},
                         {"labs", json_arr(rows)},
                         {"log", json_str(rel_path(o.repo, log))}});
  out.summary = write ? (out.written.empty()
                             ? rel + " already holds these digests"
                             : "rewrote " + rel + " for " + (lab.empty() ? "every lab" : lab) +
                                   (moved ? "; say why each moved in the PR" : ""))
                      : "the digests above, each beside " + rel;
  out.next = write ? "ge test" : "";
  return out;
}

}  // namespace ge::cli
