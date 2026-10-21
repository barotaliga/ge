// engine/racing/records against the TypeScript oracle (tests/golden/racing_records.txt,
// written by tools/parity/golden-racing.mjs): `beats` over non-figures and ties both ways,
// `split_gap` at every edge, and a book grown by `note_record` compared whole after each call.
#include "racing/records.hpp"

#include <string>
#include <vector>

#include "golden.hpp"

using ge::test::as_double;
namespace r = ge::racing;

namespace {

// Reads `has n splits...` at v[at], advancing `at`.
void read_splits(const std::vector<double>& v, size_t& at, r::RecordRow& row) {
  row.has_splits = v[at++] != 0;
  const auto n = size_t(v[at++]);
  row.splits.assign(v.begin() + std::ptrdiff_t(at), v.begin() + std::ptrdiff_t(at + n));
  at += n;
}

std::string id_of(double k) { return "r/" + std::to_string(int(k)); }

}  // namespace

TEST(records_match_oracle) {
  const auto rows = ge::test::golden_rows("tests/golden/racing_records.txt");
  r::Book<r::RecordRow> book;
  int counts[4] = {};
  for (const auto& raw : rows) {
    std::vector<double> v;
    v.reserve(raw.size());
    for (const uint64_t w : raw) v.push_back(as_double(w));
    const int op = int(v[0]);
    counts[op]++;
    if (op == 0) {
      const auto better = v[4] != 0 ? r::Better::Higher : r::Better::Lower;
      const double* standing = v[2] != 0 ? &v[3] : nullptr;
      CHECK(r::beats(v[1], standing, better) == (v[5] != 0));
    } else if (op == 1) {
      r::RecordRow rec;
      size_t at = 2;
      read_splits(v, at, rec);
      const auto index = int64_t(v[at]);
      const double time = v[at + 1];
      const auto gap = r::split_gap(v[1] != 0 ? &rec : nullptr, index, time);
      CHECK(gap.has_value() == (v[at + 2] != 0));
      if (gap) CHECK_BITS(*gap, v[at + 3]);
    } else if (op == 2) {
      const auto better = v[1] != 0 ? r::Better::Higher : r::Better::Lower;
      r::RecordRow run;
      run.value = v[3];
      run.at = v[4];
      size_t at = 5;
      read_splits(v, at, run);
      CHECK(r::note_record(book, id_of(v[2]), run, better) == (v[at] != 0));
    } else {
      const auto n = size_t(v[1]);
      CHECK(book.rows.size() == n);
      size_t at = 2;
      for (size_t k = 0; k < n && k < book.rows.size(); k++) {
        const auto& [id, row] = book.rows[k];
        CHECK(id == id_of(v[at]));
        CHECK(book.best(id) == &row);
        CHECK_BITS(row.value, v[at + 1]);
        CHECK_BITS(row.at, v[at + 2]);
        r::RecordRow want;
        at += 3;
        read_splits(v, at, want);
        CHECK(row.has_splits == want.has_splits);
        CHECK(row.splits.size() == want.splits.size());
        for (size_t i = 0; i < want.splits.size() && i < row.splits.size(); i++) {
          CHECK_BITS(row.splits[i], want.splits[i]);
        }
      }
    }
  }
  CHECK(counts[0] == 2 * 11 * 12 && counts[1] == 4 * 7 * 3 && counts[2] == 300 && counts[3] == 300);
  CHECK(book.best("r/none") == nullptr);
}
