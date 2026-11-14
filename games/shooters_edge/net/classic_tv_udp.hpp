// OUR TV NODE'S FACE FOR CLASSIC CLIENTS (docs/tv.md § Classic clients at the TV): a match as one
// player's client recording (each record a server packet: the channel's header and its messages)
// served to any multiplayer client of the classic game over plain protocol 28, so a player watches
// through our TV with the client they already have, the oldest one too: the client connects to
// the TV as to a server, as a spectator. The packets come from wherever the caller takes them: a
// recording played (classic_tv_recording.hpp), or a multiview recording turned to each player's
// view a viewer asks for (views on request, below).
//
// PLAIN PROTOCOL 28 OUT, whatever came in: the server data names no extension, so a viewer's
// client is never sent a message it may not know. Players and entities are read under the
// source's agreed extensions and written again plain; every entity list goes out whole against
// the baselines (svc 47), never as a delta, so a viewer's client needs no frame of its own to
// read the next one and a lost packet costs it nothing but that packet. Entities past 511 or with a
// model past 255 and the extensions' own messages (voice, named effects, the stat strings) are left
// out; an extended sound or baseline goes as its plain form when it fits.
//
// WHAT THE FACE KEEPS of the level, so a viewer starts at any moment: the server data, the server
// info, the sound and model lists, the static entities and sounds, the baselines, the light styles,
// each player's user info, frags, ping, loss and time in, the recorder's stats, and the newest
// frame (the players and the entities). The recorder's own view is left out (its damage flashes,
// kicks, turns, maxspeed): the viewer flies as a spectator, and its client follows a player with
// its own tracking keys.
//
// A VIEWER asks for a challenge (`c<number>`, no extensions offered) and connects (`connect 28
// <qport> <challenge> "<userinfo>"`, the password in `password` when one is set; a refusal is a
// print a player reads), and is answered `j`. Then it walks the signon as from a server, each step
// on its echo: `new` (the server data, its own slot a spectator's, the music track, the server
// info), `soundlist` and `modellist` in pages, `prespawn` in pages (the statics, the baselines, the
// static sounds), `spawn` (every player's line, the light styles, the stats, then `skins`) and
// `begin`. From then on every packet it is sent carries the newest frame and the sounds and effects
// since the last one; what changes the level's state (prints, frags, user info, light styles,
// stats) goes on the reliable channel in order. A viewer silent for `timeout` seconds, or that
// falls behind by more than `most_behind` reliable messages, is dropped.
//
// A NEW LEVEL (the server data again): the face starts over and every viewer is sent `changing`
// and `reconnect`, so its client keeps its connection and asks for the new signon.
//
// OUT OF BAND it answers `getchallenge`, `connect`, `status` (the source's server info under the
// TV's name, and the recorded game's players) and `ping`.
//
// ge:summary Our TV node's face for classic clients: one player's packets served in plain 28
// ge:keywords tv-node classic-face spectate viewer protocol-28 plain-protocol signon
// ge:keywords client-recording views-on-request udp_tv_wanted udp_tv_watching udp_tv_clear
// ge:keywords old-clients full-entity-list udp_tv_start udp_tv_packet udp_tv_frame udp_tv_stop
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_tv_udp_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <map>
#include <span>
#include <string>
#include <vector>

#include "net/transport.hpp"
#include "shooters_edge/net/classic_channel.hpp"
#include "shooters_edge/net/classic_delta.hpp"
#include "shooters_edge/net/classic_ext.hpp"

namespace shooters_edge::net {

namespace gn = ge::net;

struct UdpTvConfig {
  std::string name = "Shooter's Edge TV";
  int max_viewers = 32;
  std::string password;      // "": none asked
  double timeout = 30;       // s of silence before a viewer is dropped
  size_t most_behind = 256;  // reliable messages waiting for a viewer before it is dropped
};

enum class UdpViewerStage : uint8_t { free, connected, active };

struct UdpViewer {
  UdpViewerStage stage = UdpViewerStage::free;
  gn::Address address;
  uint16_t qport = 0;
  std::string name;
  Channel ch;
  double heard = 0, sent = 0;
  std::deque<std::vector<uint8_t>> queue;  // reliable messages waiting, in order
  bool asked = false;                      // it said `new` before the level came
  int stream = 0;                          // the view it watches
  bool chose = false;                      // it asked for a player of its own (ptrack, fire)
  bool fire = false;                       // its fire button held (a press is the next view)
  int want = -1;       // a player it asked for whose view no stream has yet (views on request)
  int64_t frames = 0;  // packets sent with a frame
};

// One player's line of the recorded game.
struct UdpTvSlot {
  bool used = false;
  int32_t userid = 0;
  std::string info;
  int frags = 0, ping = 0, loss = 0;
  float in_for = 0;  // s since it entered (updateentertime)
};

// One player's view: what the face keeps of its packets.
struct UdpTvStream {
  // The level as the source stands now.
  bool level = false;
  int32_t count = 0;  // the server data's spawn count, the face's `count_raise` added
  int pov = -1;       // the slot whose view it is (the server data's player)
  std::string gamedir, title, serverinfo;
  float movevars[10] = {};
  Ext ext;  // the source's agreed extensions (its server data's, its server info's z set)
  std::vector<std::string> sounds, models;                   // [0] ""
  std::vector<std::vector<uint8_t>> statics, static_sounds;  // each message, plain
  std::vector<EntityState> baselines;                        // by number (512)
  std::array<std::string, 64> lightstyles;
  std::array<UdpTvSlot, 32> slots;
  int32_t stats[32] = {};
  int cdtrack = 0;
  // The frames: the entity lists by the source packet's sequence (mod 64), the newest frame and
  // the unreliable messages since the last packet out, all plain.
  std::array<std::vector<EntityState>, 64> ring;
  std::array<uint32_t, 64> ring_seq{};
  std::vector<uint8_t> frame, events;
  bool fresh = false;  // a frame came since the last packet out
  sh::Vec3f view;      // the player's view's angles, degrees (udp_tv_view)
  bool has_view = false;
  int64_t packets = 0, frames = 0, levels = 0;
};

struct ClassicTvUdp {
  UdpTvConfig cfg;
  gn::Transport* transport = nullptr;
  double now = 0;
  // One stream a player's view (a client recording has one; a multiview match turned to every
  // player's view has one each). A viewer watches one: the one the TV follows (`follow`) until
  // its client asks for a player of its own (`ptrack <slot>`, a spectator's track keys), then that
  // player's while the face has a stream of his view.
  std::vector<UdpTvStream> streams;
  int follow = 0;
  // VIEWS ON REQUEST (a multiview match, classic_tv_recording.hpp): a viewer may ask for any
  // player's view, with its track keys or fire, though no stream carries it yet; the source reads
  // what is wanted (`udp_tv_wanted`) and turns a stream to it, and the viewer moves when it comes.
  bool views_on_request = false;
  int32_t count_raise = 0;  // added to every count read (a recording's rounds)
  int viewer_slot = 31;     // the slot every viewer is told is its own (one no player holds)
  std::array<UdpViewer, 32> viewers;
  std::map<uint64_t, int32_t> challenges;  // by host
  uint32_t next_challenge = 0x5eb1;
  // Counts.
  int64_t refused = 0, dropped = 0, left_out = 0, switches = 0, bytes_out = 0;
  std::string error;             // a packet that did not read (the face keeps serving the rest)
  std::vector<std::string> log;  // what it would print on its console (the first 256)
};

// `streams` views (at least one), every viewer on `follow` first.
void udp_tv_start(ClassicTvUdp& f, gn::Transport& t, const UdpTvConfig& cfg, int streams = 1);
// One packet of stream `s` as its client received it: the 8-byte channel header, then its
// messages. False (and `error`) when its messages do not read; what read before stays.
bool udp_tv_packet(ClassicTvUdp& f, int s, std::span<const uint8_t> packet);
// The view's angles of stream `s`'s player (a client recording's command records carry them).
void udp_tv_view(ClassicTvUdp& f, int s, sh::Vec3f angles);
// The stream the TV follows (its director's pick): every viewer that chose none moves to it.
void udp_tv_follow(ClassicTvUdp& f, int s);
// Stream `s` emptied: no level until its source sends one (a view let go, or turned to another
// player).
void udp_tv_clear(ClassicTvUdp& f, int s);
// The player slots viewers asked for that no stream carries yet (views on request).
std::vector<int> udp_tv_wanted(const ClassicTvUdp& f);
// How many viewers watch stream `s`, or wait for its player (views on request).
int udp_tv_watching(const ClassicTvUdp& f, int s);
// Reads every viewer's datagrams and sends what is due.
void udp_tv_frame(ClassicTvUdp& f, int64_t now_us);
// Every viewer sent the disconnect.
void udp_tv_stop(ClassicTvUdp& f);
int udp_tv_viewers(const ClassicTvUdp& f);

}  // namespace shooters_edge::net
