// A stock map's versions kept, chosen and remembered (map_versions.hpp).
//
// ge:summary A map's versions: kept hidden by checksum, chosen on the search path, remembered
// ge:keywords map-versions keep_version stored_versions choose_version keep_choice servers-txt
// ge:skill classic vfs
// ge:test tests/classic_content_map_versions_test.cpp
#include "classic/content/map_versions.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>
#include <system_error>

namespace ge::classic::content {

namespace fs = std::filesystem;

namespace {

constexpr std::string_view kFolder = "maps/versions";
constexpr std::string_view kChoices = "maps/versions/servers.txt";

// "maps/dm3.bsp": "dm3"; "" when it is not a map's path.
std::string map_name(std::string_view map) {
  if (!map.starts_with("maps/") || !map.ends_with(".bsp")) return {};
  std::string name(map.substr(5, map.size() - 9));
  if (name.empty() || name.find_first_of("/\\:.") != std::string::npos) return {};
  for (char& c : name) c = c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
  return name;
}

bool read_bytes(const fs::path& p, std::vector<uint8_t>& out) {
  std::ifstream in(p, std::ios::binary);
  if (!in) return false;
  out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return true;
}

}  // namespace

std::string version_path(std::string_view map, uint32_t key) {
  const std::string name = map_name(map);
  if (name.empty()) return {};
  static constexpr char kHex[] = "0123456789abcdef";
  std::string hex(8, '0');
  for (int i = 7; i >= 0; i--, key >>= 4) hex[static_cast<size_t>(i)] = kHex[key & 15u];
  return std::string(kFolder) + "/" + name + "." + hex + ".bsp";
}

std::vector<std::pair<std::string, std::vector<uint8_t>>> stored_versions(const fs::path& folder,
                                                                          std::string_view map) {
  std::vector<std::pair<std::string, std::vector<uint8_t>>> out;
  const std::string name = map_name(map);
  if (name.empty() || folder.empty()) return out;
  std::error_code ec;
  std::vector<std::string> found;
  for (fs::directory_iterator it(folder / kFolder, ec), end; !ec && it != end; it.increment(ec)) {
    const std::string leaf = it->path().filename().string();
    // "<name>.<8 hex>.bsp"
    if (leaf.size() != name.size() + 13 || !leaf.starts_with(name + ".") || !leaf.ends_with(".bsp"))
      continue;
    found.push_back(leaf);
  }
  std::sort(found.begin(), found.end());
  for (const std::string& leaf : found) {
    std::vector<uint8_t> bytes;
    if (read_bytes(folder / kFolder / leaf, bytes) && !bytes.empty())
      out.emplace_back(std::string(kFolder) + "/" + leaf, std::move(bytes));
  }
  return out;
}

std::string keep_version(const fs::path& folder, std::string_view map, uint32_t key,
                         const std::vector<uint8_t>& bytes, std::string& why) {
  const std::string path = version_path(map, key);
  if (path.empty() || folder.empty()) {
    why = "no game folder to keep another version of " + std::string(map) + " in";
    return {};
  }
  std::error_code ec;
  fs::create_directories(folder / kFolder, ec);
  std::ofstream out(folder / path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
  if (!out) {
    why = "could not write " + (folder / path).string();
    return {};
  }
  return path;
}

void choose_version(SearchPath& files, std::string_view map, std::string_view version) {
  if (!files.links) files.links = std::make_shared<FileLinks>();
  auto& v = files.links->versions;
  v.erase(std::remove_if(v.begin(), v.end(), [&](const auto& p) { return p.first == map; }),
          v.end());
  if (!version.empty() && version != map) v.emplace_back(std::string(map), std::string(version));
}

std::optional<uint32_t> remembered_key(const fs::path& folder, std::string_view server,
                                       std::string_view map) {
  std::ifstream in(folder / kChoices);
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream words(line);
    std::string s, m, k;
    if (!(words >> s >> m >> k) || s != server || m != map || k.size() != 8) continue;
    return static_cast<uint32_t>(std::stoul(k, nullptr, 16));
  }
  return std::nullopt;
}

void keep_choice(const fs::path& folder, std::string_view server, std::string_view map,
                 uint32_t key) {
  if (folder.empty() || server.empty() || map_name(map).empty()) return;
  std::vector<std::string> lines;
  {
    std::ifstream in(folder / kChoices);
    std::string line;
    while (std::getline(in, line)) {
      std::istringstream words(line);
      std::string s, m;
      if ((words >> s >> m) && s == server && m == map) continue;
      if (!line.empty()) lines.push_back(line);
    }
  }
  const std::string path = version_path(map, key);  // its key in hex, as the file names it
  lines.push_back(std::string(server) + " " + std::string(map) + " " +
                  path.substr(path.size() - 12, 8));
  std::error_code ec;
  fs::create_directories(folder / kFolder, ec);
  std::ofstream out(folder / kChoices, std::ios::trunc);
  for (const std::string& l : lines) out << l << '\n';
}

}  // namespace ge::classic::content
