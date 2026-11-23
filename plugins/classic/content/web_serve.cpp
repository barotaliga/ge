// A classic game server's files on the web (web_serve.hpp).
//
// ge:summary The classic server's web lookup: folder, kind and switches checked, then the file
// ge:keywords web-serve web-files allow-list
// ge:skill classic net
// ge:test tests/classic_content_web_serve_test.cpp
#include "classic/content/web_serve.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace ge::classic::content {

namespace {

std::string lowered(std::string s) {
  for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return s;
}

}  // namespace

bool web_serve_allows(const WebServe& rules, const std::string& path, std::string& file) {
  const size_t slash = path.find('/');
  if (slash == std::string::npos || slash + 1 >= path.size()) return false;
  const std::string folder = lowered(path.substr(0, slash));
  if (folder != lowered(rules.game_folder) && folder != lowered(rules.base_folder)) return false;
  file = lowered(path.substr(slash + 1));
  if (file.find('/') == std::string::npos) return false;  // in a folder of the game's, always
  const size_t dot = file.rfind('.');
  if (dot == std::string::npos || file.find('/', dot) != std::string::npos) return false;
  const std::string kind = file.substr(dot + 1);
  if (std::find(rules.kinds.begin(), rules.kinds.end(), kind) == rules.kinds.end()) return false;
  return !rules.allowed || rules.allowed(file);
}

ge::net::FileLookup web_serve_lookup(const SearchPath& files, WebServe rules) {
  return [&files, rules = std::move(rules)](const std::string& path, std::vector<uint8_t>& out) {
    std::string file;
    if (!web_serve_allows(rules, path, file)) return false;
    std::vector<uint8_t> store;
    const std::span<const uint8_t> got = find_file(files, file, store);
    out.assign(got.begin(), got.end());
    return !out.empty();
  };
}

}  // namespace ge::classic::content
