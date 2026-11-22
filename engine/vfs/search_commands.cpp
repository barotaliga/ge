// The search path's console words (search_commands.hpp).
//
// ge:summary path, dir, fdir and whereis over a search path
// ge:keywords add_search_commands glob_match whereis fdir dir path
// ge:skill vfs console
// ge:test tests/vfs_search_commands_test.cpp
#include "vfs/search_commands.hpp"

#include <cctype>
#include <string>
#include <vector>

namespace ge::vfs {

namespace {

char fold(char c) {
  return c == '\\' ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

std::string folded(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = fold(c);
  return out;
}

// What a source is called on the console: its file's or folder's name as mounted.
std::string source_name(const Source& s) { return s.path; }

// Whether source `s` holds `name` (folded).
bool holds(const Source& s, const std::string& name) {
  switch (s.kind) {
    case SourceKind::directory:
      for (const std::string& f : s.files)
        if (folded(f) == name) return true;
      return false;
    case SourceKind::pack:
      for (const PakEntry& e : s.pak.entries)
        if (folded(e.path) == name) return true;
      return false;
    case SourceKind::zip:
      return zip_find(s.zip, name) != nullptr;
  }
  return false;
}

size_t file_count(const Source& s) {
  switch (s.kind) {
    case SourceKind::directory:
      return s.files.size();
    case SourceKind::pack:
      return s.pak.entries.size();
    case SourceKind::zip:
      return s.zip.entries.size();
  }
  return 0;
}

std::string listed(size_t n) {
  return std::to_string(n) + (n == 1 ? " file listed." : " files listed.");
}

}  // namespace

bool glob_match(std::string_view pattern, std::string_view name) {
  size_t p = 0, n = 0, star = std::string_view::npos, mark = 0;
  while (n < name.size()) {
    if (p < pattern.size() && (pattern[p] == '?' || fold(pattern[p]) == fold(name[n]))) {
      p++;
      n++;
    } else if (p < pattern.size() && pattern[p] == '*') {
      star = p++;
      mark = n;
    } else if (star != std::string_view::npos) {
      p = star + 1;
      n = ++mark;
    } else {
      return false;
    }
  }
  while (p < pattern.size() && pattern[p] == '*') p++;
  return p == pattern.size();
}

void add_search_commands(console::Console& con, const SearchPath& sp) {
  using Con = console::Console;
  using Args = console::Args;
  con.add_command("path", "path: where the game's files are read from, the first winning",
                  [&sp](Con& c, const Args&) {
                    c.print("Current search path:");
                    for (size_t i = sp.sources.size(); i-- > 0;) {
                      const Source& s = sp.sources[i];
                      c.print(source_name(s) + " (" + std::to_string(file_count(s)) + " files)");
                    }
                  });
  con.add_command(
      "dir", "dir <folder> [ext]: the files in a folder, only those of a kind when one is given",
      [&sp](Con& c, const Args& a) {
        if (a.size() < 2) {
          c.print("Usage: dir <folder> [ext]");
          return;
        }
        std::string folder = folded(a[1]);
        while (!folder.empty() && folder.back() == '/') folder.pop_back();
        if (!folder.empty()) folder += '/';
        std::string ext = a.size() >= 3 ? folded(a[2]) : std::string();
        if (!ext.empty() && ext[0] != '.') ext.insert(ext.begin(), '.');
        size_t n = 0;
        for (const Listed& f : list(sp)) {
          if (f.name.substr(0, folder.size()) != folder) continue;
          if (!ext.empty() &&
              (f.name.size() < ext.size() || f.name.substr(f.name.size() - ext.size()) != ext))
            continue;
          c.print(std::string(f.name.substr(folder.size())));
          n++;
        }
        c.print(listed(n));
      });
  con.add_command("fdir", "fdir <pattern>: every file whose name fits, * and ? as wildcards",
                  [&sp](Con& c, const Args& a) {
                    if (a.size() < 2) {
                      c.print("Usage: fdir <pattern>");
                      return;
                    }
                    size_t n = 0;
                    for (const Listed& f : list(sp))
                      if (glob_match(a[1], f.name)) {
                        c.print(std::string(f.name));
                        n++;
                      }
                    c.print(listed(n));
                  });
  con.add_command("whereis", "whereis <file> [all]: which archive or folder a file is read from",
                  [&sp](Con& c, const Args& a) {
                    if (a.size() < 2) {
                      c.print("Usage: whereis <file> [all]");
                      return;
                    }
                    const std::string name = folded(a[1]);
                    const FileRef* won = find(sp, name);
                    if (won == nullptr) {
                      c.print(a[1] + " was not found.");
                      return;
                    }
                    c.print(a[1] + " is in " + source_name(sp.sources[won->source]));
                    if (a.size() < 3 || a[2] != "all") return;
                    for (size_t i = sp.sources.size(); i-- > 0;)
                      if (i != won->source && holds(sp.sources[i], name))
                        c.print("  and under it in " + source_name(sp.sources[i]));
                  });
}

}  // namespace ge::vfs
