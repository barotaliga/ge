// The packet log (packet_log.hpp).
//
// ge:summary The packet log: the file, its settings, each datagram's header and hex rows
// ge:keywords net packet-log net_log hex-dump
// ge:skill net
// ge:test tests/net_packet_log_test.cpp
#include "net/packet_log.hpp"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <system_error>

namespace ge::net {

PacketLog::~PacketLog() {
  if (file != nullptr) std::fclose(file);
}

void packet_log_close(PacketLog& log) {
  if (log.file == nullptr) return;
  std::fclose(log.file);
  log.file = nullptr;
  log.shown.clear();
}

bool packet_log_update(PacketLog& log, int mode, std::string_view folder, std::string_view name,
                       int flush, std::string& said) {
  if (mode == log.mode && flush == log.flush && name == log.name && folder == log.folder)
    return true;
  if (log.file != nullptr) said += "Closing network log.\n";
  packet_log_close(log);
  log.mode = mode;
  log.flush = flush;
  log.name = name;
  log.folder = folder;
  if (mode <= 0) return true;
  std::string file(name);
  for (char& c : file)  // the name stays in logs/: no folders, no odd letters
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
          c == '-' || c == '.'))
      c = '_';
  if (file.size() < 4 || file.compare(file.size() - 4, 4, ".log") != 0) file += ".log";
  log.shown = "logs/" + file;
  if (name.empty()) {
    said += "Couldn't open " + log.shown + ": the name is empty\n";
    log.mode = 0;
    return false;
  }
  const std::filesystem::path dir = std::filesystem::path(folder) / "logs";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  log.file = std::fopen((dir / file).string().c_str(), mode == 1 ? "w" : "a");
  if (log.file == nullptr) {
    said += "Couldn't open " + log.shown + ": " + std::strerror(errno) + "\n";
    log.mode = 0;
    return false;
  }
  if (flush >= 2) std::setvbuf(log.file, nullptr, _IONBF, 0);  // 1: flushed after each entry
  said += "Logging network packets to " + log.shown + "\n";
  return true;
}

std::string packet_log_text(int64_t ms, bool tcp, bool sent, const Address& peer,
                            const uint8_t* data, size_t size) {
  static const char kHex[] = "0123456789abcdef";
  std::string out = std::to_string(ms < 0 ? 0 : ms);
  out += tcp ? " : TCP " : " : UDP ";
  out += sent ? "send : " : "recv : ";
  out += to_string(peer);
  out += " : " + std::to_string(size) + " bytes\n";
  for (size_t row = 0; row < size; row += 16) {
    char at[8];
    std::snprintf(at, sizeof at, "%04zx", row & 0xffff);
    out += at;
    out += " : ";
    for (size_t i = 0; i < 16; i++) {
      if (row + i < size) {
        out += kHex[data[row + i] >> 4];
        out += kHex[data[row + i] & 15];
        out += ' ';
      } else {
        out += "   ";
      }
    }
    out += ": ";
    for (size_t i = 0; i < 16; i++) {
      if (row + i >= size) {
        out += ' ';
        continue;
      }
      const uint8_t b = data[row + i];
      out += b >= 32 && b <= 126 ? static_cast<char>(b) : '.';
    }
    out += '\n';
  }
  out += '\n';
  return out;
}

void packet_log_datagram(PacketLog& log, int64_t now_us, bool tcp, bool sent, const Address& peer,
                         const uint8_t* data, size_t size) {
  if (log.file == nullptr) return;
  const std::string text =
      packet_log_text((now_us - log.start_us) / 1000, tcp, sent, peer, data, size);
  std::fwrite(text.data(), 1, text.size(), log.file);
  if (log.flush == 1) std::fflush(log.file);
}

}  // namespace ge::net
