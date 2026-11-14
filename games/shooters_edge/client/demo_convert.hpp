// A RECORDED GAME CONVERTED FROM ONE DEMO FORMAT TO ANOTHER, AS IT IS READ: a stream that takes
// the source format's bytes in whatever pieces they arrive (a whole file, a growing file, a live
// stream) and gives back the target format's bytes frame by frame, so a broadcast can serve a
// recording in a format its viewers play (the TV's `--demo`) and `demos convert <in> <out>`
// (cli/spec.cpp) writes a file. demo.hpp's formats are the formats here.
//
// THE CALLS: `convert_begin` names the two formats and the options; `convert_feed` hands over the
// next source bytes; `convert_take` appends to `out` the target bytes of every source frame
// completed so far, always whole records or blocks of the target (never half of one), the
// level's signon first; `convert_end` says the source has ended, so the last frame is given and,
// unless `ConvertOptions::no_end`, the target's end (a disconnect). A source that does not read
// stops the stream: `convert_take` returns false with the reason in `error`,
// and what was taken before stays good.
//
// EXACT WHERE THE TARGET HOLDS IT. What both formats carry goes across unchanged: coordinates
// and angles keep their bytes, the lists, the baselines, the statics, the light styles, the
// sounds, the prints, the centre messages, the frags, the names and the colours. What the target
// has no place for is dropped and counted in `ConvertNotes::dropped`, one line a kind of message
// with its count, so the caller can say what was lost; what the target holds in another form is
// rewritten (`ConvertNotes::rewritten`). Which pairs exist: `convert_supported`.
//
// THE PAIRS:
// - SINGLE (`.dem`, protocol 15) TO MULTIVIEW (`.mvd`): demo_convert_single.cpp. The recorder's
//   own player is slot 0 (or his slot in a game of several), the other players are the slots
//   their entities are, every other entity is renumbered past the 32 player slots, a frame a
//   block that carries the server's time.
// - SINGLE TO CLIENT (the client recording, protocol 28 with no extension): the same reading of
//   the source (demo_convert_single.cpp), written by demo_convert_client.hpp for the oldest
//   clients: the recorder's view as the recording's player, whole entity lists of at most 64, a
//   command record a frame with the view's angles. The default for a recording of one view.
// - MULTIVIEW TO CLIENT (one player's view): demo_convert_multiview.cpp, written the same way.
//   The view is `ConvertOptions::pov` or `convert_set_pov`, chosen or changed at any frame: the
//   whole recording is read whether or not anyone watches, so a view starts with a whole signon
//   and a whole frame wherever it starts (a TV converts a view only while someone watches it).
// - CLIENT TO MULTIVIEW: demo_convert_recorded.cpp. A player's own recording in the form every
//   multiview player reads: every player he saw, his view from his commands, his status as his
//   stats, his own messages (a view set, a kick, damage) to him alone.
// - SEQUEL TO SEQUEL (`.dm2` to `.dm2`): demo_convert_sequel.cpp. Any protocol the reader takes
//   (26 to 34) written as protocol 34, which every stock client and the TV's viewers take; a
//   protocol 34 recording comes back byte for byte.
//
// ge:summary Demo formats converted as they are read: a stream of source bytes in, target out
// ge:keywords demo-convert convert conversion transcode dem mvd client-recording dm2
// ge:keywords stream on-the-fly tv
// ge:keywords convert_begin convert_feed convert_take convert_end convert-notes dropped
// ge:keywords single-to-multiview multiview-to-client convert_set_pov pov format-name
// ge:skill shooter replay
// ge:test tests/shooters_edge_demo_convert_test.cpp
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::client {

struct ConvertOptions {
  // The game folder the target's server data names (the multiplayer formats'): the folder a
  // player's client opens the recording's files from.
  std::string game = "id1";
  // Leave out the target's end (its disconnect): a broadcast that loops a recording starts it
  // again rather than ending it.
  bool no_end = false;
  // The view a conversion to one player's view rides (a multiview source's): a slot, -1 the
  // lowest slot playing, -2 none yet (nothing is written until `convert_set_pov` names one).
  int pov = -1;
};

// What a conversion changed: each kind of message the target has no place for, with how many
// were dropped, and each rewritten into another form.
struct ConvertNotes {
  std::map<std::string, int64_t> dropped, rewritten;
  int64_t frames = 0;  // source frames converted
  int64_t levels = 0;  // levels started
};

struct DemoConvert;

// One pair's state (demo_convert_*.cpp): `step` reads what `c.in` holds of whole source frames
// (consuming them) and appends the target's bytes to `out`; at `c.ended` it gives the rest and the
// end. False with the reason and the next step in `error`.
struct ConvertStream {
  virtual ~ConvertStream() = default;
  virtual bool step(DemoConvert& c, std::vector<uint8_t>& out, std::string& error) = 0;
};

struct DemoConvert {
  DemoFormat from = DemoFormat::single, to = DemoFormat::multiview;
  ConvertOptions options;
  ConvertNotes notes;
  std::vector<uint8_t> in;  // source bytes fed and not yet read
  bool ended = false;       // convert_end was called
  bool failed = false;
  std::string error;
  std::unique_ptr<ConvertStream> stream;
};

// Whether `from` converts to `to`.
bool convert_supported(DemoFormat from, DemoFormat to);

// A stream from `from` to `to`. False with the reason when the pair is not one there is.
bool convert_begin(DemoConvert& c, DemoFormat from, DemoFormat to, const ConvertOptions& options,
                   std::string& error);
// The source's next bytes, any length.
void convert_feed(DemoConvert& c, std::span<const uint8_t> bytes);
// The source has ended: the next take gives the last frame and the target's end.
void convert_end(DemoConvert& c);
// The target's bytes of every source frame completed since the last take, appended to `out`.
// False with the reason in `error` when the source does not read.
bool convert_take(DemoConvert& c, std::vector<uint8_t>& out, std::string& error);
// The view a conversion to one player's view rides from the next frame on (`ConvertOptions::pov`):
// a view chosen or changed partway starts with a whole signon and a whole frame.
void convert_set_pov(DemoConvert& c, int slot);

// A whole recording converted at once: the source detected (open_demo's formats; packed and
// archived files unpacked first), then converted to `to`. False with the reason.
bool convert_demo(std::vector<uint8_t> bytes, DemoFormat to, const ConvertOptions& options,
                  std::vector<uint8_t>& out, ConvertNotes& notes, std::string& error);

// The format a file's name asks for (`.dem` single, the client recording's, `.mvd` multiview,
// `.dm2` sequel; a packed name's `z` and a `.gz` taken off first). False when it names none.
bool format_of_name(const std::string& path, DemoFormat& out);
// The format's name for a player: "single-player demo (.dem)", ...
std::string format_words(DemoFormat f);

// --- the pairs (demo_convert_*.cpp) ---
std::unique_ptr<ConvertStream> single_to_multiview_stream();
std::unique_ptr<ConvertStream> single_to_client_stream();
std::unique_ptr<ConvertStream> sequel_to_34_stream();
std::unique_ptr<ConvertStream> multiview_to_client_stream();
std::unique_ptr<ConvertStream> client_to_multiview_stream();

}  // namespace shooters_edge::client
