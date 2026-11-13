// The classic installs around the game (installs.hpp).
//
// ge:summary The classic installs around the game: started inside one, placed above several
// ge:keywords installs install_around installs_below release_of pick_install placed_folder
// ge:skill shooter classicenhance
// ge:test tests/shooters_edge_install_search_test.cpp
#include "shooters_edge/client/install_search.hpp"

#include <algorithm>
#include <filesystem>
#include <system_error>

#include "shooters_edge/client/game_folder.hpp"

namespace shooters_edge::client {

namespace fs = std::filesystem;

namespace {

std::string lower(std::string s) {
  for (char& c : s) c = c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
  return s;
}

// The rank of a folder's name in kInstallNames, or the list's size when it is none of them.
size_t name_rank(const std::string& name) {
  const std::string l = lower(name);
  for (size_t i = 0; i < kInstallNames.size(); ++i)
    if (l == kInstallNames[i]) return i;
  return kInstallNames.size();
}

bool found_at(const std::string& folder, FoundInstall& out) {
  std::string basedir, why;
  const ge::classicenhance::Profile* content = install_of(folder, &basedir, &why);
  if (content == nullptr) return false;
  out = {};
  out.content = content;
  out.basedir = basedir;
  release_of(*content, basedir, out);
  return true;
}

}  // namespace

void release_of(const ge::classicenhance::Profile& content, const std::string& basedir,
                FoundInstall& out) {
  std::error_code ec;
  fs::path base;
  for (const fs::directory_entry& e : fs::directory_iterator(basedir, ec))
    if (lower(e.path().filename().string()) == lower(std::string(content.base_dir)))
      base = e.path();
  const KnownReleasePack* sample = nullptr;
  const KnownReleasePack* full = nullptr;
  if (!base.empty()) {
    for (const fs::directory_entry& e : fs::directory_iterator(base, ec)) {
      if (!e.is_regular_file(ec)) continue;
      const std::string name = lower(e.path().filename().string());
      const uint64_t size = e.file_size(ec);
      for (const KnownReleasePack& k : kKnownReleasePacks) {
        if (k.profile != content.id || name != k.pack || size != k.size) continue;
        (k.full ? full : sample) = &k;
      }
    }
  }
  if (full != nullptr) {
    out.release = std::string(full->release);
    out.full = true;
  } else if (sample != nullptr) {
    out.release = std::string(sample->release);
    out.full = false;
  } else {
    out.release = "your own copy";
    out.full = true;
  }
}

bool install_around(const std::string& folder, FoundInstall& out) {
  std::error_code ec;
  fs::path at = fs::absolute(fs::path(folder.empty() ? "." : folder), ec).lexically_normal();
  if (!at.has_filename()) at = at.parent_path();
  for (int up = 0; up <= kInstallDepth; ++up) {
    if (fs::is_directory(at, ec) && found_at(at.string(), out)) return true;
    if (!at.has_parent_path() || at.parent_path() == at) break;
    at = at.parent_path();
  }
  return false;
}

std::vector<FoundInstall> installs_below(const std::string& parent) {
  std::vector<FoundInstall> found;
  std::error_code ec;
  fs::path root = fs::absolute(fs::path(parent.empty() ? "." : parent), ec).lexically_normal();
  if (!root.has_filename()) root = root.parent_path();
  if (root == root.root_path() || !fs::is_directory(root, ec)) return found;
  std::vector<fs::path> folders;
  for (const fs::directory_entry& e : fs::directory_iterator(root, ec))
    if (e.is_directory(ec)) folders.push_back(e.path());
  std::sort(folders.begin(), folders.end(), [](const fs::path& a, const fs::path& b) {
    const size_t ra = name_rank(a.filename().string()), rb = name_rank(b.filename().string());
    if (ra != rb) return ra < rb;
    return a.filename().string() < b.filename().string();
  });
  for (const fs::path& f : folders) {
    FoundInstall one;
    std::string basedir, why;
    // Only a folder that is an install itself (its base folder in it), never its parent's.
    const ge::classicenhance::Profile* content = install_of(f.string(), &basedir, &why);
    if (content == nullptr || !fs::equivalent(basedir, f, ec)) continue;
    one.content = content;
    one.basedir = basedir;
    release_of(*content, basedir, one);
    found.push_back(std::move(one));
  }
  return found;
}

std::string placed_folder(const std::string& exe_dir) {
  if (exe_dir.empty()) return {};
  fs::path at = fs::path(exe_dir).lexically_normal();
  if (!at.has_filename()) at = at.parent_path();
  for (fs::path p = at; p.has_filename(); p = p.parent_path()) {
    if (lower(p.extension().string()) == ".app") return p.parent_path().string();
    if (p.parent_path() == p) break;
  }
  return at.string();
}

int pick_install(const std::vector<FoundInstall>& found, const std::string& kept) {
  std::error_code ec;
  if (!kept.empty())
    for (size_t i = 0; i < found.size(); ++i)
      if (fs::equivalent(found[i].basedir, kept, ec)) return static_cast<int>(i);
  for (size_t i = 0; i < found.size(); ++i)
    if (found[i].full) return static_cast<int>(i);
  return found.empty() ? -1 : 0;
}

std::string install_line(const FoundInstall& f, int n) {
  std::string line = std::to_string(n) + "  ";
  line += f.content != nullptr ? std::string(f.content->what) : std::string("a classic game");
  line += " (" + f.release + ")  " + f.basedir;
  return line;
}

}  // namespace shooters_edge::client
