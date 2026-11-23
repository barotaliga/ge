// A GAME'S DISC, READ FROM ITS IMAGE: the files of its data track and the sound of its audio
// tracks, without mounting anything. The images read:
//
// - **`.iso`** (and `.img` or `.bin` holding 2048-byte sectors): one data track, its sectors as the
//   file system stores them.
// - **`.cue` with its `.bin` files**: the sheet's `FILE`, `TRACK nn <mode>` and `INDEX 01 mm:ss:ff`
//   lines (`PREGAP`, `INDEX 00` and the rest are read past), any number of tracks and files. A
//   data track of raw 2352-byte sectors (`MODE1/2352`, `MODE2/2352`) has its 2048 bytes of user
//   data at 16 and 24; a `MODE1/2048` track is plain. An `AUDIO` track is 2352-byte sectors of
//   44.1 kHz 16-bit stereo, little-endian, as an image stores them. A track runs from its `INDEX
//   01` to the next track's `INDEX 01` in the same file (a gap goes with the track before it, as
//   rips keep it), the last one to the end of its file.
// - **a `.bin`, `.img` or `.mdf` alone** (no sheet): raw sectors when the first carries the sync
//   pattern (its mode byte placing the user data), 2448 bytes each when the second sector's sync
//   falls there (96 bytes of subchannel after each, as a disc copier's `.mdf` keeps them), else
//   2352; 2048-byte ones when there is no sync; one data track.
//
// The file system is ISO 9660 on the first data track: the Joliet tree (UCS-2 names, any length)
// when the disc has one, else the primary one (its `;1` and trailing `.` dropped). Names are kept
// as stored and only matched, never used as paths on disk by this reader. A file is read by its
// extent; a record or extent outside the track is refused with its path.
//
// The image is read in place with seeks: a 650 MB disc is never loaded whole.
//
// ge:summary Reads a disc image (.iso, .cue with .bin, raw .bin/.img): ISO 9660 files, audio tracks
// ge:keywords disc iso iso9660 joliet cue bin img image cd cd-rom cdda audio-track track mode1
// ge:keywords mode2 raw-sector 2352 2048 2448 mdf subchannel sync data-track open_disc read_file read_audio
// ge:keywords write_iso write_cue_bin fixture
// ge:skill vfs
// ge:test tests/vfs_disc_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ge::vfs {

enum class TrackKind : uint8_t { data, audio };

struct DiscTrack {
  int number = 0;  // as the sheet numbers it (1 for an image with no sheet)
  TrackKind kind = TrackKind::data;
  int file = 0;            // index into Disc::image_files
  uint64_t start = 0;      // byte offset of its first sector in that file
  uint32_t sector = 2048;  // bytes a sector takes in the file: 2048, 2352 or 2448
  uint32_t user = 0;       // where a data sector's 2048 bytes start inside it (0, 16 or 24)
  uint32_t sectors = 0;    // its length
};

struct DiscFile {
  std::string path;     // '/' separated from the root, as the disc names it
  uint32_t extent = 0;  // the first logical sector, in the data track
  uint32_t size = 0;
};

class Disc {
 public:
  Disc();
  ~Disc();
  Disc(Disc&&) noexcept;
  Disc& operator=(Disc&&) noexcept;

  std::vector<std::string> image_files;  // the image's own files (the sheet's `.bin` files)
  std::vector<DiscTrack> tracks;
  std::vector<DiscFile> files;  // every regular file of the data track, in the disc's order
  std::string volume;           // the volume's name, as the file system gives it
  bool joliet = false;          // names from the Joliet tree

  // The first file whose path is `path`, any case; null when there is none.
  const DiscFile* find(const std::string& path) const;
  // The bytes of `f`; false with the path and the reason when they run outside the track.
  bool read_file(const DiscFile& f, std::vector<uint8_t>* out, std::string* error) const;
  // Track `t`'s samples (interleaved stereo, 44.1 kHz) when it is audio; false otherwise.
  bool read_audio(const DiscTrack& t, std::vector<int16_t>* out, std::string* error) const;

  struct Impl;  // the open image files (disc.cpp)
  std::unique_ptr<Impl> impl_;
};

// Whether `path` names an image this reader opens by its extension (.iso, .cue, .bin, .img, .mdf).
bool is_disc_image(const std::string& path);
// The image at `path` opened: its tracks and the files of its first data track; false with what is
// wrong (the sheet's line, the missing `.bin`, no file system) and the next step in `*error`.
bool open_disc(const std::string& path, Disc* out, std::string* error);

// FIXTURES, for tests: an ISO 9660 image of `files` (path, bytes; directories made from the paths,
// names kept to the primary tree's upper-case 8.3 when `joliet` is off, the Joliet tree with the
// names as given when on), 2048-byte sectors.
std::vector<uint8_t> write_iso(
    const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files, bool joliet,
    const std::string& volume = "GE_DISC");
// The same data track as raw `MODE1/2352` sectors (sync, header, user data; no error codes).
std::vector<uint8_t> raw_sectors(const std::vector<uint8_t>& iso);

}  // namespace ge::vfs
