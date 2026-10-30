// The capture's container (mux.hpp).
//
// ge:summary container_of and Mux::open: the file's name to its container, the writer opened
// ge:keywords container extension mp4 m4v mkv webm open
// ge:skill replay
// ge:test tests/capture_mp4_test.cpp
#include "capture/mux.hpp"

#include <cctype>

namespace ge::capture {

Container container_of(std::string_view path) {
  const size_t dot = path.rfind('.');
  if (dot == std::string_view::npos) return Container::kMatroska;
  std::string ext;
  for (const char ch : path.substr(dot + 1)) {
    ext.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
  }
  return ext == "mp4" || ext == "m4v" ? Container::kMp4 : Container::kMatroska;
}

bool container_named(std::string_view word, std::string_view path, Container& out,
                     std::string& error) {
  if (word.empty()) {
    out = container_of(path);
  } else if (word == "mp4") {
    out = Container::kMp4;
  } else if (word == "mkv") {
    out = Container::kMatroska;
  } else {
    error = "no container `" + std::string(word) + "`; the containers are mp4 and mkv";
    return false;
  }
  return true;
}

bool Mux::open(const std::string& path, Container c, const MatroskaOptions& o, std::string& error) {
  mp4_on_ = c == Container::kMp4;
  if (mp4_on_) {
    doc_type_ = "mp4";
    return mp4_.open(path, o, error);
  }
  doc_type_ = std::string(doc_type_of(o));
  if (doc_type_ != "webm" && path.size() >= 5) {
    std::string ext;
    for (const char ch : std::string_view(path).substr(path.size() - 5)) {
      ext.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    if (ext == ".webm") {
      error = "a .webm carries AV1 and Opus only, and " + path +
              "'s tracks are not; name it .mp4 or .mkv, or record with --codec=av1";
      return false;
    }
  }
  return mkv_.open(path, o, error);
}

}  // namespace ge::capture
