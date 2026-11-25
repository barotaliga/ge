// A CLASSIC GAME'S SOUNDTRACK, KEPT ONE WAY WHEREVER IT CAME FROM. The music of the classic games
// was the audio tracks of their discs (track 1 is the data, the music tracks 2 and on); later
// releases carry the same music as files (Ogg Vorbis in a store's install, FLAC or WAV in a rip).
// Whatever the source (a disc image's audio tracks, a mounted disc, a store's install, a folder of
// files), each track is kept in the game's data folder the same way:
//
//     <game dir>/music/track02.flac ... track11.flac     (the disc's track numbers)
//     <game dir>/music/tracks.txt                         (the manifest: one line a track)
//
// - **The name** is the disc's track number (`track_file`), never the source's own: a file named
//   `track02.ogg`, `Track02.flac` or `02 - title.wav` is track 2; a set numbered from `01` with no
//   "track" in its names is an album's numbering and is moved up one (`track_numbers`), since the
//   disc's first track is its data.
// - **The file** is FLAC (engine/audio/flac.hpp), the samples exactly as the source decodes: a
//   disc's track bit for bit; a store's Ogg Vorbis decoded once (it was lossy before it came here,
//   and nothing more is lost). Tags: TRACKNUMBER, GAME, SOURCE and SOURCE_SHA256.
// - **The identity** is the SHA-256 of the samples as little-endian 16-bit (the bytes a disc image
//   holds for the track, so a track's hash is the same from a `.bin`, a WAV rip or a FLAC rip of
//   the same disc), its length in samples, and the source file's own SHA-256. `kKnownTracks`
//   names the tracks this tree has seen by that hash; any other is kept as the player's own copy,
//   unverified, and its length is checked against the release's disc (`Soundtrack::seconds`).
// - **Which source wins**: a track already kept with the same samples is left alone; one kept
//   from a lower source is replaced by a better one (a disc, then FLAC or WAV, then Ogg Vorbis).
//
// The manifest's lines (space separated `key=value` after the file name, any order):
//
//     track02.flac frames=13436544 rate=44100 channels=2 pcm=<sha256> from=disc source=<sha256>
//     known=-
//
// ge:summary A game's soundtrack kept one way: music/trackNN.flac, a manifest, tracks known by hash
// ge:keywords soundtrack music cd-audio track track02 flac lossless manifest tracks-txt known-track
// ge:keywords pcm-sha256 store_track decode_music ogg vorbis wav track_numbers music-files
// ge:keywords steam gog rerelease import q2_remap_track ogg_remap_tracks mission-pack rogue xatrix
// ge:skill classic audio
// ge:test tests/classic_content_import_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ge::classic::content {

// A game's soundtrack as its release's disc holds it.
struct Soundtrack {
  std::string_view game;  // the content profile's id (classicenhance/profile.hpp)
  int first = 2;          // the disc's first and last music tracks
  int last = 11;
  std::span<const uint16_t>
      seconds;  // each track's length on the disc, from `first`; empty unknown
};

// A track this tree has seen, by the SHA-256 of its samples.
struct KnownTrack {
  std::string_view game;
  int track = 0;
  std::string_view pcm_sha256;
  std::string_view what;  // the release it is from
};

// The soundtrack of the game with content profile `game`; null when it has none.
const Soundtrack* soundtrack_of(std::string_view game);
// The tracks known by hash (empty until a release's tracks are recorded from a real disc).
std::span<const KnownTrack> known_tracks();

// "track02.flac".
std::string track_file(int track);
// The kept file of a track a level or a server asks for: `<game_dir>/music/trackNN.flac`, or ""
// when it is not there (the game was taken in without its music, or the track is not on the
// disc).
std::string track_path(const std::string& game_dir, int track);
// The kept file a track's NAME asks for (`music play <name>`, a server naming its level's music by
// text): a path under `<game_dir>/music` (`\` read as `/`), its extension optional (.flac, .opus,
// .ogg, .mp3 or .wav, dropped), matched without case against every file below that folder that the
// music player decodes (FLAC); among files of one name the first in an A-to-Z walk wins. "" when
// none matches. Files whose names start with a dot are skipped.
std::string music_file(const std::string& game_dir, std::string_view name);
// The track a level plays: its first entity's (the world's) `sounds` key, 0 when it names none.
int level_track(std::string_view entities);
// Quake II's mission packs' disc tracks as the digital release numbers them (`ogg_remap_tracks`):
// in game folder `rogue` tracks 2..11 are 12..21, in `xatrix` 9, 13, 14, 7, 16, 2, 15, 3, 4, 18;
// any other folder or track is itself. The folder is matched by its last name in any case.
int q2_remap_track(std::string_view game_folder, int track);

// Samples as a source decodes them.
struct Samples {
  std::vector<int32_t> data;  // interleaved
  int channels = 2;
  int rate = 44100;
  int bits = 16;
};

// The source's kind, best first: what decides which copy of a track is kept.
enum class MusicSource : uint8_t { disc, flac, wav, ogg };
const char* source_name(MusicSource s);
// The kind of a music file by its extension (any case); false when it is none the store reads.
bool music_source_of(std::string_view path, MusicSource* out);
// A music file's bytes (FLAC, WAV or Ogg Vorbis, by `kind`) decoded; false with the reason.
bool decode_music(std::span<const uint8_t> bytes, MusicSource kind, Samples* out,
                  std::string* error);

// The disc track number of each music file in `paths` (index for index; 0: not a track): the
// number in `trackNN` (any case, any separator), else a leading number, a set of bare numbers from
// 1 moved up one (an album's numbering: the disc's track 1 is its data).
std::vector<int> track_numbers(std::span<const std::string> paths);

struct StoredTrack {
  int track = 0;
  std::string file;  // "track02.flac"
  uint64_t frames = 0;
  int rate = 0, channels = 0, bits = 0;
  std::string pcm_sha256;
  std::string from;           // source_name
  std::string source_sha256;  // the source file's (a disc track: the same as pcm)
  std::string known;          // the known track's release, or "" (the player's own, unverified)
  bool kept = false;          // false: an equal or better copy was already there
  // The length against the release's disc: 0 unknown, 1 within two seconds, -1 off.
  int length_check = 0;
};

// Track `track` of `game`'s soundtrack written into `<game_dir>/music/` from `samples` decoded from
// a `from` source whose own SHA-256 is `source_sha256` (a disc track passes ""), and the manifest
// updated; false with what failed (a folder that cannot be written) and the next step.
bool store_track(const std::string& game_dir, std::string_view game, int track,
                 const Samples& samples, MusicSource from, const std::string& source_sha256,
                 StoredTrack* out, std::string* error);
// The manifest's tracks (empty when there is none).
std::vector<StoredTrack> read_manifest(const std::string& game_dir);

}  // namespace ge::classic::content
