// THE MAPS' STILLS, CACHED (docs/hub.md § Match cards): one picture a map, drawn by the game
// (spec.hpp's `HubStills::render`: Shooter's Edge's from the map's intermission camera in the
// classic look, classic enhance off) the first time a match on it is shown, then kept on disk as
// `<cache>/<game>/<map>-<key>.jpg`. The key is the game's digest of the map's file and of the code
// and settings that draw it (`HubStills::key`), so a changed map or a new build of the renderer
// makes a new still and the old one is deleted; nothing of it is ever committed. A map the content
// does not hold has no still: its card is drawn over the map's hue.
//
// A still is drawn on a worker thread of its own (the drawing takes a second or more in software),
// so a page never waits: until it is made the card shows the hue, and the next refresh the still.
// `make_waiting` draws every asked still on the caller's thread instead (the tests and the labs).
//
// NAMES FROM THE WIRE: a map's name comes from a server's status record, so it is a path only when
// it is a plain file name (letters, digits, `_`, `-`, `+`, `.`; no `..`; at most 64 bytes, and the
// game's a word); any other name is never looked up.
//
// ge:summary The maps' stills a game draws, cached on disk by a key of the map and the renderer
// ge:keywords hub stills still cache intermission-camera map-picture worker key invalidate
// ge:keywords match-card card-look content
// ge:skill net
// ge:test tests/hub_card_test.cpp
#pragma once

#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "hub/api.hpp"
#include "hub/card.hpp"
#include "hub/spec.hpp"

namespace ge::hub {

struct StillsConfig {
  std::string cache;              // the folder the stills are kept in
  std::string content;            // the game's content (HubStills' hooks read it)
  int width = 960, height = 540;  // a still's size, and a card's
  bool thread = true;             // draw on a worker (false: only `make_waiting` draws)
};

// Whether `name` may name a file (a map's or a game's).
bool still_name_ok(std::string_view name);

class Stills {
 public:
  Stills(const HubStills& game, StillsConfig config);
  ~Stills();
  Stills(const Stills&) = delete;
  Stills& operator=(const Stills&) = delete;

  const StillsConfig& config() const { return config_; }
  // The still of `map` of `game` when made (null until then, and for a map the content lacks); a
  // first ask queues it.
  std::shared_ptr<const CardPicture> still(std::string_view game, std::string_view map) const;
  // The still's file as it is kept (empty until made).
  std::vector<uint8_t> still_file(std::string_view game, std::string_view map) const;
  // The board's look for `game` (read once); null when the game has none.
  const CardLook* look(std::string_view game) const;
  // Every still asked for and not made, drawn now on this thread. The count drawn.
  int make_waiting();
  // Stills drawn and refused since the start (a status line's).
  int made() const;
  int refused() const;

 private:
  struct Entry;
  void work();
  bool make(Entry& e, const std::string& game, const std::string& map);

  HubStills game_;
  StillsConfig config_;
  mutable std::mutex mu_;
  mutable std::condition_variable wake_;
  mutable std::map<std::string, std::shared_ptr<Entry>> entries_;  // "<game>/<map>"
  mutable std::map<std::string, std::unique_ptr<CardLook>> looks_;
  mutable std::vector<std::string> queue_;
  mutable std::thread worker_;
  bool stop_ = false;
  int made_ = 0, refused_ = 0;
};

// The `/card/<address>.jpg` and `/still/<game>/<map>.jpg` answers (pages.cpp calls it): false when
// `path` is neither, or names no node, no game with a look, or no still made.
bool stills_answer(const Poller& p, const HubSpec& spec, const Stills* stills,
                   std::string_view path, Answer& out);

}  // namespace ge::hub
