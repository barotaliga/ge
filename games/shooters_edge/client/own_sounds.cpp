// The game's own sounds under the classic names (own_sounds.hpp).
//
// ge:summary The asset map's sound rows read; our WAVs packed in memory under the classic names
// ge:keywords own-sounds assetmap sound-map mount
// ge:skill audio shooter
#include "shooters_edge/client/own_sounds.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

#include "vfs/pak.hpp"

namespace shooters_edge::client {

namespace {

bool read_whole(const std::filesystem::path& p, std::vector<uint8_t>& out) {
  std::ifstream in(p, std::ios::binary);
  if (!in) return false;
  out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return true;
}

}  // namespace

bool read_sound_map(std::string_view text, std::vector<std::pair<std::string, std::string>>& out,
                    std::string& error) {
  out.clear();
  std::istringstream lines{std::string(text)};
  std::string line;
  int number = 0;
  while (std::getline(lines, line)) {
    number++;
    // A comment: '#' at the line's start or after whitespace.
    for (size_t i = 0; i < line.size(); i++) {
      if (line[i] == '#' && (i == 0 || line[i - 1] == ' ' || line[i - 1] == '\t')) {
        line.resize(i);
        break;
      }
    }
    std::istringstream words(line);
    std::string kind, classic, ours, more;
    if (!(words >> kind) || kind != "sound") continue;
    if (!(words >> classic >> ours) || (words >> more)) {
      error =
          "classic.assetmap line " + std::to_string(number) +
          ": a sound row is `sound <classic path> <our name>`; fix the line (free-assets skill)";
      return false;
    }
    out.emplace_back(std::move(classic), std::move(ours));
  }
  return true;
}

bool mount_own_sounds(SearchPath& files, const std::string& content, OwnSounds& out,
                      std::string& error) {
  out = {};
  const std::filesystem::path dir(content);
  std::vector<uint8_t> text;
  if (!read_whole(dir / "classic.assetmap", text)) {
    error = "no classic.assetmap in " + content + "; run from the tree's root or pass its folder";
    return false;
  }
  std::vector<std::pair<std::string, std::string>> rows;
  if (!read_sound_map({reinterpret_cast<const char*>(text.data()), text.size()}, rows, error))
    return false;
  out.mapped = static_cast<int>(rows.size());
  std::vector<std::pair<std::string, std::vector<uint8_t>>> entries;
  for (const auto& [classic, ours] : rows) {
    std::vector<uint8_t> wav;
    if (!read_whole(dir / "sounds" / (ours + ".wav"), wav)) continue;
    entries.emplace_back("sound/" + classic, std::move(wav));
  }
  out.mounted = static_cast<int>(entries.size());
  if (entries.empty()) return true;
  std::vector<uint8_t> pak;
  if (!ge::vfs::write_pak(entries, &pak, &error)) return false;
  return add_archive(files, "own-sounds.pak", std::move(pak), error);
}

std::string find_content_dir(const std::string& built_in) {
  std::error_code ec;
  for (const std::string& d : {std::string("games/shooters_edge/content"), built_in}) {
    if (!d.empty() && std::filesystem::exists(std::filesystem::path(d) / "classic.assetmap", ec))
      return d;
  }
  return "";
}

}  // namespace shooters_edge::client
