// Two digest ledgers compared (digest_why.hpp).
//
// ge:summary Two runs' digest ledgers compared, the reading of digest.tsv and the explanation
// ge:keywords lab digest why ledger moved explain digest-tsv column cells
// ge:skill labs
// ge:test tests/lab_digest_why_test.cpp
#include "lab/digest_why.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>
#include <utility>

namespace ge::lab {

namespace {

std::vector<std::string> split_tabs(std::string_view s) {
  std::vector<std::string> out;
  size_t at = 0;
  while (true) {
    const size_t tab = s.find('\t', at);
    out.emplace_back(s.substr(at, tab == std::string_view::npos ? s.npos : tab - at));
    if (tab == std::string_view::npos) break;
    at = tab + 1;
  }
  return out;
}

std::string key_of(const LedgerLine& l) { return l.kind + "\t" + l.name; }

// A row's cells that differ, each by its column's name ("speed 31.2 -> 31.4").
std::string row_change(const LedgerLine& was, const LedgerLine& now) {
  const auto a = split_tabs(was.value), b = split_tabs(now.value);
  std::string out;
  for (size_t c = 0; c < std::max(a.size(), b.size()); c++) {
    const std::string x = c < a.size() ? a[c] : "(none)", y = c < b.size() ? b[c] : "(none)";
    if (x == y) continue;
    const std::string col = c < now.columns.size() && !now.columns[c].empty()
                                ? now.columns[c]
                                : "column " + std::to_string(c + 1);
    out.append(out.empty() ? "" : ", ").append(col).append(" ").append(x).append(" -> ").append(y);
  }
  return out;
}

std::string shown(const LedgerLine& l) {
  if (l.kind == "row" || l.kind == "columns") {
    std::string v = l.value;
    for (char& c : v) {
      if (c == '\t') c = '|';
    }
    return v;
  }
  return l.value;
}

std::string change(const LedgerLine& was, const LedgerLine& now) {
  const std::string head = now.kind + " " + now.name + ": ";
  if (now.kind == "row") return head + row_change(was, now);
  if (now.kind == "shot" && was.value != "measured" && now.value != "measured") {
    return head + "its pixels changed (ge lab <lab> --before=<ref> --shots=" + now.name +
           " draws the change)";
  }
  if (now.kind == "file" && was.value != "measured" && now.value != "measured") {
    const size_t a = was.value.find(' '), b = now.value.find(' ');
    return head + "its bytes changed (" + was.value.substr(a + 1) + " -> " +
           now.value.substr(b + 1) + ")";
  }
  return head + shown(was) + " -> " + shown(now);
}

}  // namespace

std::vector<LedgerLine> parse_ledger(std::string_view text) {
  std::vector<LedgerLine> out;
  std::vector<std::string> columns;
  size_t at = 0;
  while (at < text.size()) {
    size_t end = text.find('\n', at);
    if (end == std::string_view::npos) end = text.size();
    std::string_view line = text.substr(at, end - at);
    at = end + 1;
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    if (line.empty() || line.front() == '#') continue;
    const size_t t1 = line.find('\t');
    if (t1 == std::string_view::npos) continue;
    const size_t t2 = line.find('\t', t1 + 1);
    LedgerLine l;
    l.kind = std::string(line.substr(0, t1));
    l.name = std::string(line.substr(t1 + 1, t2 == line.npos ? line.npos : t2 - t1 - 1));
    if (t2 != line.npos) l.value = std::string(line.substr(t2 + 1));
    if (l.kind == "columns") columns = split_tabs(l.value);
    if (l.kind == "row") l.columns = columns;
    out.push_back(std::move(l));
  }
  return out;
}

std::vector<LedgerLine> read_ledger(const std::string& path, bool* found) {
  std::ifstream in(path, std::ios::binary);
  if (found) *found = static_cast<bool>(in);
  if (!in) return {};
  std::ostringstream text;
  text << in.rdbuf();
  return parse_ledger(text.str());
}

DigestWhy explain_digest(const std::vector<LedgerLine>& before,
                         const std::vector<LedgerLine>& after, size_t limit) {
  DigestWhy why;
  std::map<std::string, const LedgerLine*> was;
  for (const LedgerLine& l : before) was.emplace(key_of(l), &l);
  std::map<std::string, bool> kept;
  std::vector<std::string> all;
  for (const LedgerLine& now : after) {
    const auto it = was.find(key_of(now));
    if (it == was.end()) {
      why.added++;
      const bool bytes = now.kind == "shot" || now.kind == "file";
      all.push_back("+ " + now.kind + " " + now.name +
                    (now.value.empty() || bytes ? "" : ": " + shown(now)));
      continue;
    }
    kept[it->first] = true;
    if (it->second->value == now.value) continue;
    why.changed++;
    all.push_back(change(*it->second, now));
  }
  for (const LedgerLine& l : before) {
    if (kept.count(key_of(l))) continue;
    why.removed++;
    all.push_back("- " + l.kind + " " + l.name);
  }
  if (why.changed == 0 && why.added == 0 && why.removed == 0) {
    for (size_t i = 0; i < std::min(before.size(), after.size()); i++) {
      if (key_of(before[i]) == key_of(after[i])) continue;
      why.reordered = true;
      all.push_back("the same inputs hashed in another order, from " + after[i].kind + " " +
                    after[i].name + " (it was " + before[i].kind + " " + before[i].name + ")");
      break;
    }
  }
  for (size_t i = 0; i < all.size() && i < limit; i++) why.lines.push_back(all[i]);
  if (all.size() > limit) {
    why.lines.push_back("... and " + std::to_string(all.size() - limit) +
                        " more (the two digest.tsv files hold them all)");
  }
  return why;
}

}  // namespace ge::lab
