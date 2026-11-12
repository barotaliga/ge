// The hub's demo archive, defined: the rows from a replay file's meta, the files by hash, the
// operator's table, the inbox, retention and search.
//
// ge:summary The hub's demo archive defined: rows from meta, files by hash, inbox, prune, search
// ge:keywords hub archive demo sha256 search inbox prune retention archive-tsv
// ge:skill net
// ge:test tests/hub_archive_test.cpp
#include "hub/archive.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <system_error>
#include <utility>

#include "hub/api.hpp"
#include "mod/sha256.hpp"
#include "replay/stream.hpp"

namespace ge::hub {

namespace fs = std::filesystem;
namespace gr = ge::replay;

namespace {

int64_t to_i64(const std::string& s, int64_t fallback) {
  if (s.empty()) return fallback;
  int64_t v = 0;
  size_t i = 0;
  const bool neg = s[0] == '-';
  if (neg) i = 1;
  if (i == s.size()) return fallback;
  for (; i < s.size(); i++) {
    if (s[i] < '0' || s[i] > '9') return fallback;
    v = v * 10 + (s[i] - '0');
  }
  return neg ? -v : v;
}

std::pair<std::string, std::string> split_tab(const std::string& s) {
  const size_t t = s.find('\t');
  if (t == std::string::npos) return {s, ""};
  return {s.substr(0, t), s.substr(t + 1)};
}

std::string lower(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
  return out;
}

void add_tag(std::vector<std::string>& tags, const std::string& t) {
  if (!t.empty() && std::find(tags.begin(), tags.end(), t) == tags.end()) tags.push_back(t);
}

char fold(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

// Whether `needle` (already lower case) is in `hay`, ASCII case folded; no allocation.
bool contains_folded(std::string_view hay, std::string_view needle) {
  if (needle.size() > hay.size()) return false;
  for (size_t i = 0; i + needle.size() <= hay.size(); i++) {
    size_t k = 0;
    while (k < needle.size() && fold(hay[i + k]) == needle[k]) k++;
    if (k == needle.size()) return true;
  }
  return false;
}

bool newer(const DemoRow& a, const DemoRow& b) {
  if (a.start_s != b.start_s) return a.start_s > b.start_s;
  return a.sha < b.sha;
}

void sort_rows(Archive& a) { std::sort(a.rows.begin(), a.rows.end(), newer); }

std::optional<std::vector<uint8_t>> read_file(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  if (!f) return std::nullopt;
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

bool write_file(const fs::path& p, std::span<const uint8_t> bytes) {
  std::error_code ec;
  fs::create_directories(p.parent_path(), ec);
  const fs::path tmp = p.string() + ".part";
  {
    std::ofstream f(tmp, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    if (!f) return false;
  }
  fs::rename(tmp, p, ec);
  return !ec;
}

// The operator's table: hash, the time it came, its tags (comma-separated).
struct Kept {
  int64_t added_s = 0;
  std::vector<std::string> tags;
};

std::map<std::string, Kept> read_table(const fs::path& p) {
  std::map<std::string, Kept> out;
  std::ifstream f(p);
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty() || line[0] == '#') continue;
    const auto [sha, rest] = split_tab(line);
    const auto [added, tags] = split_tab(rest);
    Kept k;
    k.added_s = to_i64(added, 0);
    size_t at = 0;
    while (at <= tags.size() && !tags.empty()) {
      const size_t comma = tags.find(',', at);
      add_tag(k.tags, tags.substr(at, comma == std::string::npos ? std::string::npos : comma - at));
      if (comma == std::string::npos) break;
      at = comma + 1;
    }
    out[sha] = k;
  }
  return out;
}

void write_table(const Archive& a) {
  std::string text = "# sha256\tadded (unix s)\ttags\n";
  for (const DemoRow& r : a.rows) {
    text += r.sha + "\t" + std::to_string(r.added_s) + "\t";
    for (size_t i = 0; i < r.tags.size(); i++) text += (i ? "," : "") + r.tags[i];
    text += "\n";
  }
  write_file(fs::path(a.dir) / "archive.tsv",
             std::span(reinterpret_cast<const uint8_t*>(text.data()), text.size()));
}

bool is_hex_sha(std::string_view s) {
  if (s.size() != 64) return false;
  for (char c : s) {
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  }
  return true;
}

}  // namespace

std::string archive_path(std::string_view sha) {
  return "d/" + std::string(sha.substr(0, 3)) + "/" + std::string(sha) + ".ger";
}

bool demo_row(std::span<const uint8_t> bytes, DemoRow& out, std::string& error) {
  gr::StreamReader r;
  if (!gr::read_stream_header(bytes, r, error)) return false;
  out = DemoRow{};
  out.game = r.header.game;
  out.game_version = r.header.game_version;
  out.size = static_cast<int64_t>(bytes.size());
  out.start_s = 0;
  for (const gr::TrackInfo& t : r.header.tracks) out.radar |= t.kind == gr::TrackKind::radar;
  for (const auto& [k, v] : r.header.meta) {
    if (k == "map") {
      out.map = v;
    } else if (k == "mode") {
      out.mode = v;
    } else if (k == "server") {
      out.server = v;
    } else if (k == "title") {
      out.title = v;
    } else if (k == "start") {
      out.start_s = to_i64(v, 0);
    } else if (k == "tag") {
      add_tag(out.tags, v);
    } else if (k == "team") {
      out.teams.push_back({v, 0, false});
    } else if (k == "player") {
      const auto [name, team] = split_tab(v);
      out.players.push_back({name, team, 0, false});
    }
  }
  if (const std::optional<gr::Trailer> t = gr::read_trailer(bytes)) {
    out.duration_ms = 0;
    for (const auto& [k, v] : t->meta) {
      if (k == "duration_ms") {
        out.duration_ms = to_i64(v, 0);
      } else if (k == "score" || k == "team_score") {
        const auto [name, score] = split_tab(v);
        const int32_t s = static_cast<int32_t>(to_i64(score, 0));
        if (k == "score") {
          auto it = std::find_if(out.players.begin(), out.players.end(),
                                 [&](const DemoPlayer& p) { return p.name == name; });
          if (it == out.players.end())
            it = out.players.insert(out.players.end(), {name, "", 0, false});
          it->score = s;
          it->scored = true;
        } else {
          auto it = std::find_if(out.teams.begin(), out.teams.end(),
                                 [&](const DemoTeam& d) { return d.name == name; });
          if (it == out.teams.end()) it = out.teams.insert(out.teams.end(), {name, 0, false});
          it->score = s;
          it->scored = true;
        }
      }
    }
  }
  return true;
}

bool archive_open(Archive& a, const std::string& dir, std::string& error) {
  a = Archive{};
  a.dir = dir;
  std::error_code ec;
  fs::create_directories(fs::path(dir) / "d", ec);
  fs::create_directories(fs::path(dir) / "inbox", ec);
  if (ec || !fs::is_directory(fs::path(dir) / "d", ec)) {
    error = "the archive's directory " + dir + " cannot be made (" + ec.message() +
            "); pass --archive=<a writable directory>";
    return false;
  }
  const std::map<std::string, Kept> table = read_table(fs::path(dir) / "archive.tsv");
  for (fs::recursive_directory_iterator it(fs::path(dir) / "d", ec), end; !ec && it != end;
       it.increment(ec)) {
    if (!it->is_regular_file(ec) || it->path().extension() != ".ger") continue;
    const std::string sha = it->path().stem().string();
    if (!is_hex_sha(sha)) continue;
    const std::optional<std::vector<uint8_t>> bytes = read_file(it->path());
    DemoRow row;
    std::string why;
    if (!bytes || !demo_row(*bytes, row, why)) continue;
    row.sha = sha;
    const auto k = table.find(sha);
    if (k != table.end()) {
      row.added_s = k->second.added_s;
      for (const std::string& t : k->second.tags) add_tag(row.tags, t);
    }
    if (row.start_s == 0) row.start_s = row.added_s;
    a.bytes += row.size;
    a.rows.push_back(std::move(row));
  }
  if (ec) {
    error = "the archive's directory " + dir + "/d cannot be read (" + ec.message() +
            "); check its permissions";
    return false;
  }
  sort_rows(a);
  return true;
}

AddResult archive_add(Archive& a, std::span<const uint8_t> bytes, int64_t now_s,
                      const std::vector<std::string>& tags, std::string& error, DemoRow* row) {
  DemoRow r;
  if (!demo_row(bytes, r, error)) {
    a.stats.refused++;
    return AddResult::refused;
  }
  r.sha = mod::sha256_hex(bytes.data(), bytes.size());
  for (DemoRow& old : a.rows) {
    if (old.sha != r.sha) continue;
    for (const std::string& t : tags) add_tag(old.tags, t);
    if (!a.dir.empty()) write_table(a);
    if (row) *row = old;
    a.stats.duplicates++;
    return AddResult::duplicate;
  }
  r.added_s = now_s;
  if (r.start_s == 0) r.start_s = now_s;
  for (const std::string& t : tags) add_tag(r.tags, t);
  if (!a.dir.empty() && !write_file(fs::path(a.dir) / archive_path(r.sha), bytes)) {
    error = "the demo cannot be written under " + a.dir + "/d; check the disk and its permissions";
    a.stats.refused++;
    return AddResult::refused;
  }
  a.bytes += r.size;
  if (row) *row = r;
  a.rows.push_back(std::move(r));
  sort_rows(a);
  if (!a.dir.empty()) write_table(a);
  a.stats.added++;
  return AddResult::added;
}

int archive_ingest(Archive& a, int64_t now_s, std::vector<std::string>& errors) {
  if (a.dir.empty()) return 0;
  const fs::path inbox = fs::path(a.dir) / "inbox";
  std::error_code ec;
  std::vector<fs::path> files;
  for (fs::directory_iterator it(inbox, ec), end; !ec && it != end; it.increment(ec)) {
    if (it->is_regular_file(ec) && it->path().extension() == ".ger") files.push_back(it->path());
  }
  std::sort(files.begin(), files.end());
  int taken = 0;
  for (const fs::path& p : files) {
    const std::optional<std::vector<uint8_t>> bytes = read_file(p);
    if (!bytes) continue;
    std::string error;
    if (archive_add(a, *bytes, now_s, {}, error) == AddResult::refused) {
      errors.push_back(p.filename().string() + ": " + error);
      fs::create_directories(inbox / "refused", ec);
      fs::rename(p, inbox / "refused" / p.filename(), ec);
      continue;
    }
    fs::remove(p, ec);
    taken++;
  }
  return taken;
}

int archive_prune(Archive& a, int64_t max_bytes, int64_t max_age_s, int64_t now_s) {
  int dropped = 0;
  // The oldest first: the rows are newest first, so walk from the end.
  for (size_t i = a.rows.size(); i-- > 0;) {
    const DemoRow& r = a.rows[i];
    if (!r.tags.empty()) continue;
    const bool too_old = max_age_s > 0 && now_s - r.start_s > max_age_s;
    const bool too_big = max_bytes > 0 && a.bytes > max_bytes;
    if (!too_old && !too_big) continue;
    if (!a.dir.empty()) {
      std::error_code ec;
      fs::remove(fs::path(a.dir) / archive_path(r.sha), ec);
    }
    a.bytes -= r.size;
    a.rows.erase(a.rows.begin() + static_cast<std::ptrdiff_t>(i));
    dropped++;
  }
  a.stats.pruned += static_cast<uint64_t>(dropped);
  if (dropped && !a.dir.empty()) write_table(a);
  return dropped;
}

const DemoRow* archive_find(const Archive& a, std::string_view sha) {
  for (const DemoRow& r : a.rows) {
    if (r.sha == sha) return &r;
  }
  return nullptr;
}

DemoQuery demo_query(std::string_view target) {
  DemoQuery q;
  q.mode = query_param(target, "mode");
  q.map = query_param(target, "map");
  q.player = query_param(target, "player");
  q.team = query_param(target, "team");
  q.tag = query_param(target, "tag");
  q.server = query_param(target, "server");
  q.days = static_cast<int>(std::clamp<int64_t>(to_i64(query_param(target, "days"), 0), 0, 36500));
  q.page = static_cast<int>(std::clamp<int64_t>(to_i64(query_param(target, "page"), 1), 1, 100000));
  return q;
}

DemoPage archive_search(const Archive& a, const DemoQuery& q, int64_t now_s) {
  const std::string player = lower(q.player);
  const std::string server = lower(q.server);
  std::vector<const DemoRow*> hits;
  for (const DemoRow& r : a.rows) {
    if (!q.mode.empty() && r.mode != q.mode) continue;
    if (!q.map.empty() && r.map != q.map) continue;
    if (!q.tag.empty() && std::find(r.tags.begin(), r.tags.end(), q.tag) == r.tags.end()) continue;
    if (!server.empty() && !contains_folded(r.server, server)) continue;
    if (q.days > 0 && now_s - r.start_s > int64_t{86400} * q.days) continue;
    if (!q.team.empty() && std::none_of(r.teams.begin(), r.teams.end(),
                                        [&](const DemoTeam& t) { return t.name == q.team; })) {
      continue;
    }
    if (!player.empty() &&
        std::none_of(r.players.begin(), r.players.end(),
                     [&](const DemoPlayer& p) { return contains_folded(p.name, player); })) {
      continue;
    }
    hits.push_back(&r);
  }
  DemoPage page;
  page.total = static_cast<int>(hits.size());
  page.pages = std::max(1, (page.total + kDemosPerPage - 1) / kDemosPerPage);
  page.page = std::min(q.page, page.pages);
  const size_t from = static_cast<size_t>(page.page - 1) * kDemosPerPage;
  const size_t to = std::min(hits.size(), from + kDemosPerPage);
  if (from < to)
    page.rows.assign(hits.begin() + static_cast<std::ptrdiff_t>(from),
                     hits.begin() + static_cast<std::ptrdiff_t>(to));
  return page;
}

}  // namespace ge::hub
