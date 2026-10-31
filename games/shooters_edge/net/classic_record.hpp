// RECORDING A MULTIPLAYER GAME, WITH ITS VOICES: the two recordings the classic protocol's demos
// are (client/demo.hpp reads them back), written by our own client and our own server, each either
// plain or in its voice format (`.qwdv`, `.mvdv`: the voice header, then the base format with the
// voice messages kept).
//
// THE CLIENT'S RECORDING (`ClientRecorder`, on a ClassicClientEnd's `record`): every packet as the
// client received it (the channel's 8-byte header, then its messages), stamped with the network's
// time, and every command the client sent (the command struct and the view's angles, which a
// reader's eye takes). So it holds what this player was sent and nothing more: in voice format,
// the voices the server routed to him (his team's: classic_server.hpp's `voice_route`), and his
// own, which the server never sends back, spliced in as he spoke (a voice message with his own slot
// in the next packet's record). A plain recording leaves every voice message out, so a player that
// knows only the plain format reads it.
//
// THE SERVER'S MULTIVIEW RECORDING (`MultiviewRecorder`, on a ClassicServer's `mvd`): the game as a
// client that sees everything would get it. The signon once (the server data with the server's time
// where a client's would have its slot, the model and sound lists, the baselines, the statics and
// static sounds), then a frame a server frame: the milliseconds since the last frame (at least 1,
// at most 255), every playing player in the recorder's own player form (all his fields, every
// frame), the whole entity list (no deltas: a recording is read from its start), each player's
// stats that changed as a stats record of his, and the frame's events: what the mod sent to
// everyone as an `all` record, to some as a `multiple` record with their mask, to one as a
// `single`. In voice format, every voice message the server relayed, whoever it was routed to, as
// an `all` record: a multiview recording hears every team. `mvd_stop` ends it with a disconnect.
//
// Nothing reads a clock but the server's and the network's, so a session over the simulated
// network records the same bytes every run.
//
// ge:summary Our client's and server's recordings, plain or with voice (.qwdv, .mvdv)
// ge:keywords record recorder client-recording multiview mvd mvdv qwdv voice-format demo-record
// ge:keywords own-voice team-voice signon packet command stats events mvd_frame mvd_stop
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_record_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "shooter/pmove.hpp"
#include "shooters_edge/net/classic_svc.hpp"

namespace shooters_edge::net {

struct ClassicServer;

struct ClientRecorder {
  bool voice = false;          // the voice format: the header, voice kept, own voice spliced in
  std::vector<uint8_t> bytes;  // the file so far
  std::vector<uint8_t> own;    // own voice messages for the next packet's record
  int64_t packets = 0, commands = 0, voice_messages = 0, own_messages = 0, stripped = 0;
};

// A recording begun (the voice header written when `voice`).
void record_start(ClientRecorder& r, bool voice);
// A packet received at `time`: its channel header and its messages, each a span of the payload.
// Voice messages are dropped from a plain recording; own voice is appended to a voice one.
void record_packet(ClientRecorder& r, double time, std::span<const uint8_t> header,
                   std::span<const uint8_t> payload,
                   std::span<const std::pair<size_t, size_t>> messages);
// A command sent at `time`.
void record_command(ClientRecorder& r, double time, const sh::MoveCmd& cmd);
// The recorder's own voice, as the server would have sent it: `slot` the recorder's.
void record_own_voice(ClientRecorder& r, int slot, const ClcMsg& m);

struct MultiviewRecorder {
  bool voice = false;                  // the voice format: the header and every voice relayed
  std::vector<uint8_t> bytes;          // the file so far
  int spawncount = -1;                 // the level the signon was written for (-1: none yet)
  int64_t written_ms = 0;              // the milliseconds the records' clock has summed
  double start_time = 0;               // the server's time at the signon
  int statics = 0, static_sounds = 0;  // written so far
  int32_t stats[32][kStats] = {};
  bool stats_known[32][kStats] = {};
  struct Held {
    int kind = 6;  // all, multiple or single
    int target = 0;
    uint32_t mask = 0;
    std::vector<uint8_t> msg;
  };
  std::vector<Held> held;  // this frame's events, written after its players and entities
  int64_t frames = 0, events = 0, voice_messages = 0, dropped = 0;
};

// A recording begun (the voice header written when `voice`); the signon follows at the next frame.
void mvd_start(MultiviewRecorder& r, bool voice);
// The frame's events, read before the server routes them (classic_frame.cpp's route_events).
void mvd_events(ClassicServer& s, MultiviewRecorder& r);
// A voice message the server relayed (`m.ints`: the slot, the generation, the sequence).
void mvd_voice(MultiviewRecorder& r, const SvcMsg& m);
// The server frame just run: the signon when the level is new, then the frame.
void mvd_frame(ClassicServer& s, MultiviewRecorder& r);
// The recording ended with a disconnect.
void mvd_stop(MultiviewRecorder& r);

}  // namespace shooters_edge::net
