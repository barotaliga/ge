// A finished match's report (net/report.hpp).
//
// ge:summary A finished match's report: its bytes written and read, damage refused
// ge:keywords match-report report_write report_read
// ge:skill net
// ge:test tests/net_report_test.cpp
#include "net/report.hpp"

#include <cstring>

#include "net/wire.hpp"

namespace ge::net {

namespace {

constexpr char kMagic[4] = {'g', 'e', 'M', 'R'};
constexpr uint8_t kVersion = 1;
constexpr size_t kMost = size_t{64} * 1024;

uint64_t zig(int64_t v) { return (static_cast<uint64_t>(v) << 1) ^ static_cast<uint64_t>(v >> 63); }
int64_t zag(uint64_t v) { return static_cast<int64_t>(v >> 1) ^ -static_cast<int64_t>(v & 1); }

void put_str(Writer& w, const std::string& s) {
  const size_t n = s.size() < kReportName ? s.size() : kReportName;
  write_u8(w, static_cast<uint8_t>(n));
  write_bytes(w, s.data(), n);
}
void put_int(Writer& w, int64_t v) { write_varint(w, zig(v)); }

bool get_str(Reader& r, std::string& out) {
  const uint8_t n = read_u8(r);
  if (n > kReportName) {
    r.bad = true;
    return false;
  }
  const uint8_t* p = read_view(r, n);
  if (p == nullptr) return false;
  out.assign(reinterpret_cast<const char*>(p), n);
  return true;
}
int64_t get_int(Reader& r) { return zag(read_varint(r)); }

}  // namespace

const char* match_end_name(MatchEnd e) {
  switch (e) {
    case MatchEnd::limit:
      return "limit";
    case MatchEnd::forfeit:
      return "forfeit";
    case MatchEnd::broken:
      return "broken";
  }
  return "?";
}

bool report_write(const MatchReport& r, std::vector<uint8_t>& out, std::string& error) {
  if (r.players.size() > kReportPlayers || r.teams.size() > kReportTeams ||
      r.columns.size() > kReportColumns) {
    error = "the match report has " + std::to_string(r.players.size()) + " players, " +
            std::to_string(r.teams.size()) + " teams and " + std::to_string(r.columns.size()) +
            " columns; at most " + std::to_string(kReportPlayers) + ", " +
            std::to_string(kReportTeams) + " and " + std::to_string(kReportColumns) +
            " are kept. Report fewer columns, or the match's players only";
    return false;
  }
  for (const ReportPlayer& p : r.players)
    if (p.columns.size() != r.columns.size()) {
      error = "the match report's player " + p.name + " has " + std::to_string(p.columns.size()) +
              " column values for " + std::to_string(r.columns.size()) +
              " columns. Give each player one value per column";
      return false;
    }
  out.assign(kMost, 0);
  Writer w = make_writer(out.data(), out.size());
  write_bytes(w, kMagic, 4);
  write_u8(w, kVersion);
  write_u64(w, r.id);
  for (const std::string* s :
       {&r.game, &r.version, &r.server, &r.address, &r.mode, &r.map, &r.demo_sha256})
    put_str(w, *s);
  put_int(w, r.start_s);
  put_int(w, r.duration_ms);
  write_u8(w, static_cast<uint8_t>(r.ended));
  write_u8(w, static_cast<uint8_t>(r.columns.size()));
  for (const std::string& c : r.columns) put_str(w, c);
  write_u8(w, static_cast<uint8_t>(r.teams.size()));
  for (const ReportTeam& t : r.teams) {
    put_str(w, t.name);
    put_int(w, t.score);
    write_u32(w, t.colour);
  }
  write_u8(w, static_cast<uint8_t>(r.players.size()));
  for (const ReportPlayer& p : r.players) {
    put_str(w, p.name);
    put_str(w, p.team);
    write_u64(w, p.account);
    write_u8(w, p.bot ? 1 : 0);
    for (const int64_t v : {p.score, p.frags, p.deaths, p.suicides, p.team_kills, p.seconds,
                            p.ping_min, p.ping_avg, p.ping_max})
      put_int(w, v);
    for (const int64_t v : p.columns) put_int(w, v);
  }
  out.resize(w.size);  // the limits keep it far under kMost
  return !w.overflow;
}

bool report_read(const uint8_t* data, size_t size, MatchReport& out, std::string& error) {
  out = MatchReport{};
  Reader r = make_reader(data, size);
  const uint8_t* m = read_view(r, 4);
  if (m == nullptr || std::memcmp(m, kMagic, 4) != 0) {
    error = "not a match report (no geMR at its start)";
    return false;
  }
  const uint8_t version = read_u8(r);
  if (!r.bad && version != kVersion) {
    error = "a match report of version " + std::to_string(version) + "; this reads version " +
            std::to_string(kVersion) + ". Update the reader";
    return false;
  }
  out.id = read_u64(r);
  for (std::string* s :
       {&out.game, &out.version, &out.server, &out.address, &out.mode, &out.map, &out.demo_sha256})
    get_str(r, *s);
  out.start_s = get_int(r);
  out.duration_ms = get_int(r);
  const uint8_t ended = read_u8(r);
  if (ended > static_cast<uint8_t>(MatchEnd::broken)) r.bad = true;
  out.ended = static_cast<MatchEnd>(ended);
  const auto counted = [&](size_t limit, const char* what) {
    const uint8_t n = read_u8(r);
    if (!r.bad && n > limit) {
      error = std::string("the match report lists ") + std::to_string(n) + " " + what +
              ", past the " + std::to_string(limit) + " a report holds";
      r.bad = true;
    }
    return r.bad ? size_t{0} : size_t{n};
  };
  out.columns.resize(counted(kReportColumns, "columns"));
  for (std::string& c : out.columns) get_str(r, c);
  out.teams.resize(counted(kReportTeams, "teams"));
  for (ReportTeam& t : out.teams) {
    get_str(r, t.name);
    t.score = get_int(r);
    t.colour = read_u32(r);
  }
  out.players.resize(counted(kReportPlayers, "players"));
  for (ReportPlayer& p : out.players) {
    get_str(r, p.name);
    get_str(r, p.team);
    p.account = read_u64(r);
    p.bot = read_u8(r) != 0;
    for (int64_t* v : {&p.score, &p.frags, &p.deaths, &p.suicides, &p.team_kills, &p.seconds,
                       &p.ping_min, &p.ping_avg, &p.ping_max})
      *v = get_int(r);
    p.columns.resize(out.columns.size());
    for (int64_t& v : p.columns) v = get_int(r);
  }
  if (!r.bad && remaining(r) != 0) {
    error = "the match report has " + std::to_string(remaining(r)) + " bytes past its end";
    return false;
  }
  if (r.bad) {
    if (error.empty())
      error = "the match report is cut short or damaged at byte " + std::to_string(r.at) + " of " +
              std::to_string(size);
    out = MatchReport{};
    return false;
  }
  return true;
}

}  // namespace ge::net
