// Quake II's server downloads (q2_net_download.hpp).
//
// ge:summary Quake II's server downloads: the asked file checked and read, pieces filling the packet
// ge:keywords q2-download download nextdl svc-download
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_download_pace_test.cpp
#include "shooters_edge/net/q2_net_download.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "classic/protocol/download_pace.hpp"
#include "vfs/partial.hpp"

namespace shooters_edge::net {

namespace {

namespace cp = ge::classic::protocol;

constexpr uint8_t kSvcPrint = 10, kSvcDownload = 16;
constexpr uint8_t kSvcZDownload = 22;  // a download's piece of a deflate stream (36, minor 1021)
constexpr uint8_t kSvcGeOrigin = 123;  // the game's own: a download's archive (qw_svc.hpp)
constexpr size_t kPacketHeader = 10;   // the channel's sequences and port, counted in the pause

void reliable(Q2NetClient& c, const std::vector<uint8_t>& b) {
  if (!b.empty()) channel_reliable(c.ch, b.data(), b.size());
}

// The most `sv_max_download_size` may allow: 128 MiB and 4 KiB.
constexpr int64_t kQ2DownloadCap = 134221824;

}  // namespace

std::string q2_plain_download_path(std::string path) {
  for (char& ch : path) {
    if (ch == '\\') ch = '/';
    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
  }
  std::vector<std::string> parts;
  size_t at = 0;
  while (at <= path.size()) {
    const size_t end = std::min(path.find('/', at), path.size());
    const std::string part = path.substr(at, end - at);
    if (part == "..") {
      if (parts.empty()) return "";
      parts.pop_back();
    } else if (!part.empty() && part != ".") {
      parts.push_back(part);
    }
    at = end + 1;
  }
  std::string out;
  for (const std::string& p : parts) out += (out.empty() ? "" : "/") + p;
  return out;
}

std::string q2_start_download(Q2NetServer& s, Q2NetClient& c, const std::string& asked,
                              int offset) {
  c.downloading = false;
  c.download_deflated = false;
  c.download.clear();
  const std::string path = asked.size() >= 64 ? std::string() : q2_plain_download_path(asked);
  const auto under = [&path](const char* dir) { return path.rfind(dir, 0) == 0; };
  const auto plain_end = [](char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-';
  };
  // The kind a file is by its first folder, each switched on or off by its own setting.
  const bool kind_on = under("players/")                       ? s.cfg.allow_players
                       : under("models/") || under("sprites/") ? s.cfg.allow_models
                       : under("sound/")                       ? s.cfg.allow_sounds
                       : under("maps/")                        ? s.cfg.allow_maps
                       : under("textures/") || under("env/")   ? s.cfg.allow_textures
                       : under("pics/")                        ? s.cfg.allow_pics
                                                               : s.cfg.allow_others;
  const int64_t most = s.cfg.max_download_size <= 0 || s.cfg.max_download_size > kQ2DownloadCap
                           ? kQ2DownloadCap
                           : s.cfg.max_download_size;
  std::string why;
  ge::vfs::ArchiveOrigin origin;
  bool packed = false;
  if (!s.cfg.allow_download || !s.cfg.read_file) {
    why = "downloads are off";
  } else if (path.empty() || offset < 0 || !plain_end(path.front()) || !plain_end(path.back()) ||
             path.find('/') == std::string::npos) {
    why = "not a file the server offers";
  } else if (!kind_on) {
    why = "that kind of file is not offered";
  } else if (!s.cfg.read_file(path, &c.download) || c.download.empty()) {
    why = "not found";
  } else if (offset == 0 && c.protocol == 36 && c.minor >= 1021 && c.zlib && !c.told_origins &&
             s.cfg.read_deflated && s.cfg.read_deflated(path, &c.download)) {
    // A `.pkz`'s deflated file goes as it lies; its size and the limit are the stream's.
    c.download_deflated = true;
    if (static_cast<int64_t>(c.download.size()) > most) why = "larger than sv_max_download_size";
  } else if (static_cast<int64_t>(c.download.size()) > most) {
    why = "larger than sv_max_download_size";
  } else {
    packed = s.cfg.file_origin && s.cfg.file_origin(path, &origin);
    if (packed && under("maps/") && !c.told_origins) why = "the map is in an archive";
  }
  if (why.empty() && static_cast<size_t>(offset) > c.download.size()) {
    const std::string told =
        "File size differs from server.\nPlease delete the corresponding .tmp file from your "
        "system.\n";
    std::vector<uint8_t> b = {kSvcPrint, 2};  // the high print level
    b.insert(b.end(), told.begin(), told.end());
    b.push_back(0);
    reliable(c, b);
    why = "a resume past its end";
  }
  if (!why.empty()) {
    c.download.clear();
    const uint8_t refused[4] = {kSvcDownload, 0xff, 0xff, 0};
    reliable(c, std::vector<uint8_t>(refused, refused + 4));
    return c.name + " asked for " + asked + ": refused, " + why;
  }
  if (packed && c.told_origins) {  // the archive the file comes out of, before its first piece
    std::vector<uint8_t> b = {kSvcGeOrigin};
    ge::vfs::write_origin(path, origin, &b);
    reliable(c, b);
  }
  c.downloading = true;
  c.download_name = path;
  c.download_at = static_cast<size_t>(offset);
  c.download_owed = true;
  return "";
}

bool q2_download_piece(Q2NetServer& s, Q2NetClient& c) {
  if (!c.downloading || !c.download_owed || channel_reliable_backlog(c.ch) > 0 ||
      channel_sending_pieces(c.ch) || s.now < c.download_wait)
    return false;
  // The packet's room: the client's packet length (an old channel's message held to its buffer).
  const size_t len = static_cast<size_t>(c.msglen);
  const size_t room = c.ch.fragments ? len : std::min(len, ge::classic::protocol::kMaxMsgLen - 16);
  const size_t n = cp::download_piece_bytes(cp::DownloadPiece::fill_room, c.rate, room,
                                            c.download.size() - c.download_at);
  c.download_at += n;
  const int percent =
      c.download.empty() ? 100 : static_cast<int>(c.download_at * 100 / c.download.size());
  std::vector<uint8_t> b = {c.download_deflated ? kSvcZDownload : kSvcDownload,
                            static_cast<uint8_t>(n & 0xff), static_cast<uint8_t>(n >> 8),
                            static_cast<uint8_t>(percent)};
  b.insert(b.end(), c.download.begin() + static_cast<std::ptrdiff_t>(c.download_at - n),
           c.download.begin() + static_cast<std::ptrdiff_t>(c.download_at));
  reliable(c, b);
  c.download_owed = false;
  c.download_pieces++;
  const bool here = c.address.family == ge::net::Family::ipv4 && c.address.bytes[0] == 127;
  c.download_wait = s.now + cp::download_pause(b.size() + kPacketHeader, here ? 0 : c.rate);
  if (c.download_at >= c.download.size()) {
    c.downloading = false;
    c.downloads++;
    c.download.clear();
  }
  return true;
}

}  // namespace shooters_edge::net
