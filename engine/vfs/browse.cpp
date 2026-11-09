// The folder browser (browse.hpp): the names shown, the listing, the places, the presses.
//
// ge:summary The folder browser's listing, sort, places, ages and sizes, and its presses
// ge:keywords browse list_folder named_as user_places folder-browser directory-iterator sort
// ge:keywords age_text size_text home downloads drives
// ge:skill vfs
// ge:test tests/vfs_browse_test.cpp
#include "vfs/browse.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <system_error>

namespace ge::vfs {

namespace {

namespace fs = std::filesystem;

char fold(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

std::string folded(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = fold(c);
  return out;
}

bool ends_with(std::string_view s, std::string_view tail) {
  return s.size() >= tail.size() && s.substr(s.size() - tail.size()) == tail;
}

// Names compared as a player reads a list: case folded, then as written.
bool name_less(const std::string& a, const std::string& b) {
  const std::string fa = folded(a), fb = folded(b);
  return fa != fb ? fa < fb : a < b;
}

std::string home_dir() {
#ifdef _WIN32
  const char* home = std::getenv("USERPROFILE");  // NOLINT(concurrency-mt-unsafe): read only
#else
  const char* home = std::getenv("HOME");  // NOLINT(concurrency-mt-unsafe): read only
#endif
  return home == nullptr ? std::string() : std::string(home);
}

bool is_dir(const fs::path& p) {
  std::error_code ec;
  return fs::is_directory(p, ec);
}

}  // namespace

bool wrapper_extension(std::string_view ext) {
  const std::string e = folded(ext);
  for (const std::string_view w :
       {".gz", ".zst", ".zstd", ".xz", ".tar", ".tgz", ".txz", ".zip", ".7z"})
    if (e == w) return true;
  return false;
}

bool named_as(std::string_view name, std::span<const std::string> extensions) {
  std::string n = folded(name);
  for (int peeled = 0; peeled <= 4; peeled++) {
    for (const std::string& ext : extensions) {
      const std::string e = folded(ext);
      if (n.size() > e.size() && ends_with(n, e)) return true;
    }
    const size_t dot = n.find_last_of('.');
    if (dot == std::string::npos || dot == 0) return false;
    const std::string_view last = std::string_view(n).substr(dot);
    if (!wrapper_extension(last)) return false;
    // `.tgz` and `.txz` are a tar packed: what is under them is a tar's name, which says nothing.
    if (last == ".tgz" || last == ".txz") return false;
    n.resize(dot);
    if (ends_with(n, ".tar")) n.resize(n.size() - 4);  // a tar holds the file by its own name
  }
  return false;
}

bool list_folder(const std::string& dir, std::span<const std::string> extensions,
                 std::vector<FolderEntry>& out, std::string& error) {
  out.clear();
  std::error_code ec;
  const fs::path root(dir);
  fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
  if (ec) {
    error = "the folder " + dir + " did not open: " + ec.message() +
            "; next: pick another place, or allow the game to read it";
    return false;
  }
  std::vector<FolderEntry> folders, files;
  const fs::file_time_type now = fs::file_time_type::clock::now();
  for (; it != fs::directory_iterator(); it.increment(ec)) {
    if (ec) break;
    const fs::directory_entry& e = *it;
    const std::string name = e.path().filename().string();
    if (name.empty() || name[0] == '.') continue;
    std::error_code ec2;
    if (e.is_directory(ec2)) {
      folders.push_back({.name = name, .path = e.path().string(), .folder = true});
      continue;
    }
    if (!e.is_regular_file(ec2) || !named_as(name, extensions)) continue;
    FolderEntry f{.name = name, .path = e.path().string()};
    f.bytes = e.file_size(ec2);
    if (ec2) f.bytes = 0;
    const fs::file_time_type t = e.last_write_time(ec2);
    if (!ec2)
      f.age_s =
          std::max<int64_t>(0, std::chrono::duration_cast<std::chrono::seconds>(now - t).count());
    files.push_back(std::move(f));
  }
  std::sort(folders.begin(), folders.end(),
            [](const FolderEntry& a, const FolderEntry& b) { return name_less(a.name, b.name); });
  std::sort(files.begin(), files.end(), [](const FolderEntry& a, const FolderEntry& b) {
    return a.age_s != b.age_s ? a.age_s < b.age_s : name_less(a.name, b.name);
  });
  const std::string up = parent_folder(dir);
  if (up != dir) out.push_back({.name = "..", .path = up, .folder = true});
  out.insert(out.end(), folders.begin(), folders.end());
  out.insert(out.end(), files.begin(), files.end());
  return true;
}

std::vector<Place> user_places() {
  std::vector<Place> out;
  const std::string home = home_dir();
  if (!home.empty() && is_dir(home)) {
    out.push_back({"HOME", home});
    for (const auto& [name, sub] :
         {std::pair{"DOWNLOADS", "Downloads"}, std::pair{"DESKTOP", "Desktop"},
          std::pair{"DOCUMENTS", "Documents"}}) {
      const fs::path p = fs::path(home) / sub;
      if (is_dir(p)) out.push_back({name, p.string()});
    }
  }
#ifdef _WIN32
  for (char d = 'A'; d <= 'Z'; d++) {
    const std::string root = std::string(1, d) + ":\\";
    if (is_dir(root)) out.push_back({std::string(1, d) + ":", root});
  }
#else
  out.push_back({"COMPUTER", "/"});
#endif
  return out;
}

std::string parent_folder(const std::string& dir) {
  fs::path p(dir);
  if (p.has_relative_path() && !p.has_filename()) p = p.parent_path();  // a trailing slash
  if (!p.has_relative_path()) return dir;                               // a root
  return p.parent_path().string();
}

std::string age_text(int64_t age_s) {
  if (age_s < 60) return "JUST NOW";
  if (age_s < 3600) return std::to_string(age_s / 60) + " MIN AGO";
  if (age_s < 86400) return std::to_string(age_s / 3600) + " H AGO";
  const int64_t days = age_s / 86400;
  if (days < 60) return std::to_string(days) + (days == 1 ? " DAY AGO" : " DAYS AGO");
  if (days < 730) return std::to_string(days / 30) + " MONTHS AGO";
  return std::to_string(days / 365) + " YEARS AGO";
}

std::string size_text(uint64_t bytes) {
  if (bytes < 1024) return std::to_string(bytes) + " B";
  static constexpr const char* kUnits[] = {"KB", "MB", "GB", "TB"};
  double v = static_cast<double>(bytes) / 1024;
  int u = 0;
  while (v >= 1024 && u < 3) {
    v /= 1024;
    u++;
  }
  char buf[32];
  std::snprintf(buf, sizeof buf, v < 100 ? "%.1f %s" : "%.0f %s", v, kUnits[u]);
  return buf;
}

bool FolderBrowser::open(const std::string& dir, std::string_view select) {
  std::vector<FolderEntry> list;
  std::string error;
  if (!list_folder(dir, extensions_, list, error)) {
    note_ = error;
    return false;
  }
  dir_ = dir;
  entries_ = std::move(list);
  note_.clear();
  selected_ = 0;
  if (!select.empty()) {
    select_name(select);
  } else {
    for (size_t i = 0; i < entries_.size(); i++)
      if (!entries_[i].folder) {
        selected_ = static_cast<int>(i);
        break;
      }
  }
  return true;
}

void FolderBrowser::rescan() {
  const FolderEntry* s = selection();
  const std::string name = s != nullptr ? s->name : std::string();
  std::string note = note_;
  if (open(dir_, name)) note_ = std::move(note);
}

void FolderBrowser::select_name(std::string_view name) {
  for (size_t i = 0; i < entries_.size(); i++)
    if (entries_[i].name == name) {
      selected_ = static_cast<int>(i);
      return;
    }
}

const FolderEntry* FolderBrowser::selection() const {
  if (selected_ < 0 || selected_ >= static_cast<int>(entries_.size())) return nullptr;
  return &entries_[static_cast<size_t>(selected_)];
}

int FolderBrowser::place() const {
  int best = -1;
  size_t best_len = 0;
  const std::string here = folded(fs::path(dir_).generic_string());
  for (size_t i = 0; i < places_.size(); i++) {
    std::string p = folded(fs::path(places_[i].path).generic_string());
    while (p.size() > 1 && p.back() == '/') p.pop_back();
    const bool inside =
        here == p || (here.starts_with(p) && (p.back() == '/' || here[p.size()] == '/'));
    if (inside && p.size() >= best_len) {
      best = static_cast<int>(i);
      best_len = p.size();
    }
  }
  return best;
}

int FolderBrowser::files() const {
  int n = 0;
  for (const FolderEntry& e : entries_) n += e.folder ? 0 : 1;
  return n;
}

BrowseAction FolderBrowser::input(const BrowseInput& in) {
  if (in.back) return BrowseAction::leave;
  const int n = static_cast<int>(entries_.size());
  const int was = selected_;
  if (in.up) selected_--;
  if (in.down) selected_++;
  if (in.page_up) selected_ -= page_rows_;
  if (in.page_down) selected_ += page_rows_;
  if (in.home) selected_ = 0;
  if (in.end) selected_ = n - 1;
  selected_ = std::clamp(selected_, 0, std::max(0, n - 1));
  if ((in.left || in.right) && !places_.empty()) {
    const int count = static_cast<int>(places_.size());
    const int at = place();
    const int next =
        at < 0 ? (in.right ? 0 : count - 1) : (at + (in.right ? 1 : count - 1)) % count;
    for (int k = 0; k < count; k++) {  // a place that no longer opens is passed over
      const int i = (next + (in.right ? k : count - k)) % count;
      if (open(places_[static_cast<size_t>(i)].path)) return BrowseAction::opened;
    }
    return BrowseAction::none;
  }
  if (in.parent) {
    const std::string up = parent_folder(dir_);
    if (up == dir_) return BrowseAction::none;
    const std::string from = fs::path(dir_).filename().string();
    return open(up, from) ? BrowseAction::opened : BrowseAction::none;
  }
  bool go = in.accept;
  if (in.pointed >= 0 && in.pointed < n) {
    selected_ = in.pointed;
    go = go || in.click;
  }
  if (go && selected_ < n) {
    const FolderEntry e = entries_[static_cast<size_t>(selected_)];
    if (e.folder) {
      const std::string from = e.name == ".." ? fs::path(dir_).filename().string() : "";
      return open(e.path, from) ? BrowseAction::opened : BrowseAction::none;
    }
    chosen_ = e.path;
    return BrowseAction::chosen;
  }
  return selected_ != was ? BrowseAction::moved : BrowseAction::none;
}

}  // namespace ge::vfs
