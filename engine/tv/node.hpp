// A TV NODE (docs/tv.md § TV): a match's replay file stream taken from a source, held back by a
// delay in game time, and fanned out to viewers, each of which may be another TV node (a relay).
// No socket here: the caller moves the messages (net_link.hpp runs a node over engine/net's nodes,
// on UDP, TCP or the simulated network).
//
// THE SOURCE is the replay file version 2's bytes as they are written (engine/replay/stream.hpp): a
// game's server appending blocks, a file read out, or the stream another TV node sends (a relay).
// `tv_feed` takes them in any pieces and splits them into the header and whole blocks; a block
// that fails its sum stops the source with the reason in `error`.
//
// THE DELAY is in game time: the first block anchors the source's clock to the node's, and a
// block goes out once its start plus the delay has come on the node's clock, so a source that
// runs ahead (a file read out) is paced to the match's own speed. `delay_ms` 0 sends a block as
// soon as it is whole. A source that falls behind is not held back twice: a late block goes out as
// it arrives.
//
// A VIEWER says hello (its name and whether it relays), gets the header, then the blocks from the
// newest keyframe block already out (a late joiner starts there: a keyframe is where the game's
// models start over), then each block as it goes out, then `ended` when the source has finished
// and everything is out. A viewer whose link refuses a message (its window is full) keeps its
// place and is sent the rest later, so a slow viewer slows no one else. Blocks no viewer still
// needs and older than the newest keyframe out are let go.
//
// CHAT goes up (a relay passes its viewers' lines on to its own source) and every line the first
// node sees is sent down to every viewer. THE VIEWER LIST: each relay reports how many watch under
// it, so the first node knows the whole tree's count, and every node tells its viewers that count.
//
// ge:summary A TV node: a replay stream from a source, held by a delay, fanned out to viewers
// ge:keywords tv television broadcast spectate relay fan-out delay catch-up late-joiner keyframe
// ge:keywords viewer chat viewer-count tv_feed tv_join tv_receive tv_update watch
// ge:skill replay net
// ge:test tests/tv_node_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "replay/stream.hpp"

namespace ge::tv {

inline constexpr uint8_t kWireVersion = 1;

// THE WIRE: one message a reliable, ordered link carries; the first byte is its kind.
enum class Msg : uint8_t {
  hello = 1,    // up: the wire version, 1 when the viewer relays, its name
  header = 2,   // down: the replay file's header bytes
  block = 3,    // down: a piece of a block's bytes as the file holds them; the byte after the
                // kind is 1 while more pieces follow
  chat = 4,     // up: the line; down: who said it and the line
  viewers = 5,  // down: how many watch the whole tree; up (from a relay): how many under it
  ended = 6,    // down: the source finished and every block is out
  refused = 7,  // down: why the viewer was turned away
};

// The largest piece of a block one message carries: under engine/net's biggest reliable message.
inline constexpr size_t kPiece = size_t{60} * 1024;

struct Config {
  std::string name = "tv";  // in the chat lines it passes up for its viewers
  int64_t delay_ms = 0;     // game time held back
  int max_viewers = 0;      // 0: no cap
  std::string game;         // "": any; else a source of another game is refused
  bool relay = false;       // its source is another node: chat goes up, not down
};

struct Held {
  std::vector<uint8_t> bytes;  // the block as the file holds it
  int64_t time_ms = 0;         // its start, game time
  bool keyframe = false;
};

struct Viewer {
  int link = -1;
  std::string name;
  bool relay = false;
  bool greeted = false;  // its hello came
  bool started = false;  // its header went
  int64_t next = -1;     // the next block (by number) it is sent; -1: not placed yet
  size_t piece = 0;      // of that block, the bytes already sent
  int64_t under = 0;     // a relay's viewers under it
  int64_t told = -1;     // the viewer count it was last sent
  bool ended = false;    // `ended` went
};

struct ChatLine {
  std::string who, text;
};

struct Node {
  Config config;
  // The source.
  std::vector<uint8_t> pending;  // bytes not yet split into the header or a whole block
  bool have_header = false;
  std::vector<uint8_t> header;
  replay::StreamHeader parsed;
  bool finished = false;  // the source ended (its trailer came, or `tv_end`)
  std::string error;      // the source stopped: why
  std::deque<Held> blocks;
  int64_t first = 0;     // the number of `blocks.front()`
  int64_t released = 0;  // blocks numbered below this are out
  bool anchored = false;
  int64_t anchor_game_ms = 0, anchor_clock_ms = 0;
  // Viewers.
  std::vector<Viewer> viewers;
  std::vector<ChatLine> chat;  // lines that reached this node, oldest first; the caller clears
  std::vector<ChatLine> down;  // lines to send every viewer (the first node's echo)
  std::vector<ChatLine> up;    // lines a relay passes to its own source (net_link sends them)
  int64_t tree = -1;           // the whole tree's count a relay heard from above; -1: count its own
  // Counters.
  int64_t bytes_in = 0, bytes_out = 0, blocks_in = 0;
};

// The source's bytes, in any pieces; `now_ms` is the node's clock. False once the source failed.
bool tv_feed(Node& node, std::span<const uint8_t> bytes, int64_t now_ms);
// The source ended: no more blocks; viewers get `ended` once everything is out.
void tv_end(Node& node);

void tv_join(Node& node, int link);
void tv_leave(Node& node, int link);
// A viewer's message. False when the viewer broke the wire (the caller drops the link).
bool tv_receive(Node& node, int link, std::span<const uint8_t> msg);
// Send what is due: `send(link, message)` returns false when the link will take no more now.
using Send = std::function<bool(int link, std::span<const uint8_t> msg)>;
void tv_update(Node& node, int64_t now_ms, const Send& send);
// The number of blocks out that a viewer joining now is sent first (from the newest keyframe).
int64_t tv_catch_up_blocks(const Node& node);
// Everyone watching through this node: its viewers that do not relay and every relay's count.
int64_t tv_viewer_count(const Node& node);

// THE VIEWER'S END: the messages a node sends turned back into the replay file's bytes.
struct Watch {
  bool keep = true;             // keep the stream (a relay passes it on instead)
  std::vector<uint8_t> stream;  // the header and every whole block so far: a replay file
  std::vector<uint8_t> piece;   // a block's pieces joined so far
  bool have_header = false;
  bool ended = false;
  int64_t blocks = 0;
  int64_t viewers = 0;  // the tree's count, as the node last said
  int64_t bytes = 0;    // every message's bytes taken in
  std::vector<ChatLine> chat;
  std::string refused;  // why the node turned this viewer away
  std::string error;    // a message that broke the wire
};

std::vector<uint8_t> tv_hello(std::string_view name, bool relay);
std::vector<uint8_t> tv_chat(std::string_view who, std::string_view text);
std::vector<uint8_t> tv_count(int64_t viewers);
// A node's message taken in; `fresh` (may be null) gets the stream bytes it added (the header or
// a whole block), which a relay feeds to its own node. False when the message broke the wire.
bool watch_receive(Watch& w, std::span<const uint8_t> msg, std::vector<uint8_t>* fresh = nullptr);

}  // namespace ge::tv
