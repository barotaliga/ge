// THE RECORD BOOK'S POLICY: the best figure a run has set, one row per whatever the game says
// names a row. Port of oss-game-framework `src/racing/records.ts`, bit for bit:
// - A row is beaten OUTRIGHT, never on a tie.
// - A figure that is not a figure (not finite, not positive) beats nothing.
// - Which way is better is the book's (`Better`): seconds lower, points higher.
// - A row may carry the clock at every crossing (`splits`), so a run can be told its gap to the
//   record at each checkpoint (`split_gap`).
//
// What names a row and what else a row carries (the skis it was set on) are the game's: a game
// row derives from `RecordRow`, and `Book<Row>` holds any of them. The oracle's book is an
// immutable object; here `note_record` writes the book in place, which is what a caller who
// replaces its book with the returned one observes. Rows keep insertion order, as the
// oracle's object keys do (game5's ids are never integer-like).
//
// Not ported: `readBook` (a stored JSON blob read row by row). It belongs with the record
// store and its JSON reader (M10). Parity: tests/racing_records_test.cpp against
// tests/golden/racing_records.txt.
#pragma once

#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ge::racing {

// Which way a book's figures improve.
enum class Better : uint8_t { Lower, Higher };

// The generic half of a row: the figure, when (a unix ms stamp), and the clock at each crossing.
// `has_splits` is whether the oracle's optional `splits` is present (an empty list is present).
struct RecordRow {
  double value = 0;
  double at = 0;
  bool has_splits = false;
  std::vector<double> splits;
};

// Whether `value` is a figure a run could have set.
inline bool is_figure(double value) { return std::isfinite(value) && value > 0; }

// Whether `value` beats the standing figure (null: no row stands) outright.
inline bool beats(double value, const double* standing, Better better = Better::Lower) {
  if (!is_figure(value)) return false;
  if (standing == nullptr) return true;
  return better == Better::Higher ? value > *standing : value < *standing;
}

// THE GAP AT A CROSSING: this run's clock at crossing `index` less the record's, s (negative
// is ahead). Empty where there is no record, no such crossing, or either clock is not finite.
inline std::optional<double> split_gap(const RecordRow* record, int64_t index, double time) {
  if (record == nullptr || !record->has_splits) return std::nullopt;
  if (index < 0 || index >= int64_t(record->splits.size())) return std::nullopt;
  const double then = record->splits[size_t(index)];
  if (!std::isfinite(then) || !std::isfinite(time)) return std::nullopt;
  return time - then;
}

// A book: rows by id, in insertion order. Linear lookup; a book is read once per run.
template <class Row>
struct Book {
  std::vector<std::pair<std::string, Row>> rows;

  // The row standing under `id`, or null.
  const Row* best(std::string_view id) const {
    for (const auto& [key, row] : rows) {
      if (key == id) return &row;
    }
    return nullptr;
  }
};

// File this run under `id` if it earned the row; returns whether it did. A row that already
// stands keeps its place in the order; a new one goes last.
template <class Row>
bool note_record(Book<Row>& book, std::string_view id, const Row& run,
                 Better better = Better::Lower) {
  for (auto& [key, row] : book.rows) {
    if (key != id) continue;
    if (!beats(run.value, &row.value, better)) return false;
    row = run;
    return true;
  }
  if (!beats(run.value, nullptr, better)) return false;
  book.rows.emplace_back(std::string(id), run);
  return true;
}

}  // namespace ge::racing
