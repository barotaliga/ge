// Every match recorded by itself (autorecord.hpp).
//
// ge:summary Every match recorded by itself, defined
// ge:keywords autorecord sv_autorecord match_record_frame
// ge:skill replay net
// ge:test tests/server_autorecord_test.cpp
#include "server/autorecord.hpp"

#include <cstdio>

namespace ge::server {

namespace {

// Days since 1970-01-01 to a civil date (proleptic Gregorian).
void civil(int64_t days, int64_t& y, int& m, int& d) {
  days += 719468;
  const int64_t era = (days >= 0 ? days : days - 146096) / 146097;
  const auto doe = static_cast<int64_t>(days - era * 146097);
  const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int64_t mp = (5 * doy + 2) / 153;
  d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  y = yoe + era * 400 + (m <= 2 ? 1 : 0);
}

}  // namespace

MatchRecord match_record_frame(MatchRecorder& m, const Server& s, std::string& error) {
  // `last` starts as warmup, so a server in its match when first looked at (a game with no
  // phases, a lobby off) records from the frame after.
  const LobbyPhase now = s.phase;
  const LobbyPhase was = m.last;
  m.last = now;
  if (m.on) {
    if (now != LobbyPhase::match) {
      record_finish(m.rec, s);
      m.on = false;
      m.matches++;
      return MatchRecord::finished;
    }
    record_frame(m.rec, s);
    return MatchRecord::recording;
  }
  // The phase read after a frame is what the game decided at that frame's end (a countdown run
  // out, the map restarted for the match): its viewers see it from the next frame on, so the
  // recording begins one frame after the phase turned.
  if (now != LobbyPhase::match) {
    m.due = false;
    return MatchRecord::none;
  }
  if (was != LobbyPhase::match) {
    m.due = true;
    return MatchRecord::none;
  }
  if (!m.due) return MatchRecord::none;
  m.due = false;
  m.rec = Recording{};
  m.rec.keyframe_seconds = m.keyframe_seconds;
  if (!record_begin(m.rec, s, error)) return MatchRecord::failed;
  m.on = true;
  m.started_us = s.now_us;
  return MatchRecord::started;
}

bool match_record_stop(MatchRecorder& m, const Server& s) {
  if (!m.on) return false;
  record_finish(m.rec, s);
  m.on = false;
  m.matches++;
  return true;
}

std::string match_record_name(const std::string& map, int64_t unix_s) {
  const int64_t days = (unix_s >= 0 ? unix_s : unix_s - 86399) / 86400;
  const int64_t secs = unix_s - days * 86400;
  int64_t y = 0;
  int mo = 0, d = 0;
  civil(days, y, mo, d);
  char when[40];
  std::snprintf(when, sizeof when, "%04lld%02d%02d-%02d%02d%02d", static_cast<long long>(y), mo, d,
                static_cast<int>(secs / 3600), static_cast<int>(secs / 60 % 60),
                static_cast<int>(secs % 60));
  std::string name;
  for (const char c : map)  // a map's name as a file's: letters, digits, '-', '_'
    name += (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                    c == '-' || c == '_'
                ? c
                : '_';
  if (name.empty()) name = "match";
  return name + "-" + when + ".ged";
}

}  // namespace ge::server
