// Reads a tests/golden/*.txt file: one row per line, whitespace-separated
// hex words (IEEE 754 bit patterns or plain hex integers), or words of text (golden_words). Lines
// starting with '#' are provenance (which Node wrote the file) and are skipped.
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
    if (!line.empty() && line[0] == '#') continue;  // provenance line
    std::istringstream words(line);
    std::vector<uint64_t> row;
    std::string w;
    while (words >> w) row.push_back(std::stoull(w, nullptr, 16));
    if (!row.empty()) rows.push_back(std::move(row));
  }
  return rows;
}

// A text golden file (a codec's strings): each line split into words, '#' lines skipped.
inline std::vector<std::vector<std::string>> golden_words(const char* rel) {
  std::vector<std::vector<std::string>> rows;
  std::ifstream in(root_path(rel));
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line[0] == '#') continue;
    std::istringstream words(line);
    std::vector<std::string> row;
    std::string w;
    while (words >> w) row.push_back(std::move(w));
    if (!row.empty()) rows.push_back(std::move(row));
  }
  return rows;
}

// A hex word of a text golden file back to its bytes ("-" is empty).
inline std::string unhex(const std::string& h) {
  std::string out;
  if (h == "-") return out;
  for (size_t i = 0; i + 1 < h.size(); i += 2) {
    out += static_cast<char>(std::stoi(h.substr(i, 2), nullptr, 16));
  }
  return out;
}

inline double as_double(uint64_t u) {
  double v;
  std::memcpy(&v, &u, sizeof v);
  return v;
}

}  // namespace ge::test
