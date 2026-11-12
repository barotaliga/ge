// THE HUB'S DEMO ARCHIVE (docs/hub.md § The archive): replay files (version 2, `.ger`,
// engine/replay/stream.hpp) kept by the SHA-256 of their bytes under a three-character fan-out
// (`d/abc/abc….ger`), so a demo has one name everywhere, the same demo from two places is kept
// once, and every file is immutable. A demo's row is what its header and trailer say (the hub's
// meta words, below), never what a file name says. The operator's own facts about a demo (when it
// came, its tags) are the one file the archive writes beside them, `archive.tsv`.
//
// THE HUB'S META WORDS: a recorder that wants its demos found writes these keys in the header's
// meta (a key may repeat): `map`, `mode`, `server` (its name), `start` (Unix seconds), `title`,
// `team` (a team's name), `player` ("name" or "name<TAB>team"), `tag`; and in the trailer's final
// meta: `duration_ms`, `score` ("name<TAB>score"), `team_score` ("team<TAB>score"). Anything else
// is the game's and kept unread.
//
// Files come in through `archive_add` (bytes) or the inbox: every `.ger` an operator's recorder
// drops in `<dir>/inbox/` is taken by `archive_ingest`. Search is the Demos page's: by mode, map,
// player (any part of a name, any case), team, tag, server and age, newest first, 15 a page.
// Retention drops the oldest untagged demos past a size or an age. The index lives in memory and
// is rebuilt from the files when the hub starts (docs/hub.md § Decisions).
//
// ge:summary The hub's demo archive: replay files by SHA-256, rows from their meta, search, inbox
// ge:keywords hub archive demo demos sha256 fan-out search player map mode tag retention inbox
// ge:keywords replay-v2 meta-words index page
// ge:skill net
// ge:test tests/hub_archive_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hub/api.hpp"

namespace ge::hub {

constexpr int kDemosPerPage = 15;

struct DemoPlayer {
  std::string name, team;
  int32_t score = 0;
  bool scored = false;  // the trailer gave a score
};

struct DemoTeam {
  std::string name;
  int32_t score = 0;
  bool scored = false;
};

struct DemoRow {
  std::string sha;  // 64 lower-case hex characters
  std::string game;
  int32_t game_version = 0;
  std::string map, mode, server, title;
  int64_t start_s = 0;       // the header's `start`; the time it came when the header has none
  int64_t duration_ms = -1;  // -1: no trailer (a live stream cut short)
  int64_t size = 0;
  int64_t added_s = 0;
  std::vector<DemoPlayer> players;
  std::vector<DemoTeam> teams;
  std::vector<std::string> tags;  // the header's and the operator's
  bool radar = false;             // the file has a radar track: the hub can draw its map
};

struct ArchiveStats {
  uint64_t added = 0, duplicates = 0, refused = 0, pruned = 0;
};

struct Archive {
  std::string dir;            // "" when the hub keeps no archive
  std::vector<DemoRow> rows;  // newest first (start, then the hash)
  int64_t bytes = 0;
  ArchiveStats stats;
};

// The archive in `dir` opened (made when missing), its rows read from the files under `d/`.
// False with the reason in `error` (the directory cannot be made or read).
bool archive_open(Archive& a, const std::string& dir, std::string& error);

// What `archive_add` did.
enum class AddResult : uint8_t { added, duplicate, refused };

// One replay file's bytes kept: hashed, read, written under `d/` and indexed. A file that is not a
// version 2 replay is refused with the reason in `error`; one already kept is a duplicate (its
// operator tags merged). `row` (when given) is its row.
AddResult archive_add(Archive& a, std::span<const uint8_t> bytes, int64_t now_s,
                      const std::vector<std::string>& tags, std::string& error,
                      DemoRow* row = nullptr);

// Every `.ger` in `<dir>/inbox/` added and removed from there (a refused one is moved to
// `<dir>/inbox/refused/`). The number taken.
int archive_ingest(Archive& a, int64_t now_s, std::vector<std::string>& errors);

// The oldest untagged demos dropped while the archive is over `max_bytes` (0: no cap) or older
// than `max_age_s` (0: no limit). The number dropped.
int archive_prune(Archive& a, int64_t max_bytes, int64_t max_age_s, int64_t now_s);

// The row for a hash, or null.
const DemoRow* archive_find(const Archive& a, std::string_view sha);
// A demo's file under the archive, relative to its directory: "d/abc/abc….ger".
std::string archive_path(std::string_view sha);
// A row read from a replay file's bytes alone (no hash, no times); false with `error`.
bool demo_row(std::span<const uint8_t> bytes, DemoRow& out, std::string& error);

struct DemoQuery {
  std::string mode, map, player, team, tag, server;
  int days = 0;  // 0: any age
  int page = 1;
};

struct DemoPage {
  std::vector<const DemoRow*> rows;
  int total = 0, page = 1, pages = 1;
};

DemoQuery demo_query(std::string_view target);  // from a request's query string
DemoPage archive_search(const Archive& a, const DemoQuery& q, int64_t now_s);

// THE ARCHIVE'S PATHS (archive_api.cpp): `/v1/demos` (the search as JSON: rows, total, page,
// pages), `/v1/demos/{sha256}` (one demo's summary), `/d/{abc}/{sha256}.ger` (the file, cached a
// year) and `/d/{abc}/{sha256}.json` (its summary). True when `target` is one of them, with the
// answer in `out`; a hub with no archive (`a` null) answers them 404 with the flag to pass.
bool archive_answer(const Archive* a, std::string_view target, int64_t now_s, Answer& out);
std::string demo_json(const DemoRow& r);

}  // namespace ge::hub
