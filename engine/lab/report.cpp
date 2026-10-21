// The report's text, its JSON, its TSV and its contact sheet.
//
// ge:summary The lab report's writers: text, JSON, TSV, Markdown contact sheet, compare table
// ge:keywords lab report json tsv contact-sheet compare before-after
// ge:skill labs
// ge:test tests/lab_runner_test.cpp
#include "lab/report.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "lab/png.hpp"

namespace ge::lab {

namespace {

std::string num(double v, const char* fmt = "%.6g") {
  if (v != v) return "nan";
  char b[40];
  std::snprintf(b, sizeof b, fmt, v);
  return b;
}

std::string json_str(const std::string& s) {
  std::string out = "\"";
  for (const char c : s) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char b[8];
          std::snprintf(b, sizeof b, "\\u%04x", c);
          out += b;
        } else {
          out += c;
        }
    }
  }
  return out + "\"";
}

std::string json_num(double v) {
  if (v != v || v - v != 0) return "null";  // NaN or an infinity: JSON has neither
  return num(v, "%.17g");
}

std::string json_pairs(const std::vector<std::pair<std::string, std::string>>& pairs) {
  std::string out = "{";
  for (size_t i = 0; i < pairs.size(); i++) {
    out += (i ? ", " : "") + json_str(pairs[i].first) + ": " + json_str(pairs[i].second);
  }
  return out + "}";
}

std::string shot_json(const Shot& s) {
  return "{\"id\": " + json_str(s.id) + ", \"title\": " + json_str(s.title) +
         ", \"caption\": " + json_str(s.caption) + ", \"look_for\": " + json_str(s.look_for) +
         ", \"inputs\": " + json_pairs(s.inputs) + ", \"file\": " + json_str(s.id + ".png") + "}";
}

// A table as aligned text columns.
std::string format_table(const LabTable& t) {
  std::vector<size_t> w(t.columns.size(), 0);
  for (size_t c = 0; c < t.columns.size(); c++) w[c] = t.columns[c].size();
  for (const auto& row : t.rows) {
    for (size_t c = 0; c < row.size() && c < w.size(); c++) w[c] = std::max(w[c], row[c].size());
  }
  auto line = [&](const std::vector<std::string>& cells) {
    std::string out = " ";
    for (size_t c = 0; c < w.size(); c++) {
      const std::string& cell = c < cells.size() ? cells[c] : "";
      out += " " + cell + std::string(w[c] - cell.size(), ' ');
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out + "\n";
  };
  std::string out = "  " + t.title + "\n" + line(t.columns);
  for (const auto& row : t.rows) out += line(row);
  return out;
}

bool write_text(const std::string& path, const std::string& text, std::string& error) {
  std::ofstream f(path, std::ios::binary);
  f << text;
  if (!f) error = "could not write " + path + " (is the directory writable? pass --out=DIR)";
  return static_cast<bool>(f);
}

}  // namespace

std::string hex64(uint64_t v) {
  char b[17];
  std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(v));
  return b;
}

std::string format_report(const Lab& lab, const std::string& dir) {
  const LabSpec& spec = lab.spec();
  std::string out = "── lab " + std::string(spec.id) + " (" + std::string(kind_name(spec.kind)) +
                    ", " + spec.owner + ", skill " + spec.skill + ") ──\n";
  out += "  " + std::string(spec.question) + "\n  inputs:";
  for (const auto& [k, v] : lab.args()) out.append(" --").append(k).append("=").append(v);
  out += "\n";
  if (!lab.metrics().empty()) {
    LabTable t{"metrics", {"key", "value", "unit", ""}, {}};
    for (const Metric& m : lab.metrics()) {
      t.rows.push_back({m.key, num(m.value), m.unit, m.det == Det::measured ? "(measured)" : ""});
    }
    out += format_table(t);
  }
  for (const LabTable& t : lab.tables()) out += format_table(t);
  for (const std::string& n : lab.notes()) out += "  " + n + "\n";
  for (const auto& s : lab.shots()) out += "  picture: " + dir + "/" + s.shot.id + ".png\n";
  if (!lab.shots().empty()) out += "  contact sheet: " + dir + "/index.md\n";
  for (const std::string& f : lab.failures()) out += "  FAIL " + f + "\n";
  out += "  digest " + hex64(lab.digest()) + "\n";
  return out;
}

bool write_report(const Lab& lab, const std::string& dir, std::string& error) {
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    error = "could not create " + dir + ": " + ec.message() + " (pass --out=DIR)";
    return false;
  }
  const LabSpec& spec = lab.spec();

  std::string tsv = "# key\tvalue\tunit\tdet\n";
  for (const Metric& m : lab.metrics()) {
    tsv += m.key + "\t" + num(m.value, "%.17g") + "\t" + m.unit + "\t" +
           (m.det == Det::measured ? "measured" : "deterministic") + "\n";
  }
  if (!write_text(dir + "/metrics.tsv", tsv, error)) return false;

  std::string j =
      "{\n  \"lab\": " + json_str(spec.id) +
      ",\n  \"kind\": " + json_str(std::string(kind_name(spec.kind))) +
      ",\n  \"owner\": " + json_str(spec.owner) + ",\n  \"subject\": " + json_str(spec.subject) +
      ",\n  \"skill\": " + json_str(spec.skill) + ",\n  \"question\": " + json_str(spec.question) +
      ",\n  \"inputs\": " + json_pairs(lab.args()) + ",\n  \"metrics\": [";
  for (size_t i = 0; i < lab.metrics().size(); i++) {
    const Metric& m = lab.metrics()[i];
    j += std::string(i ? "," : "") + "\n    {\"key\": " + json_str(m.key) +
         ", \"value\": " + json_num(m.value) + ", \"unit\": " + json_str(m.unit) +
         ", \"deterministic\": " + (m.det == Det::deterministic ? "true" : "false") + "}";
  }
  j += "\n  ],\n  \"tables\": [";
  for (size_t i = 0; i < lab.tables().size(); i++) {
    const LabTable& t = lab.tables()[i];
    j += std::string(i ? "," : "") + "\n    {\"title\": " + json_str(t.title) + ", \"columns\": [";
    for (size_t c = 0; c < t.columns.size(); c++) j += (c ? ", " : "") + json_str(t.columns[c]);
    j += "], \"rows\": [";
    for (size_t r = 0; r < t.rows.size(); r++) {
      j += r ? ", [" : "[";
      for (size_t c = 0; c < t.rows[r].size(); c++) j += (c ? ", " : "") + json_str(t.rows[r][c]);
      j += "]";
    }
    j += "]}";
  }
  j += "\n  ],\n  \"notes\": [";
  for (size_t i = 0; i < lab.notes().size(); i++) j += (i ? ", " : "") + json_str(lab.notes()[i]);
  j += "],\n  \"shots\": [";
  for (size_t i = 0; i < lab.shots().size(); i++) {
    j += std::string(i ? "," : "") + "\n    " + shot_json(lab.shots()[i].shot);
  }
  j += "\n  ],\n  \"failures\": [";
  for (size_t i = 0; i < lab.failures().size(); i++) {
    j += (i ? ", " : "") + json_str(lab.failures()[i]);
  }
  j += "],\n  \"digest\": " + json_str(hex64(lab.digest())) + "\n}\n";
  if (!write_text(dir + "/report.json", j, error)) return false;

  if (lab.shots().empty()) return true;
  std::string md =
      "# Lab `" + std::string(spec.id) + "`: contact sheet\n\n" + spec.question + "\n\nInputs:";
  for (const auto& [k, v] : lab.args()) md.append(" `--").append(k).append("=").append(v) += "`";
  md += "\n";
  for (const auto& taken : lab.shots()) {
    const Shot& s = taken.shot;
    const std::string file = s.id + ".png";
    const std::string path = dir + "/" + s.id + ".png";
    if (!write_png(path, taken.picture)) {
      error = "could not write " + path + " (is the directory writable?)";
      return false;
    }
    if (!write_text(dir + "/" + s.id + ".json", shot_json(s) + "\n", error)) return false;
    md += "\n## " + s.title + "\n\n![" + s.title + "](" + file + ")\n\n" + s.caption + "\n";
    if (!s.look_for.empty()) md += "\n**Look for:** " + s.look_for + "\n";
    if (!s.inputs.empty()) {
      md += "\n**Inputs:**";
      for (const auto& [k, v] : s.inputs) md.append(" ").append(k).append("=`").append(v) += "`";
      md += "\n";
    }
  }
  return write_text(dir + "/index.md", md, error);
}

std::vector<Metric> read_metrics(const std::string& path) {
  std::vector<Metric> out;
  std::ifstream f(path);
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream row(line);
    std::string key, value, unit, det;
    std::getline(row, key, '\t');
    std::getline(row, value, '\t');
    std::getline(row, unit, '\t');
    std::getline(row, det, '\t');
    out.push_back(Metric{key, std::strtod(value.c_str(), nullptr), unit,
                         det == "measured" ? Det::measured : Det::deterministic});
  }
  return out;
}

std::string format_compare(const std::vector<Metric>& before, const std::vector<Metric>& after,
                           const std::string& before_name) {
  LabTable t{"compare against " + before_name, {"key", "before", "after", "change", "unit"}, {}};
  auto find = [](const std::vector<Metric>& ms, const std::string& key) -> const Metric* {
    for (const Metric& m : ms) {
      if (m.key == key) return &m;
    }
    return nullptr;
  };
  for (const Metric& a : after) {
    const Metric* b = find(before, a.key);
    std::string change = "new";
    if (b) {
      const double d = a.value - b->value;
      change = d == 0 ? "=" : (d > 0 ? "+" : "") + num(d);
      if (d != 0 && b->value != 0) change += " (" + num(100 * d / b->value, "%+.1f") + "%)";
    }
    t.rows.push_back({a.key, b ? num(b->value) : "-", num(a.value), change, a.unit});
  }
  for (const Metric& b : before) {
    if (!find(after, b.key)) t.rows.push_back({b.key, num(b.value), "-", "gone", b.unit});
  }
  return format_table(t);
}

}  // namespace ge::lab
