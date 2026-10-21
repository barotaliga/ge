// Reads a tests/golden/*.txt file: one row per line, whitespace-separated
// hex words (IEEE 754 bit patterns or plain hex integers).
#pragma once

#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "check.hpp"

namespace ge::test {

inline std::vector<std::vector<uint64_t>> golden_rows(const char* rel) {
  std::vector<std::vector<uint64_t>> rows;
  std::ifstream in(root_path(rel));
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream words(line);
    std::vector<uint64_t> row;
    std::string w;
    while (words >> w) row.push_back(std::stoull(w, nullptr, 16));
    if (!row.empty()) rows.push_back(std::move(row));
  }
  return rows;
}

inline double as_double(uint64_t u) {
  double v;
  std::memcpy(&v, &u, sizeof v);
  return v;
}

}  // namespace ge::test
