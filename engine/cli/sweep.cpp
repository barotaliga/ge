// The definitions behind sweep.hpp: the constant found and rewritten, the lab rebuilt and run at
// each value, the source put back, and the metrics read back side by side.
//
// ge:summary ge lab sweep's search, rewrite, rebuild-and-run loop, backup and metrics table
// ge:keywords cli ge lab sweep constant constexpr rewrite backup restore metrics table
// ge:skill agent-interface labs
// ge:test tests/cli_sweep_test.cpp
#include "cli/sweep.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <regex>
#include <system_error>

#include "cli/json.hpp"

namespace ge::cli {

namespace {

std::vector<std::string> split(const std::string& s, char sep) {
  std::vector<std::string> out;
  size_t a = 0;
  while (a <= s.size()) {
    const size_t b = s.find(sep, a);
    const std::string part = s.substr(a, b == std::string::npos ? std::string::npos : b - a);
    if (!part.empty()) out.push_back(part);
    if (b == std::string::npos) break;
    a = b + 1;
  }
  return out;
}

// A value the rewrite may put in a numeric or boolean initializer: a number, true or false.
bool plain_value(const std::string& v) {
  if (v == "true" || v == "false") return true;
  char* end = nullptr;
  std::strtod(v.c_str(), &end);
  return !v.empty() && end == v.c_str() + v.size();
}

// A value as a tag's part: letters, digits, '.', '-' and '_' kept, anything else '_'.
std::string tag_part(const std::string& s) {
  std::string out = s;
  for (char& c : out) {
    const bool keep = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                      c == '.' || c == '-' || c == '_';
    if (!keep) c = '_';
  }
  return out;
}

// The repository's C++ sources, repository-relative: git's tracked files when it is a checkout,
// else a walk that skips the build trees.
std::vector<std::string> sources(const fs::path& repo) {
  std::vector<std::string> out;
  const auto is_source = [](const std::string& p) {
    return p.ends_with(".hpp") || p.ends_with(".h") || p.ends_with(".cpp");
  };
  const fs::path list = repo / "build" / "ge-sweep-files.txt";
  std::error_code ec;
  fs::create_directories(list.parent_path(), ec);
  const std::string cmd = "git -C \"" + repo.string() + "\" ls-files > \"" + list.string() + "\"";
  if (run_quiet(cmd) == 0) {
    const std::string listed = read_text(list).value_or("");
    for (const auto view : split_lines(listed)) {
      const std::string p(view);
      if (is_source(p)) out.push_back(p);
    }
  }
  fs::remove(list, ec);
  if (!out.empty()) return out;
  for (auto it = fs::recursive_directory_iterator(repo, ec);
       it != fs::recursive_directory_iterator(); it.increment(ec)) {
    if (ec) break;
    if (it->is_directory() && skipped_dir(it->path().filename().string())) {
      it.disable_recursion_pending();
      continue;
    }
    const std::string p = rel_path(repo, it->path());
    if (it->is_regular_file() && is_source(p)) out.push_back(p);
  }
  std::sort(out.begin(), out.end());
  return out;
}

// Where an interrupted sweep's original text is kept: the file's path on the first line.
fs::path backup_of(const BuildOptions& o) {
  return o.repo / "build" / o.preset / "ge-sweep" / "backup.txt";
}

// Puts back a file an interrupted sweep left rewritten; its path, or "" when there was none.
std::string restore_backup(const BuildOptions& o) {
  const auto kept = read_text(backup_of(o));
  if (!kept) return "";
  const size_t nl = kept->find('\n');
  if (nl == std::string::npos) return "";
  std::string file = kept->substr(0, nl);
  std::string error;
  if (!write_text(o.repo / file, kept->substr(nl + 1), error)) return "";
  std::error_code ec;
  fs::remove(backup_of(o), ec);
  return file;
}

}  // namespace

std::vector<ConstantSite> find_constants(const std::string& text, const std::string& file,
                                         const std::string& name) {
  std::vector<ConstantSite> out;
  const std::vector<std::string> parts = [&] {
    std::vector<std::string> p;
    size_t a = 0;
    while (true) {
      const size_t b = name.find("::", a);
      p.push_back(name.substr(a, b == std::string::npos ? std::string::npos : b - a));
      if (b == std::string::npos) break;
      a = b + 2;
    }
    return p;
  }();
  const std::string& id = parts.back();
  if (id.empty()) return out;
  for (size_t i = 0; i + 1 < parts.size(); i++) {
    const std::regex opens("namespace[^{;]*\\b" + parts[i] + "\\b");
    if (!std::regex_search(text, opens)) return out;
  }
  const std::regex def("^(\\s*(?:inline\\s+|static\\s+)*constexpr\\s+[\\w:]+\\s+" + id +
                       "\\s*=\\s*)([^;{}]*?)(\\s*;.*)$");
  int line = 0;
  for (const auto view : split_lines(text)) {
    line++;
    const std::string l(view);
    std::smatch m;
    if (std::regex_match(l, m, def)) out.push_back({file, line, m.str(1), m.str(2), m.str(3)});
  }
  return out;
}

std::string with_value(const std::string& text, const ConstantSite& site,
                       const std::string& value) {
  std::string out;
  int line = 0;
  size_t a = 0;
  while (a < text.size()) {
    size_t b = text.find('\n', a);
    const bool last = b == std::string::npos;
    if (last) b = text.size();
    line++;
    out += line == site.line ? site.head + value + site.tail : text.substr(a, b - a);
    if (!last) out += '\n';
    a = b + 1;
  }
  return out;
}

std::string sweep_table(const std::vector<SweepRun>& runs, const std::string& current,
                        const std::vector<std::string>& filters) {
  std::vector<std::string> keys;
  std::map<std::string, std::string> units;
  std::vector<std::map<std::string, std::string>> cells(runs.size());
  for (size_t r = 0; r < runs.size(); r++) {
    for (const std::string& row : runs[r].rows) {
      const auto cols = split(row, '\t');
      if (cols.size() < 2 || cols[0].starts_with("#")) continue;
      const std::string& key = cols[0];
      const bool wanted =
          filters.empty() || std::any_of(filters.begin(), filters.end(), [&](const auto& f) {
            return key.find(f) != std::string::npos;
          });
      if (!wanted) continue;
      if (std::find(keys.begin(), keys.end(), key) == keys.end()) keys.push_back(key);
      if (cols.size() > 2) units[key] = cols[2];
      char buf[32];
      std::snprintf(buf, sizeof buf, "%.6g", std::strtod(cols[1].c_str(), nullptr));
      cells[r][key] = buf;
    }
  }
  std::vector<std::vector<std::string>> grid;
  std::vector<std::string> head{"metric"};
  for (const SweepRun& r : runs) head.push_back(r.value == current ? r.value + "*" : r.value);
  head.push_back("unit");
  grid.push_back(head);
  for (const std::string& k : keys) {
    std::vector<std::string> row{k};
    for (const auto& c : cells) row.push_back(c.count(k) ? c.at(k) : "-");
    row.push_back(units[k]);
    grid.push_back(row);
  }
  std::vector<size_t> width(head.size(), 0);
  for (const auto& row : grid) {
    for (size_t i = 0; i < row.size(); i++) width[i] = std::max(width[i], row[i].size());
  }
  std::string out;
  for (const auto& row : grid) {
    std::string line;
    for (size_t i = 0; i < row.size(); i++) {
      std::string cell = row[i];
      if (i + 1 < row.size()) cell.resize(width[i], ' ');
      line += (i ? "  " : "") + cell;
    }
    while (!line.empty() && line.back() == ' ') line.pop_back();
    out += line + "\n";
  }
  return out;
}

Outcome run_lab_sweep(const BuildOptions& o, const SweepOptions& s) {
  Outcome out;
  const size_t eq = s.set.find('=');
  const std::string name = eq == std::string::npos ? "" : s.set.substr(0, eq);
  const std::vector<std::string> values =
      eq == std::string::npos ? std::vector<std::string>{} : split(s.set.substr(eq + 1), ',');
  if (name.empty() || values.empty()) {
    out.findings.push_back({Severity::error, "--set wants <name>=<a>,<b>,..., got '" + s.set + "'",
                            "", "ge lab sweep " + s.id + " --set=kSlope=3.0,3.2,3.4"});
    out.exit = 2;
    return out;
  }
  for (const std::string& v : values) {
    if (!plain_value(v)) {
      out.findings.push_back({Severity::error, "'" + v + "' is not a number, true or false", "",
                              "pass plain values: --set=" + name + "=0.5,0.7"});
      out.exit = 2;
      return out;
    }
  }
  if (const std::string put = restore_backup(o); !put.empty()) {
    out.findings.push_back({Severity::note,
                            "put back " + put + ", which an earlier sweep left rewritten", put,
                            "nothing: the sweep goes on from the file as it was"});
  }

  // The definition: exactly one site, in --in's file when it is given.
  std::vector<ConstantSite> sites;
  const std::vector<std::string> files =
      s.in.empty() ? sources(o.repo) : std::vector<std::string>{fs::path(s.in).generic_string()};
  const size_t colon = name.rfind(':');
  const std::string id = colon == std::string::npos ? name : name.substr(colon + 1);
  for (const std::string& f : files) {
    const auto text = read_text(o.repo / f);
    if (!text || text->find(id) == std::string::npos) continue;
    for (ConstantSite& site : find_constants(*text, f, name)) sites.push_back(std::move(site));
  }
  if (sites.size() != 1) {
    std::string where;
    for (const ConstantSite& site : sites) {
      where += (where.empty() ? "" : ", ") + site.file + ":" + std::to_string(site.line);
    }
    out.findings.push_back(
        {Severity::error,
         sites.empty()
             ? "no one-line constexpr definition of " + name + " was found"
             : name + " is defined in " + std::to_string(sites.size()) + " places: " + where,
         s.in,
         sites.empty() ? "check the spelling, or name its file with --in=<path>"
                       : "qualify it (spec::" + name + ") or name its file with --in=<path>"});
    return out;
  }
  const ConstantSite site = sites.front();
  const fs::path file = o.repo / site.file;
  const std::string original = read_text(file).value_or("");
  std::string error;
  if (!write_text(backup_of(o), site.file + "\n" + original, error)) {
    out.findings.push_back({Severity::error, "could not keep a backup: " + error,
                            rel_path(o.repo, backup_of(o)), "check build/ is writable"});
    return out;
  }

  const fs::path exe = o.repo / "build" / o.preset / "labs";
  const fs::path previews = o.repo / "previews";
  const fs::path log = build_log(o, "ge-lab-sweep");
  std::error_code ec;
  fs::remove(log, ec);
  std::vector<SweepRun> runs;
  std::vector<std::string> dirs;
  bool ok = true;
  for (const std::string& v : values) {
    if (!write_text(file, with_value(original, site, v), error)) {
      out.findings.push_back({Severity::error, "could not rewrite " + site.file + ": " + error,
                              site.file, "check the file is writable"});
      ok = false;
      break;
    }
    const std::string at = std::string(name).append(" = ").append(v).append(": ");
    Outcome built;
    if (!build_targets(o, built, "labs")) {
      for (Finding& f : built.findings) {
        f.what.insert(0, at);
        out.findings.push_back(std::move(f));
      }
      ok = false;
      break;
    }
    const std::string tag = "sweep-" + tag_part(id) + "-" + tag_part(v);
    std::string cmd = "\"" + exe.string() + "\" " + s.id + " --out=\"" + previews.string() +
                      "\" --quiet --tag=" + tag;
    for (const std::string& w : s.passthrough) cmd += " \"" + w + "\"";
    const Logged r = run_to_log(cmd, log);
    const fs::path dir = previews / (s.id + "@" + tag);
    if (r.code == 2) {
      out.findings.push_back({Severity::error, "the lab refused its flags", rel_path(o.repo, log),
                              "ge lab " + s.id + " -- --help lists them"});
      out.exit = 2;
      ok = false;
      break;
    }
    if (r.code != 0) {
      out.findings.push_back({Severity::warn,
                              std::string(at)
                                  .append("the lab failed (exit ")
                                  .append(std::to_string(r.code))
                                  .append("); its metrics are what it measured"),
                              rel_path(o.repo, dir / "report.json"),
                              "read the report and the log"});
    }
    SweepRun run{v, {}};
    const std::string metrics = read_text(dir / "metrics.tsv").value_or("");
    for (const auto view : split_lines(metrics)) {
      run.rows.emplace_back(view);
    }
    runs.push_back(std::move(run));
    dirs.push_back(rel_path(o.repo, dir));
  }

  // Put the source back, and the labs on what it holds, whatever happened above.
  if (!write_text(file, original, error)) {
    out.findings.push_back({Severity::error, "could not put " + site.file + " back: " + error,
                            rel_path(o.repo, backup_of(o)),
                            "copy the backup's text (after its first line) over " + site.file});
    return out;
  }
  fs::remove(backup_of(o), ec);
  Outcome rebuilt;
  if (!build_targets(o, rebuilt, "labs")) {
    out.findings.push_back({Severity::warn, "the labs did not rebuild on the original " + name,
                            rel_path(o.repo, build_log(o, "ge-build")),
                            "ge build before running a lab by hand"});
  }
  if (!ok) return out;

  const std::vector<std::string> filters = split(s.metrics, ',');
  out.text = name + " in " + site.file + ":" + std::to_string(site.line) + " (now " + site.value +
             ", starred if swept)\n" + sweep_table(runs, site.value, filters);
  std::vector<std::string> run_json;
  for (size_t i = 0; i < runs.size(); i++) {
    run_json.push_back(json_obj({{"value", json_str(runs[i].value)}, {"dir", json_str(dirs[i])}}));
  }
  out.result = json_obj({{"constant", json_str(name)},
                         {"file", json_str(site.file)},
                         {"line", std::to_string(site.line)},
                         {"current", json_str(site.value)},
                         {"runs", json_arr(run_json)},
                         {"log", json_str(rel_path(o.repo, log))}});
  out.summary = "swept " + name + " over " + std::to_string(runs.size()) + " values through lab " +
                s.id + "; " + site.file + " is as it was";
  out.next = "set the value you want in " + site.file + ":" + std::to_string(site.line) +
             "; ge lab diff " + s.id + " --run --against=sweep-" + tag_part(id) +
             "-<value> diffs its pictures against a swept run";
  return out;
}

}  // namespace ge::cli
