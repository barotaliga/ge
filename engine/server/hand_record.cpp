// A recording the operator starts and stops (hand_record.hpp).
//
// ge:summary serverrecord and serverstop: a match recorded from the console to demos/
// ge:keywords serverrecord serverstop hand-record
// ge:skill replay net
// ge:test tests/server_hand_record_test.cpp
#include "server/hand_record.hpp"

#include <span>
#include <system_error>

namespace ge::server {

namespace {

void write_fresh(HandRecording& h) {
  const std::span<const uint8_t> fresh = record_fresh(h.rec);
  h.out.write(reinterpret_cast<const char*>(fresh.data()),
              static_cast<std::streamsize>(fresh.size()));
}

}  // namespace

bool hand_record_start(HandRecording& h, const Server& s, const std::filesystem::path& dir,
                       std::string_view name, std::string& said) {
  if (!h.path.empty()) {
    said = "serverrecord: already recording to " + h.path + "; serverstop ends it first";
    return false;
  }
  if (name.empty() || name.find("..") != std::string_view::npos ||
      name.find_first_of("/\\:") != std::string_view::npos) {
    said = "serverrecord: give a file name alone, without .., /, \\ or : (it goes in " +
           dir.string() + ")";
    return false;
  }
  std::string file(name);
  if (file.size() < 4 || file.compare(file.size() - 4, 4, ".ged") != 0) file += ".ged";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  const std::string path = (dir / file).string();
  h.rec = Recording{};
  std::string error;
  if (!record_begin(h.rec, s, error)) {
    said = "serverrecord: this game cannot be recorded: " + error;
    return false;
  }
  h.out.open(path, std::ios::binary);
  if (!h.out) {
    said = "serverrecord: cannot make " + path + "; check the folder";
    h.rec = Recording{};
    return false;
  }
  h.path = path;
  write_fresh(h);
  said = "serverrecord: recording to " + path + "; serverstop ends it";
  return true;
}

void hand_record_frame(HandRecording& h, const Server& s) {
  if (h.path.empty() || !h.rec.open) return;
  record_frame(h.rec, s);
  write_fresh(h);
}

bool hand_record_stop(HandRecording& h, const Server& s, std::string& said) {
  if (h.path.empty()) {
    said = "serverstop: nothing is being recorded";
    return false;
  }
  if (h.rec.open) record_finish(h.rec, s);
  write_fresh(h);
  h.out.close();
  said = h.out
             ? "serverstop: " + h.path + " kept (" + std::to_string(h.rec.bytes.size()) + " bytes)"
             : "serverstop: " + h.path + " was cut short; check the disk";
  h.path.clear();
  h.rec = Recording{};
  return true;
}

}  // namespace ge::server
