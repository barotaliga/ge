// The toolkit's rows gathered and run (call.hpp).
//
// ge:summary The toolkit's rows gathered from every group, found by name and run with a call
// ge:keywords toolkit rows find_row run_row has_flag flag_value usage-error
// ge:skill agent-interface
// ge:test tests/toolkit_pak_test.cpp
#include "toolkit/call.hpp"

#include <vector>

#include "toolkit/mod.hpp"
#include "toolkit/pak.hpp"
#include "toolkit/qc.hpp"

namespace ge::toolkit {

bool has_flag(const Call& call, std::string_view name) {
  for (const auto& [n, v] : call.flags)
    if (n == name) return true;
  return false;
}

std::string flag_value(const Call& call, std::string_view name, std::string_view fallback) {
  for (const auto& [n, v] : call.flags)
    if (n == name) return v;
  return std::string(fallback);
}

std::span<const Row> rows() {
  // Every group's rows, in the order help lists them.
  static const std::vector<Row> all = [] {
    std::vector<Row> v;
    for (std::span<const Row> group : {pak_rows(), qc_rows(), mod_rows()})
      v.insert(v.end(), group.begin(), group.end());
    return v;
  }();
  return all;
}

const Row* find_row(std::string_view name) {
  for (const Row& r : rows())
    if (r.name == name) return &r;
  return nullptr;
}

Outcome run_row(const Row& row, const Call& call) {
  const int n = static_cast<int>(call.words.size());
  if (n < row.min_args || (row.max_args >= 0 && n > row.max_args)) {
    Outcome o;
    o.exit = 2;
    o.findings.push_back({Severity::error,
                          std::string(row.name) + " takes " + std::string(row.args) + ", not " +
                              std::to_string(n) + " word(s)",
                          "", call.program + " help " + std::string(row.name)});
    return o;
  }
  return row.run(call);
}

}  // namespace ge::toolkit
