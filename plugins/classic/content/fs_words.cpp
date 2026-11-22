// The definitions behind fs_words.hpp.
//
// ge:summary The classic file words' definitions: listings, the lookup, the links
// ge:keywords dir fdir whereis link unlink fs_restart
// ge:skill classic vfs
// ge:test tests/classic_content_fs_words_test.cpp
#include "classic/content/fs_words.hpp"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <system_error>

namespace ge::classic::content {

namespace fs = std::filesystem;

namespace {

using Links = std::vector<std::pair<std::string, std::string>>;

constexpr size_t kLongestPath = 255;  // a game path this long or longer is refused
constexpr size_t kPackPath = 64;      // the archives keep names shorter than this
constexpr int kDeepest = 8;           // how deep `fdir` walks a folder

char lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

std::string lowered(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = lower(c);
  return out;
}

bool same(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); i++)
    if (lower(a[i]) != lower(b[i])) return false;
  return true;
}

bool ends_with(std::string_view name, std::string_view end) {
  return end.size() <= name.size() && same(name.substr(name.size() - end.size()), end);
}

std::vector<std::string> split(const std::string& text, char by) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at <= text.size()) {
    const size_t end = std::min(text.find(by, at), text.size());
    if (end > at) out.push_back(text.substr(at, end - at));
    at = end + 1;
  }
  return out;
}

const std::pair<std::string, std::string>* link_of(const Links& links, std::string_view path) {
  for (const auto& l : links)
    if (!l.first.empty() && l.first.size() <= path.size() &&
        same(path.substr(0, l.first.size()), l.first))
      return &l;
  return nullptr;
}

std::string rewritten(const std::pair<std::string, std::string>& l, std::string_view path) {
  return l.second + std::string(path.substr(l.first.size()));
}

// The files of a folder on disk under `rel` (relative to the game's root), `depth` folders below
// it at most, each by its path from the root; names starting with `.` skipped.
void walk(const fs::path& root, const std::string& rel, int depth, std::vector<std::string>& out) {
  std::error_code ec;
  fs::directory_iterator it(rel.empty() ? root : root / rel, ec);
  for (; !ec && it != fs::directory_iterator(); it.increment(ec)) {
    const std::string name = it->path().filename().string();
    if (name.empty() || name[0] == '.') continue;
    std::string at = rel;
    if (!at.empty()) at += '/';
    at += name;
    std::error_code kind;
    if (it->is_regular_file(kind))
      out.push_back(at);
    else if (depth > 0 && it->is_directory(kind))
      walk(root, at, depth - 1, out);
  }
}

// Every file the search path holds under `folder` ("" the root), the one searched first at the
// top: `deep` every one below it, else those directly in it.
std::vector<std::string> every_file(const SearchPath& path, const std::string& folder, bool deep) {
  std::vector<std::string> out;
  const std::string head = folder.empty() ? "" : folder + "/";
  const auto take = [&](std::string name) {
    std::replace(name.begin(), name.end(), '\\', '/');
    if (name.empty() || name.back() == '/') return;  // a zip's folder entry
    if (!head.empty() && (name.size() <= head.size() || !same(name.substr(0, head.size()), head)))
      return;
    if (!deep && name.find('/', head.size()) != std::string::npos) return;
    out.push_back(std::move(name));
  };
  for (auto src = path.vfs.sources.rbegin(); src != path.vfs.sources.rend(); ++src) {
    if (src->kind == ge::vfs::SourceKind::pack)
      for (const ge::vfs::PakEntry& e : src->pak.entries) take(e.path);
    else if (src->kind == ge::vfs::SourceKind::zip)
      for (const ge::vfs::ZipEntry& e : src->zip.entries) take(e.name);
    else {
      std::vector<std::string> found;
      walk(src->path, folder, deep ? kDeepest : 0, found);
      for (std::string& f : found) take(std::move(f));
    }
  }
  return out;
}

// The listing's lines: sorted ignoring case, each name once, then the count.
std::string listed(std::vector<std::string> names) {
  std::stable_sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) {
    return lowered(a) < lowered(b);
  });
  names.erase(std::unique(names.begin(), names.end(),
                          [](const std::string& a, const std::string& b) { return same(a, b); }),
              names.end());
  std::string out;
  for (const std::string& n : names) out += n + "\n";
  return out + std::to_string(names.size()) + " files listed\n";
}

std::string dir_words(const SearchPath& path, const std::vector<std::string>& w) {
  if (w.size() < 2) return "Usage: dir <directory> [.extension]\n";
  const std::string folder = normal_game_path(w[1]);
  const std::vector<std::string> ends =
      w.size() > 2 ? split(w[2], ';') : std::vector<std::string>{};
  std::vector<std::string> names;
  const size_t cut = folder.empty() ? 0 : folder.size() + 1;
  for (const std::string& f : every_file(path, folder, false)) {
    const std::string name = f.substr(cut);
    bool keep = ends.empty();
    for (const std::string& e : ends) keep = keep || ends_with(name, e);
    if (keep) names.push_back(name);
  }
  return listed(std::move(names));
}

std::string fdir_words(const SearchPath& path, const std::vector<std::string>& w) {
  if (w.size() < 2) return "Usage: fdir <filter>\n";
  const std::vector<std::string> filters = split(w[1], ';');
  std::vector<std::string> names;
  for (const std::string& f : every_file(path, "", true))
    for (const std::string& filter : filters)
      if (filter_match(filter, f)) {
        names.push_back(f);
        break;
      }
  return listed(std::move(names));
}

bool printable(std::string_view p) {
  for (const char c : p)
    if (c < 32 || c > 126) return false;
  return true;
}

bool has_upper(std::string_view p) {
  for (const char c : p)
    if (c >= 'A' && c <= 'Z') return true;
  return false;
}

// One look for `p` in every source, the one searched first at the top: the lines in `out`, the
// count returned; the first find ends it unless `all`.
int look(const SearchPath& path, const std::string& p, bool all, std::string& out) {
  int found = 0;
  const bool packs = p.size() < kPackPath;
  if (!packs)
    out +=
        "Not searching for '" + p + "' in pack files since path length exceedes 63 characters.\n";
  bool folders = true;
  for (auto src = path.vfs.sources.rbegin(); src != path.vfs.sources.rend(); ++src) {
    if (src->kind != ge::vfs::SourceKind::directory) {
      if (!packs) continue;
      uint64_t size = 0;
      bool hit = false;
      if (src->kind == ge::vfs::SourceKind::pack) {
        for (const ge::vfs::PakEntry& e : src->pak.entries)
          if (same(e.path, p)) {
            size = e.size;
            hit = true;
            break;
          }
      } else if (const ge::vfs::ZipEntry* e = ge::vfs::zip_find(src->zip, p)) {
        size = e->size;
        hit = true;
      }
      if (!hit) continue;
      out += src->path + "/" + p + " (" + std::to_string(size) + " bytes)\n";
      if (++found > 0 && !all) return found;
      continue;
    }
    if (!folders) continue;
    if (!printable(p)) {
      out += "Not searching for '" + p +
             "' in physical file system since path contains invalid characters.\n";
      folders = false;
      continue;
    }
    std::string name = p;
    std::string full = src->path + "/" + name;
    if (full.size() > kLongestPath) {
      out += "Full path length '" + full + "' exceeded 255 characters.\n";
      continue;
    }
    std::error_code ec;
    bool hit = fs::is_regular_file(full, ec);
    if (!hit && has_upper(name)) {
      name = lowered(name);
      full = src->path + "/" + name;
      hit = fs::is_regular_file(full, ec);
      if (hit) out += "Physical path found after converting to lower case.\n";
    }
    if (!hit) continue;
    const uintmax_t size = fs::file_size(full, ec);
    if (ec) {
      out += "Couldn't get info on '" + full + "': " + ec.message() + "\n";
      if (!all) return found;
      continue;
    }
    out += full + " (" + std::to_string(size) + " bytes)\n";
    if (++found > 0 && !all) return found;
  }
  return found;
}

std::string whereis_words(const SearchPath& path, const std::vector<std::string>& w) {
  if (w.size() < 2) return "Usage: whereis <path> [all]\n";
  const bool all = w.size() > 2;
  std::string p = normal_game_path(w[1]);
  if (p.size() >= kLongestPath) return "Refusing to lookup oversize path.\n";
  if (p.empty()) return "Refusing to lookup empty path.\n";
  std::string out;
  const FileLinks& links = *path.links;
  if (const auto* l = link_of(links.hard, p)) {
    out += "Symbolic link ('" + l->first + "' --> '" + l->second + "') in effect.\n";
    const std::string to = rewritten(*l, p);
    if (to.size() >= kLongestPath)
      return out + "Oversize symbolic link ('" + l->first + " --> '" + l->second + "').\n";
    p = to;
  }
  int found = look(path, p, all, out);
  if (found > 0 && !all) return out;
  if (const auto* l = link_of(links.soft, p)) {
    out += "Symbolic link ('" + l->first + "' --> '" + l->second + "') in effect.\n";
    const std::string to = rewritten(*l, p);
    if (to.size() < kLongestPath) found += look(path, to, all, out);
    if (found > 0 && !all) return out;
  }
  if (found > 0) return out + std::to_string(found) + " instances of " + p + "\n";
  return out + p + " was not found\n";
}

std::string link_words(Links& links, const std::vector<std::string>& w) {
  if (w.size() == 1) {
    std::string out;
    for (const auto& [name, target] : links) {
      out += name;
      out += " --> ";
      out += target;
      out += '\n';
    }
    const size_t n = links.size();
    return out + "------------------\n" + std::to_string(n) + " symbolic link" +
           (n == 1 ? "" : "s") + " listed.\n";
  }
  if (w.size() != 3)
    return "Usage: " + w[0] +
           " <name> <target>\nCreates symbolic link to target with the specified name.\n"
           "Virtual quake paths are accepted.\nLinks are effective only for reading.\n";
  const std::string name = normal_game_path(w[1]), target = normal_game_path(w[2]);
  if (name.empty() || name.size() >= kLongestPath) return "Invalid symbolic link name.\n";
  if (target.empty() || target.size() >= kLongestPath) return "Invalid symbolic link target.\n";
  for (auto& l : links)
    if (same(l.first, name)) {
      l.second = target;
      return "";
    }
  links.emplace_back(name, target);
  return "";
}

std::string unlink_words(Links& links, const std::vector<std::string>& w) {
  const std::string tryline = "Try '" + w[0] + " --help' for more information.\n";
  size_t i = 1;
  for (; i < w.size() && !w[i].empty() && w[i][0] == '-'; i++) {
    if (w[i] == "-a" || w[i] == "--all") {
      links.clear();
      return "Deleted all symbolic links.\n";
    }
    if (w[i] == "-h" || w[i] == "--help")
      return "Usage: " + w[0] +
             " [-ah] <name>\nDeletes a symbolic link with the specified name.\n"
             "Available options:\n-a | --all  | delete all links\n"
             "-h | --help | display this message\n\n";
    return "Unknown option: " + w[i] + ".\n" + tryline;
  }
  if (i >= w.size()) return "Missing name argument.\n" + tryline;
  for (auto it = links.begin(); it != links.end(); ++it)
    if (same(it->first, w[i])) {
      links.erase(it);
      return "";
    }
  return "Symbolic link '" + w[i] + "' does not exist.\n";
}

}  // namespace

std::string normal_game_path(std::string_view path) {
  std::string text(path);
  std::replace(text.begin(), text.end(), '\\', '/');
  std::vector<std::string> parts;
  for (const std::string& part : split(text, '/')) {
    if (part == ".") continue;
    if (part == "..") {
      if (!parts.empty()) parts.pop_back();
      continue;
    }
    parts.push_back(part);
  }
  std::string out;
  for (const std::string& part : parts) out += (out.empty() ? "" : "/") + part;
  return out;
}

std::string linked(const Links& links, std::string_view path) {
  const auto* l = link_of(links, path);
  return l == nullptr ? std::string() : rewritten(*l, path);
}

bool filter_match(std::string_view filter, std::string_view name) {
  size_t f = 0, n = 0, star = std::string_view::npos, from = 0;
  while (n < name.size()) {
    if (f < filter.size() && filter[f] == '*') {
      star = ++f;
      from = n;
      continue;
    }
    if (f < filter.size()) {
      const bool escaped = filter[f] == '\\' && f + 1 < filter.size();
      const char c = escaped ? filter[f + 1] : filter[f];
      if ((!escaped && c == '?') || lower(c) == lower(name[n])) {
        f += escaped ? 2 : 1;
        n++;
        continue;
      }
    }
    if (star == std::string_view::npos) return false;
    f = star;
    n = ++from;
  }
  while (f < filter.size() && filter[f] == '*') f++;
  return f == filter.size();
}

bool fs_command(const SearchPath& path, const std::vector<std::string>& w, std::string& out) {
  if (w.empty()) return false;
  const std::string& cmd = w[0];
  if (cmd == "dir") {
    out = dir_words(path, w);
  } else if (cmd == "fdir") {
    out = fdir_words(path, w);
  } else if (cmd == "whereis") {
    out = whereis_words(path, w);
  } else if (cmd == "link" || cmd == "softlink" || cmd == "unlink" || cmd == "softunlink") {
    if (!path.links) return false;
    Links& links = cmd.starts_with("soft") ? path.links->soft : path.links->hard;
    out = cmd.ends_with("unlink") ? unlink_words(links, w) : link_words(links, w);
  } else {
    return false;
  }
  return true;
}

std::string fs_restart_text(const SearchPath& path) {
  return "----- FS_Restart -----\n" + search_path_text(path, true) + "----------------------\n";
}

bool game_folder_check(std::string& game, std::string_view base, std::string& said) {
  if (game.empty()) return true;
  if (same(game, base)) {
    game.clear();
    return false;
  }
  for (const char c : game)
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
          c == '-')) {
      said += "'game' should contain characters [A-Za-z0-9_-] only.\n";
      game.clear();
      return false;
    }
  return true;
}

}  // namespace ge::classic::content
