// The series file's reader and writer (series.hpp).
//
// ge:summary The series file's reader and writer
// ge:keywords series tsv read_series write_series parse
// ge:skill debug labs
// ge:test tests/lab_series_test.cpp
#include "lab/series.hpp"

#include <cstdlib>
#include <limits>

#include "core/num_text.hpp"

namespace ge::lab {

namespace {

std::vector<std::string_view> split_tabs(std::string_view line) {
  std::vector<std::string_view> out;
  size_t start = 0;
  for (;;) {
    const size_t tab = line.find('\t', start);
    out.push_back(line.substr(start, tab == std::string_view::npos ? tab : tab - start));
    if (tab == std::string_view::npos) break;
    start = tab + 1;
  }
  return out;
}

bool parse_number(std::string_view text, double& out) {
  if (text == "nan" || text == "NaN" || text.empty()) {
    out = std::numeric_limits<double>::quiet_NaN();
    return true;
  }
  const std::string s(text);
  char* end = nullptr;
  out = std::strtod(s.c_str(), &end);
  return end != s.c_str() && *end == '\0';
}

std::string number_text(double v) {
  if (v != v) return "nan";
  return core::num::to_string(v);
}

}  // namespace

size_t Series::rows() const {
  if (columns.empty()) return 0;
  const Column& c = columns.front();
  return c.text ? c.cell.size() : c.num.size();
}

const Column* Series::find(std::string_view name) const {
  for (const Column& c : columns) {
    if (c.name == name) return &c;
  }
  return nullptr;
}

Column* Series::find(std::string_view name) {
  for (Column& c : columns) {
    if (c.name == name) return &c;
  }
  return nullptr;
}

std::string Series::get(std::string_view key) const {
  for (const auto& [k, v] : meta) {
    if (k == key) return v;
  }
  return "";
}

void Series::set(std::string key, std::string value) {
  for (auto& [k, v] : meta) {
    if (k == key) {
      v = std::move(value);
      return;
    }
  }
  meta.emplace_back(std::move(key), std::move(value));
}

const Column* Series::x() const {
  if (const Column* named = find(get("x")); named && !named->text) return named;
  for (const Column& c : columns) {
    if (!c.text) return &c;
  }
  return nullptr;
}

std::string Series::unit(std::string_view column) const {
  return get("unit." + std::string(column));
}

bool Series::is_spans() const {
  const Column* name = find("name");
  const Column* depth = find("depth");
  const Column* start = find("start_ns");
  const Column* dur = find("dur_ns");
  return name && name->text && depth && !depth->text && start && !start->text && dur && !dur->text;
}

Series make_series(const std::vector<std::string>& columns) {
  Series s;
  for (const std::string& name : columns) s.columns.push_back(Column{name, false, {}, {}});
  return s;
}

void add_row(Series& s, const std::vector<double>& values) {
  for (size_t i = 0; i < s.columns.size(); i++) {
    s.columns[i].num.push_back(i < values.size() ? values[i]
                                                 : std::numeric_limits<double>::quiet_NaN());
  }
}

std::string write_series(const Series& s) {
  std::string out;
  for (const auto& [k, v] : s.meta) {
    out += "# ";
    out += k;
    out += ": ";
    out += v;
    out.push_back('\n');
  }
  for (size_t i = 0; i < s.columns.size(); i++) {
    if (i) out.push_back('\t');
    out += s.columns[i].name;
  }
  out.push_back('\n');
  const size_t n = s.rows();
  for (size_t r = 0; r < n; r++) {
    for (size_t i = 0; i < s.columns.size(); i++) {
      if (i) out.push_back('\t');
      const Column& c = s.columns[i];
      if (c.text)
        out += r < c.cell.size() ? c.cell[r] : "";
      else
        out += number_text(r < c.num.size() ? c.num[r] : std::numeric_limits<double>::quiet_NaN());
    }
    out.push_back('\n');
  }
  return out;
}

bool read_series(std::string_view text, Series& out, std::string& error) {
  out = Series{};
  std::vector<std::vector<std::string_view>> rows;
  bool header = false;
  size_t line_no = 0;
  size_t start = 0;
  while (start < text.size()) {
    size_t end = text.find('\n', start);
    if (end == std::string_view::npos) end = text.size();
    std::string_view line = text.substr(start, end - start);
    start = end + 1;
    line_no++;
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    if (line.empty()) continue;
    if (line.front() == '#') {
      std::string_view body = line.substr(1);
      while (!body.empty() && body.front() == ' ') body.remove_prefix(1);
      const size_t colon = body.find(':');
      if (colon == std::string_view::npos) continue;  // a plain comment
      std::string_view value = body.substr(colon + 1);
      while (!value.empty() && value.front() == ' ') value.remove_prefix(1);
      out.meta.emplace_back(std::string(body.substr(0, colon)), std::string(value));
      continue;
    }
    const auto cells = split_tabs(line);
    if (!header) {
      for (const auto name : cells) out.columns.push_back(Column{std::string(name), false, {}, {}});
      header = true;
      continue;
    }
    if (cells.size() != out.columns.size()) {
      error = "line " + std::to_string(line_no) + " has " + std::to_string(cells.size()) +
              " cells, the header " + std::to_string(out.columns.size()) +
              ": every row takes one tab-separated cell per column (engine/lab/series.hpp)";
      return false;
    }
    rows.push_back(cells);
  }
  if (!header) {
    error =
        "no header row: a series file is `# key: value` lines, a tab-separated header, then "
        "rows (engine/lab/series.hpp)";
    return false;
  }
  for (size_t i = 0; i < out.columns.size(); i++) {
    Column& c = out.columns[i];
    c.num.reserve(rows.size());
    for (const auto& row : rows) {
      double v;
      if (!parse_number(row[i], v)) {
        c.text = true;
        break;
      }
      c.num.push_back(v);
    }
    if (c.text) {
      c.num.clear();
      c.cell.reserve(rows.size());
      for (const auto& row : rows) c.cell.emplace_back(row[i]);
    }
  }
  return true;
}

}  // namespace ge::lab
