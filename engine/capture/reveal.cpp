// A file shown in the system's file manager: each desktop's commands (reveal.hpp).
//
// ge:summary The reveal steps for macOS, Windows and freedesktop desktops, and file URIs
// ge:keywords reveal reveal_steps file_uri dbus-send xdg-open explorer-select open-r
// ge:skill replay platform
// ge:test tests/capture_reveal_test.cpp
#include "capture/reveal.hpp"

#include <string>
#include <vector>

namespace ge::capture {

Desktop this_desktop() {
#if defined(__APPLE__)
  return Desktop::mac;
#elif defined(_WIN32)
  return Desktop::windows;
#else
  return Desktop::freedesktop;
#endif
}

std::string_view file_manager_name(Desktop d) {
  switch (d) {
    case Desktop::mac:
      return "FINDER";
    case Desktop::windows:
      return "EXPLORER";
    case Desktop::freedesktop:
      break;
  }
  return "FILES";
}

std::string file_uri(std::string_view path) {
  static constexpr char kHex[] = "0123456789ABCDEF";
  std::string out = "file://";
  if (path.empty() || path.front() != '/') out += '/';  // a Windows path: file:///C:/...
  for (const char ch : path) {
    const auto c = static_cast<unsigned char>(ch);
    const bool keep = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                      c == '-' || c == '.' || c == '_' || c == '~' || c == '/';
    if (keep) {
      out += ch;
    } else if (c == '\\') {
      out += '/';
    } else {
      out += '%';
      out += kHex[c >> 4];
      out += kHex[c & 15];
    }
  }
  return out;
}

namespace {

// The folder a path is in: everything before its last separator ("/" for a file at the root).
std::string folder_of(std::string_view path) {
  const size_t cut = path.find_last_of("/\\");
  if (cut == std::string_view::npos) return std::string(path);
  if (cut == 0) return std::string(path.substr(0, 1));
  return std::string(path.substr(0, cut));
}

std::string windows_path(std::string_view path) {
  std::string out(path);
  for (char& c : out)
    if (c == '/') c = '\\';
  return out;
}

}  // namespace

std::vector<RevealStep> reveal_steps(Desktop d, std::string_view path, bool folder) {
  std::vector<RevealStep> steps;
  if (path.empty()) return steps;
  const std::string p(path);
  switch (d) {
    case Desktop::mac:
      steps.push_back(folder ? RevealStep{{"open", p}, "", false}
                             : RevealStep{{"open", "-R", p}, "", false});
      break;
    case Desktop::windows: {
      const std::string w = windows_path(path);
      RevealStep s;
      s.argv = {"explorer.exe"};
      s.line = folder ? "explorer.exe \"" + w + "\"" : "explorer.exe /select,\"" + w + "\"";
      steps.push_back(std::move(s));
      break;
    }
    case Desktop::freedesktop: {
      RevealStep bus;
      bus.argv = {"dbus-send",
                  "--session",
                  "--print-reply",
                  "--reply-timeout=2000",
                  "--dest=org.freedesktop.FileManager1",
                  "/org/freedesktop/FileManager1",
                  folder ? "org.freedesktop.FileManager1.ShowFolders"
                         : "org.freedesktop.FileManager1.ShowItems",
                  "array:string:" + file_uri(path),
                  "string:"};
      bus.judged = true;
      steps.push_back(std::move(bus));
      steps.push_back({{"xdg-open", folder ? p : folder_of(path)}, "", false});
      break;
    }
  }
  return steps;
}

}  // namespace ge::capture
