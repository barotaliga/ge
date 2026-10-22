// THE SERIES FILE: the one text format every measurement over time is written in, so any of
// them can be charted, diffed and grepped the same way. A lab's trace, `ge bench`'s steps,
// `ge profile`'s zones and `ge mem`'s counters all write it; `ge graph` (and plot.hpp) read it.
//
//   # title: seed 38, the player's skier        <- optional `# key: value` lines first
//   # x: t_s                                     <- which column is x (default: the first)
//   # unit.speed: km/h                           <- a column's unit
//   t_s	speed	edge	gate                      <- the header: column names, tab-separated
//   0	0	0	0                                     <- one row per step, frame or sample
//   0.00833	0.42	-0.1	0
//
// Cells are tab-separated. A column whose every cell reads as a number is numeric; any other is
// text (a zone's name). Numbers are written shortest round-trip (num_text.hpp), so a file read
// back gives the same doubles. Missing cells are NaN, written "nan".
//
// TWO SHAPES, by their columns: SERIES (wide: x first, then one numeric column per measure, one
// row per step) and SPANS (a timeline: `name depth start_ns dur_ns`, one row per zone entered,
// depth its nesting), which plot.hpp draws as a flame timeline (`is_spans`).
//
// ge:summary The series text file (tab-separated, # key: value header): read, write, columns
// ge:keywords series tsv trace timeseries format file read_series write_series column spans
// ge:keywords bench profile mem graph data-format meta unit
// ge:skill debug labs
// ge:test tests/lab_series_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ge::lab {

struct Column {
  std::string name;
  bool text = false;
  std::vector<double> num;        // numeric columns
  std::vector<std::string> cell;  // text columns
};

struct Series {
  std::vector<std::pair<std::string, std::string>> meta;  // the `# key: value` lines, in order
  std::vector<Column> columns;

  size_t rows() const;
  // The column called `name`, or nullptr.
  const Column* find(std::string_view name) const;
  Column* find(std::string_view name);
  // A meta value ("" when absent).
  std::string get(std::string_view key) const;
  void set(std::string key, std::string value);
  // The x column: `# x:` when it names a numeric column, else the first numeric column.
  const Column* x() const;
  // A column's unit (`# unit.<name>:`), "" when absent.
  std::string unit(std::string_view column) const;
  // Whether this is a SPANS file: text `name`, numeric `depth`, `start_ns` and `dur_ns`.
  bool is_spans() const;
};

// A numeric series with these columns and no rows.
Series make_series(const std::vector<std::string>& columns);
// Append one row to a numeric series (one value per column).
void add_row(Series& s, const std::vector<double>& values);

std::string write_series(const Series& s);
// Parse `text` into `out`. On failure returns false and `error` says which line and why.
bool read_series(std::string_view text, Series& out, std::string& error);

}  // namespace ge::lab
