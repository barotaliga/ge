// The packed formats' names and the archive pick (demo_pack.hpp).
//
// ge:summary The packed extensions and the document names an archive's pick leaves out
// ge:keywords packed_extension demo_name document readme nfo diz plain_extension
// ge:keywords listed_extensions
// ge:skill shooter
// ge:test tests/shooters_edge_demo_test.cpp
#include "shooters_edge/client/demo_pack.hpp"

#include <algorithm>

namespace shooters_edge::client {

std::string packed_extension(DemoFormat base, bool voice) {
  if (base == DemoFormat::single) return ".demz";
  if (base == DemoFormat::sequel) return ".dm2z";
  if (voice) return voice_extension(base) + "z";
  return base == DemoFormat::multiview ? ".mvdz" : ".qwdz";
}

std::string plain_extension(DemoFormat base) {
  if (base == DemoFormat::single) return ".dem";
  if (base == DemoFormat::multiview) return ".mvd";
  if (base == DemoFormat::sequel) return ".dm2";
  const std::string voice = voice_extension(base);
  return voice.substr(0, voice.size() - 1);
}

std::vector<std::string> listed_extensions(std::span<const std::string_view> game) {
  std::vector<std::string> out(game.begin(), game.end());
  for (const DemoFormat f : {DemoFormat::single, DemoFormat::client, DemoFormat::multiview}) {
    std::string e = plain_extension(f);
    if (std::find(out.begin(), out.end(), e) == out.end()) out.push_back(std::move(e));
  }
  return out;
}

bool demo_name(std::string_view name) {
  std::string n(name);
  for (char& c : n) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
  const size_t slash = n.find_last_of('/');
  const std::string base = slash == std::string::npos ? n : n.substr(slash + 1);
  for (const std::string_view doc : {"readme", "license", "licence", "file_id.diz"})
    if (base.starts_with(doc)) return false;
  const size_t dot = base.find_last_of('.');
  if (dot == std::string::npos) return true;
  const std::string_view ext = std::string_view(base).substr(dot);
  for (const std::string_view doc :
       {".txt", ".md",   ".nfo", ".diz", ".htm",    ".html", ".pdf", ".cfg", ".ini",  ".url",
        ".jpg", ".jpeg", ".png", ".gif", ".sha256", ".md5",  ".sig", ".asc", ".json", ".xml"})
    if (ext == doc) return false;
  return true;
}

}  // namespace shooters_edge::client
