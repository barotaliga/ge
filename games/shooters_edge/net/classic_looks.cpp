// THE LOOKS A SERVER OFFERS (classic_looks.hpp).
//
// ge:summary The skins and player figures a server offers, listed for `listskins`
// ge:keywords listskins looks_offered
// ge:skill net
// ge:test tests/shooters_edge_classic_looks_test.cpp
#include "shooters_edge/net/classic_looks.hpp"

#include <map>
#include <set>
#include <string_view>

namespace shooters_edge::net {
namespace {

// The name of a picture file without its extension; empty when it is not one.
std::string picture_name(std::string_view file) {
  for (const char* end : {".pcx", ".png", ".tga"})
    if (file.size() > 4 && file.ends_with(end)) return std::string(file.substr(0, file.size() - 4));
  return {};
}

std::string joined(const std::set<std::string>& names) {
  std::string out;
  for (const std::string& n : names) {
    if (!out.empty()) out += ", ";
    out += n;
  }
  return out;
}

}  // namespace

std::string looks_offered(std::span<const std::string> names, bool downloads) {
  std::set<std::string> skins;
  std::map<std::string, std::set<std::string>> pictures;  // folder -> its skins
  std::set<std::string> figures;
  for (const std::string& n : names) {
    const std::string_view v = n;
    if (v.starts_with("skins/") && v.find('/', 6) == std::string_view::npos) {
      const std::string s = picture_name(v.substr(6));
      if (!s.empty()) skins.insert(s);
      continue;
    }
    if (!v.starts_with("players/")) continue;
    const std::string_view rest = v.substr(8);
    const size_t slash = rest.find('/');
    if (slash == std::string_view::npos || slash == 0) continue;
    const std::string folder(rest.substr(0, slash));
    const std::string_view file = rest.substr(slash + 1);
    if (file.find('/') != std::string_view::npos) continue;
    if (file == "tris.md2" || file == "tris.mdl") {
      figures.insert(folder);
      continue;
    }
    const std::string s = picture_name(file);
    if (!s.empty() && !s.ends_with("_i") && s != "weapon") pictures[folder].insert(s);
  }
  std::string out = skins.empty() ? "This server has no skins.\n"
                                  : "Skins on this server: " + joined(skins) + ".\n";
  if (figures.empty()) {
    out += "This server has no player models.\n";
  } else {
    out += "Player models on this server:";
    bool first = true;
    for (const std::string& f : figures) {
      out += first ? " " : ", ";
      first = false;
      out += f;
      const auto p = pictures.find(f);
      if (p != pictures.end() && !p->second.empty()) out += " (" + joined(p->second) + ")";
    }
    out += ".\n";
  }
  if (!downloads) out += "Downloads are off here, so you see only the ones you already have.\n";
  return out;
}

}  // namespace shooters_edge::net
