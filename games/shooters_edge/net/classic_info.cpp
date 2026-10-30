// Info strings (classic_info.hpp).
//
// ge:summary Info strings' parse, set and remove; names cleaned and made unique
// ge:keywords info_pairs info_set info_get sanitize_name unique_name
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_wire_test.cpp
#include "shooters_edge/net/classic_info.hpp"

namespace shooters_edge::net {

namespace {

bool clean(const std::string& s) {
  return s.find('\\') == std::string::npos && s.find('"') == std::string::npos;
}

std::string join(const std::vector<std::pair<std::string, std::string>>& pairs) {
  std::string s;
  for (const auto& [k, v] : pairs) s += "\\" + k + "\\" + v;
  return s;
}

}  // namespace

bool info_pairs(const std::string& info, std::vector<std::pair<std::string, std::string>>& out) {
  out.clear();
  if (info.empty()) return true;
  if (info.find('"') != std::string::npos) return false;
  size_t at = info[0] == '\\' ? 1 : 0;
  std::vector<std::string> parts;
  while (true) {
    const size_t next = info.find('\\', at);
    parts.push_back(info.substr(at, next == std::string::npos ? std::string::npos : next - at));
    if (next == std::string::npos) break;
    at = next + 1;
  }
  if (parts.size() % 2) return false;
  for (size_t i = 0; i < parts.size(); i += 2) {
    if (parts[i].empty() || parts[i].size() > kInfoKeyBytes) return false;
    out.emplace_back(parts[i], parts[i + 1]);
  }
  return true;
}

std::string info_get(const std::string& info, const std::string& key) {
  std::vector<std::pair<std::string, std::string>> pairs;
  if (!info_pairs(info, pairs)) return "";
  for (const auto& [k, v] : pairs)
    if (k == key) return v;
  return "";
}

bool info_set(std::string& info, const std::string& key, const std::string& value, size_t limit) {
  if (key.empty() || key.size() > kInfoKeyBytes || !clean(key) || !clean(value)) return false;
  std::vector<std::pair<std::string, std::string>> pairs;
  if (!info_pairs(info, pairs)) pairs.clear();
  bool found = false;
  for (size_t i = 0; i < pairs.size(); i++)
    if (pairs[i].first == key) {
      found = true;
      if (value.empty())
        pairs.erase(pairs.begin() + static_cast<std::ptrdiff_t>(i));
      else
        pairs[i].second = value;
      break;
    }
  if (!found && !value.empty()) {
    if (static_cast<int>(pairs.size()) >= kInfoMaxKeys) return false;
    pairs.emplace_back(key, value);
  }
  std::string next = join(pairs);
  if (next.size() > limit) return false;
  info = std::move(next);
  return true;
}

void info_remove(std::string& info, const std::string& key) {
  std::vector<std::pair<std::string, std::string>> pairs;
  if (!info_pairs(info, pairs)) return;
  std::vector<std::pair<std::string, std::string>> kept;
  for (auto& p : pairs)
    if (p.first != key) kept.push_back(std::move(p));
  info = join(kept);
}

bool info_valid(const std::string& info, size_t limit) {
  std::vector<std::pair<std::string, std::string>> pairs;
  return info.size() <= limit && info_pairs(info, pairs) &&
         static_cast<int>(pairs.size()) <= kInfoMaxKeys;
}

std::string info_public(const std::string& info) {
  std::vector<std::pair<std::string, std::string>> pairs, kept;
  if (!info_pairs(info, pairs)) return "";
  for (auto& p : pairs)
    if (p.first[0] != '_') kept.push_back(std::move(p));
  return join(kept);
}

std::string sanitize_name(const std::string& name, const std::string& fallback) {
  for (char c : name)
    if (c == '\\' || c == '\r' || c == '\n' || c == '$' || c == '#' || c == '"' || c == ';')
      return fallback;
  size_t a = 0, b = name.size();
  while (a < b && name[a] == ' ') a++;
  while (b > a && name[b - 1] == ' ') b--;
  std::string s = name.substr(a, b - a);
  std::string lower = s;
  for (char& c : lower)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  if (s.empty() || lower == "console") return fallback;
  return s;
}

std::string unique_name(const std::string& name, const std::vector<std::string>& taken) {
  auto held = [&](const std::string& n) {
    for (const std::string& t : taken)
      if (t == n) return true;
    return false;
  };
  if (!held(name)) return name;
  for (int n = 1; n < 100; n++) {
    std::string c = "(" + std::to_string(n) + ")" + name;
    if (c.size() > 10) c.resize(10);
    if (!held(c)) return c;
  }
  return name;
}

}  // namespace shooters_edge::net
