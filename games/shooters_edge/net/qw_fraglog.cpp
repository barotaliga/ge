// The classic endpoint's frag log (qw_fraglog.hpp).
//
// ge:summary The frag log's buffer and file, fraglogfile and the `log` query's answer
// ge:keywords fraglogfile frag-log stdlog log-query
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_admin_test.cpp
#include "shooters_edge/net/qw_fraglog.hpp"

#include <cstdlib>
#include <filesystem>

#include "net/transport.hpp"

namespace shooters_edge::net {

namespace gn = ge::net;

void qw_frag_frame(QwServer& s) {
  FragLog& f = s.frags;
  if (!f.started) {
    f.turned_at = s.now;
    f.started = true;
  }
  if (s.game != nullptr) {
    const std::vector<std::string>& lines = s.game->frag_log;
    if (f.seen > lines.size()) f.seen = 0;  // the rules started again
    for (; f.seen < lines.size(); f.seen++) {
      const std::string& line = lines[f.seen];
      if (f.filling.size() + line.size() + 1 > kFragBuffer) f.filling.clear();
      f.filling += line;
      if (f.file != nullptr) {
        std::fwrite(line.data(), 1, line.size(), f.file);
        std::fflush(f.file);
      }
    }
  }
  if (f.filling.size() > 4096 || (s.now - f.turned_at > kFragTurnoverS && !f.filling.empty())) {
    f.served = std::move(f.filling);
    f.filling.clear();
    f.turned_at = s.now;
    f.sequence++;
    if (s.game != nullptr)
      s.game->console.push_back("beginning fraglog sequence " + std::to_string(f.sequence));
  }
}

std::string qw_fraglogfile(QwServer& s) {
  FragLog& f = s.frags;
  if (f.file != nullptr) {
    qw_frag_close(s);
    return "Frag file logging off.\n";
  }
  if (s.cfg.frag_dir.empty()) return "Can't open any logfiles.\n";
  for (int n = 0; n < kFragFiles; n++) {
    const std::filesystem::path path =
        std::filesystem::path(s.cfg.frag_dir) / ("frag_" + std::to_string(n) + ".log");
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) continue;
    f.file = std::fopen(path.string().c_str(), "wb");
    if (f.file == nullptr) break;
    f.path = path.string();
    return "Logging frags to " + f.path + ".\n";
  }
  return "Can't open any logfiles.\n";
}

void qw_log_query(QwServer& s, const gn::Address& from, const std::string& sequence, bool asked) {
  const FragLog& f = s.frags;
  const int theirs = asked ? std::atoi(sequence.c_str()) : -1;
  std::string out;
  if (theirs == f.sequence - 1 || f.file == nullptr) {
    out = "m";
  } else {
    out = "stdlog " + std::to_string(f.sequence - 1) + "\n" + f.served;
    out.push_back('\0');
  }
  gn::transport_send(*s.transport, from, reinterpret_cast<const uint8_t*>(out.data()), out.size(),
                     static_cast<int64_t>(s.now * 1e6));
  s.counts.oob_out++;
  s.counts.datagrams_out++;
  s.counts.bytes_out += static_cast<int64_t>(out.size());
}

void qw_frag_close(QwServer& s) {
  if (s.frags.file != nullptr) std::fclose(s.frags.file);
  s.frags.file = nullptr;
  s.frags.path.clear();
}

}  // namespace shooters_edge::net
