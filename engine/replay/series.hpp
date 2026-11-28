// A DEMO SERIES (`.geds`; docs/tv.md § A demo series): replay files (`.ged`, stream.hpp) kept in
// one file in the order they play: a clan fight's maps and rounds, a campaign run's levels. Each
// entry is a whole replay file, unchanged, so splitting a series gives back the very files merged
// into it, byte for byte.
//
//   header  magic "geseries", u16 version 1 (little-endian), the series' meta (key and value text
//           pairs: kind, title, lineup...)
//   entry*  sync "GSEN", the entry's name, its meta (the replay file's header meta and its final
//           meta, read once when it was added, so a list needs no replay file opened), the replay
//           file's bytes (a size, then the bytes), a 32-bit FNV-1a (little-endian) of the entry
//           from its name to its last byte
//
// Numbers are LEB128 varints unless a size is given, as in the replay file. There is no end
// marker: an entry is added by writing it at the file's end, so a series kept across a match
// night grows one finished match at a time. A reader takes every whole entry whose sum holds and
// says where the rest was cut or damaged.
//
// ge:summary Demo series (.geds): replay files kept whole in play order; add, read, merge
// ge:keywords demo-series geds series clan-fight campaign-run entries split merge playlist
// ge:keywords series_begin series_add read_series is_series series_merge ged_meta
// ge:skill replay
// ge:test tests/replay_series_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "replay/stream.hpp"

namespace ge::replay {

inline constexpr int kSeriesVersion = 1;

// The series' meta words a writer sets (each optional): what it holds, for people and a list.
inline constexpr std::string_view kSeriesKind = "kind";      // "matches", "campaign"
inline constexpr std::string_view kSeriesTitle = "title";    // "Slackers vs Hell's Fire"
inline constexpr std::string_view kSeriesLineup = "lineup";  // the sides and their players

// One entry as read: `bytes` and `name` view the series' bytes (keep them alive).
struct SeriesEntry {
  std::string name;  // no extension: a file split out is `<name>.ged`
  Meta meta;
  std::span<const uint8_t> bytes;  // the replay file, whole
  size_t offset = 0;               // of the entry's sync, from the series' start
};

struct SeriesView {
  int version = kSeriesVersion;
  Meta meta;
  std::vector<SeriesEntry> entries;  // in play order
  std::string cut;  // "": every byte read; else where and why the rest was not taken
};

// The header written onto `out` (cleared first).
void series_begin(std::vector<uint8_t>& out, const Meta& meta);
// An entry for the replay file `ged` written at `out`'s end, its meta read from the file
// (`ged_meta`). False with the reason when `ged` is not a version 2 replay file.
bool series_add(std::vector<uint8_t>& out, std::string_view name, std::span<const uint8_t> ged,
                std::string& error);
// The entry's bytes alone (to append to a series file on disk).
bool series_entry(std::string_view name, std::span<const uint8_t> ged, std::vector<uint8_t>& out,
                  std::string& error);
// The header and every whole entry. False (with `error`) when the bytes are not a series, are of
// a newer version, or the header is damaged; an entry cut short or damaged ends the list with
// `out.cut` saying where.
bool read_series(std::span<const uint8_t> bytes, SeriesView& out, std::string& error);
// Whether `bytes` opens as a demo series.
bool is_series(std::span<const uint8_t> bytes);

// A replay file's header meta, then its final meta (the trailer's, when it has one) less the
// match report's packed bytes (`report`, which a hub reads from the replay file itself).
Meta ged_meta(std::span<const uint8_t> ged);

// One input to a merge: a replay file (one entry, named `name`) or a whole series (its entries,
// their names kept).
struct SeriesPart {
  std::string name;
  std::span<const uint8_t> bytes;
};
// The parts' entries in order as one series with `meta` (empty: the first series part's meta).
// False with the part and the reason when one is neither a replay file nor a whole series.
bool series_merge(std::span<const SeriesPart> parts, const Meta& meta, std::vector<uint8_t>& out,
                  std::string& error);

// A meta value by key ("" when absent; the last when repeated).
std::string meta_get(const Meta& m, std::string_view key);

}  // namespace ge::replay
