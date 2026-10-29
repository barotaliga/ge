// `ge credits check`'s body (credits.hpp): CREDITS.md read into sections, the directory walked,
// each mismatch and each licence that cannot be carried a finding.
//
// ge:summary ge credits check's body: parse CREDITS.md, walk the assets, judge the licences
// ge:keywords cli credits run_credits_check parse_credits licence_verdict asset_files table
// ge:skill agent-interface assets
// ge:test tests/cli_credits_test.cpp
#include "cli/credits.hpp"

#include <algorithm>
#include <cctype>
#include <set>

#include "cli/json.hpp"

namespace ge::cli {

namespace {

std::string lower(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

std::string_view trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
    s.remove_suffix(1);
  return s;
}

constexpr std::string_view kNext = "the free-assets skill: CREDITS.md, one section a source pack";

}  // namespace

std::vector<CreditSection> parse_credits(std::string_view text) {
  std::vector<CreditSection> out;
  int n = 0;
  for (std::string_view line : split_lines(text)) {
    n++;
    line = trim(line);
    if (line.starts_with("## ")) {
      out.push_back({std::string(trim(line.substr(3))), "", n, {}, {}});
      continue;
    }
    if (out.empty()) continue;
    for (std::string_view key : {"- Licence:", "- License:"}) {
      if (line.starts_with(key)) out.back().licence = std::string(trim(line.substr(key.size())));
    }
    // A table row whose first cell is a quoted path: | `sounds/x.wav` | ... |
    if (line.starts_with("|")) {
      const std::string_view cell = trim(line.substr(1, line.find('|', 1) - 1));
      if (cell.size() > 2 && cell.front() == '`' && cell.back() == '`') {
        out.back().files.emplace_back(cell.substr(1, cell.size() - 2));
        out.back().file_lines.push_back(n);
      }
    }
  }
  return out;
}

Severity licence_verdict(std::string_view licence) {
  const std::string l = lower(licence);
  if (l.empty()) return Severity::error;
  for (std::string_view bad : {"-sa", " sa ", "sharealike", "share-alike", "share alike", "-nc",
                               "noncommercial", "non-commercial", "-nd", "noderiv", "gpl"}) {
    if (l.find(bad) != std::string::npos) return Severity::error;
  }
  for (std::string_view good :
       {"cc0", "public domain", "cc-by", "cc by", "mit", "the repository"}) {
    if (l.starts_with(good)) return Severity::note;
  }
  return Severity::warn;
}

std::vector<std::string> asset_files(const fs::path& dir) {
  static const std::set<std::string> kExt = {".wav",  ".ogg",  ".flac", ".mp3", ".opus",
                                             ".glb",  ".gltf", ".bin",  ".png", ".jpg",
                                             ".jpeg", ".webp", ".ttf",  ".otf"};
  std::vector<std::string> out;
  std::error_code ec;
  for (auto it = fs::recursive_directory_iterator(dir, ec); !ec && it != fs::end(it);
       it.increment(ec)) {
    if (it->is_directory(ec) && skipped_dir(it->path().filename().string())) {
      it.disable_recursion_pending();
      continue;
    }
    if (!it->is_regular_file(ec) || !kExt.contains(lower(it->path().extension().string()))) {
      continue;
    }
    out.push_back(it->path().lexically_relative(dir).generic_string());
  }
  std::sort(out.begin(), out.end());
  return out;
}

Outcome run_credits_check(const fs::path& dir) {
  Outcome o;
  const fs::path file = dir / "CREDITS.md";
  const std::string where = rel_path(fs::current_path(), file);
  const std::optional<std::string> text = read_text(file);
  if (!text) {
    o.findings.push_back(
        {Severity::error, "no CREDITS.md in " + dir.string(), where, std::string(kNext)});
    return o;
  }
  const std::vector<CreditSection> sections = parse_credits(*text);
  std::set<std::string> credited;
  int rows = 0;
  for (const CreditSection& s : sections) {
    const Severity v = licence_verdict(s.licence);
    const std::string at = where + ":" + std::to_string(s.line);
    if (s.licence.empty()) {
      o.findings.push_back({Severity::error, "\"" + s.title + "\" names no licence", at,
                            "add a line `- Licence: ...` under its heading"});
    } else if (v == Severity::error) {
      o.findings.push_back(
          {Severity::error,
           "\"" + s.title + "\" is under " + s.licence + ", which this repository cannot carry", at,
           "replace its files with CC0 or CC BY ones (skill: free-assets)"});
    } else if (v == Severity::warn) {
      o.findings.push_back(
          {Severity::warn,
           "\"" + s.title + "\" is under " + s.licence + ", a licence ge does not know", at,
           "read it: it must allow redistribution, change and sale"});
    }
    for (size_t k = 0; k < s.files.size(); k++) {
      rows++;
      credited.insert(s.files[k]);
      std::error_code ec;
      if (!fs::is_regular_file(dir / s.files[k], ec)) {
        o.findings.push_back({Severity::error, "credits " + s.files[k] + ", which is not there",
                              where + ":" + std::to_string(s.file_lines[k]),
                              "remove the row, or fix its path to the file's"});
      }
    }
  }
  const std::vector<std::string> assets = asset_files(dir);
  int missing = 0;
  for (const std::string& a : assets) {
    if (credited.contains(a)) continue;
    missing++;
    o.findings.push_back({Severity::error, a + " has no credit",
                          rel_path(fs::current_path(), dir / a),
                          "add it to its source pack's table in CREDITS.md, or remove it"});
  }
  o.text = "  " + std::to_string(sections.size()) + " source(s), " + std::to_string(rows) +
           " credited file(s), " + std::to_string(assets.size()) + " asset file(s)\n";
  o.result = json_obj({{"sources", json_num(static_cast<long long>(sections.size()))},
                       {"credited", json_num(rows)},
                       {"assets", json_num(static_cast<long long>(assets.size()))},
                       {"uncredited", json_num(missing)}});
  o.summary = "every one of " + std::to_string(assets.size()) +
              " asset file(s) credited under a licence this repository can carry";
  return o;
}

}  // namespace ge::cli
