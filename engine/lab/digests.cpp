// The lab digest baseline's two forms: a directory of one-line files (tests/digests/<lab>.txt)
// and the older single file of "<lab> <hex>" rows.
//
// ge:summary Reads and writes the lab digest baseline, a directory of one file a lab
// ge:keywords lab digests baseline directory one-file-per-lab single-file rows
// ge:skill labs
// ge:test tests/lab_runner_test.cpp
#include "lab/digests.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace ge::lab {

namespace fs = std::filesystem;

namespace {

constexpr std::string_view kExt = ".txt";

std::vector<std::pair<std::string, std::string>> file_rows(const std::string& path) {
  std::vector<std::pair<std::string, std::string>> out;
  std::ifstream f(path);
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream row(line);
    std::string id, hex;
    row >> id >> hex;
    if (!id.empty()) out.emplace_back(id, hex);
  }
  return out;
}

}  // namespace

bool digest_file_form(const std::string& path) {
  return path.size() >= kExt.size() && path.compare(path.size() - kExt.size(), kExt.size(), kExt) == 0;
}

std::map<std::string, std::string> read_digests(const std::string& path) {
  std::map<std::string, std::string> out;
  if (digest_file_form(path)) {
    for (const auto& [id, hex] : file_rows(path)) out[id] = hex;
    return out;
  }
  std::error_code ec;
  for (const auto& e : fs::directory_iterator(path, ec)) {
    const fs::path p = e.path();
    if (!e.is_regular_file() || p.extension() != kExt) continue;
    std::ifstream f(p);
    std::string hex;
    f >> hex;
    if (!hex.empty()) out[p.stem().string()] = hex;
  }
  return out;
}

std::vector<std::string> doubled_digests(const std::string& path) {
  std::vector<std::string> out;
  if (!digest_file_form(path)) return out;
  std::map<std::string, int> rows;
  for (const auto& [id, hex] : file_rows(path)) rows[id]++;
  for (const auto& [id, n] : rows) {
    if (n > 1) out.push_back(id);
  }
  return out;
}

bool write_digests(const std::string& path, const std::map<std::string, std::string>& digests) {
  if (digest_file_form(path)) {
    std::ofstream f(path, std::ios::binary);
    f << "# lab digests at their smoke flags: labs --all --smoke --write-digests\n";
    for (const auto& [id, hex] : digests) f << id << " " << hex << "\n";
    return static_cast<bool>(f);
  }
  std::error_code ec;
  fs::create_directories(path, ec);
  bool ok = true;
  for (const auto& [id, hex] : digests) {
    const fs::path p = fs::path(path) / (id + std::string(kExt));
    std::ifstream in(p);
    std::string was;
    in >> was;
    if (was == hex) continue;  // untouched: no write, no changed mtime
    std::ofstream f(p, std::ios::binary);
    f << hex << "\n";
    ok = ok && static_cast<bool>(f);
  }
  return ok;
}

}  // namespace ge::lab
