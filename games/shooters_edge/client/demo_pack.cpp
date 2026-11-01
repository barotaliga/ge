// The packed formats' names and the archive pick (demo_pack.hpp).
//
// ge:summary The packed extensions and the document names an archive's pick leaves out
// ge:keywords packed_extension demo_name document readme nfo diz
// ge:skill shooter
// ge:test tests/shooters_edge_demo_test.cpp
#include "shooters_edge/client/demo_pack.hpp"

namespace shooters_edge::client {

std::string packed_extension(DemoFormat base, bool voice) {
  if (base == DemoFormat::single) return ".demz";
  if (voice) return voice_extension(base) + "z";
  return base == DemoFormat::multiview ? ".mvdz" : ".qwdz";
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
