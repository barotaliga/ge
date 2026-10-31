// WHY A LAB'S DIGEST MOVED. A lab's digest is one hash over everything deterministic it found, so
// a moved digest says only that something did. Each run also writes digest.tsv beside its report
// (Lab::ledger: every input the digest hashed, a line each, "kind\tname\tvalue"); two of them read
// back and compared here name what moved: a metric's two values, a table row's cells by column,
// a row or a metric that came or went, a picture or a file whose bytes changed, or the same inputs
// hashed in another order. `ge digest <lab> --why` prints it against a ref's run or a kept one.
//
// ge:summary Two runs' digest ledgers compared: which metrics, rows, columns, shots and files moved
// ge:keywords lab digest why ledger moved metric row column shot file explain digest-tsv
// ge:skill labs
// ge:test tests/lab_digest_why_test.cpp
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ge::lab {

struct LedgerLine {
  std::string kind, name, value;     // "metric", "columns", "row", "shot", "file"
  std::vector<std::string> columns;  // a row's table's columns, from the columns line before it
};

// digest.tsv's text parsed; '#' lines and blank lines skipped.
std::vector<LedgerLine> parse_ledger(std::string_view text);
// The file read and parsed; `found` false when it could not be read.
std::vector<LedgerLine> read_ledger(const std::string& path, bool* found = nullptr);

struct DigestWhy {
  std::vector<std::string> lines;  // what moved, a line each, at most the limit and a tail
  size_t changed = 0, added = 0, removed = 0;
  bool reordered = false;  // the same inputs, hashed in another order
  bool same() const { return changed == 0 && added == 0 && removed == 0 && !reordered; }
};

// `before` against `after`: every input that changed value, came or went, in `after`'s order
// (those that went last). At most `limit` lines, then one saying how many more.
DigestWhy explain_digest(const std::vector<LedgerLine>& before,
                         const std::vector<LedgerLine>& after, size_t limit = 40);

}  // namespace ge::lab
