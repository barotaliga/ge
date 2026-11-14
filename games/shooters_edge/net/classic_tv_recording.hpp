// A CLIENT RECORDING AS THE TV FOR CLASSIC CLIENTS (docs/tv.md § Classic clients at the TV): a
// multiplayer client's recording (packed or archived too: client/demo.hpp opens it) played
// into the classic clients' face (classic_tv_udp.hpp) at its own pace, so any classic client, the
// oldest too, watches it as it would a live match, seated in the recording player's view.
//
// THE RECORDS: each packet goes to the face when its time comes (the recording's clock from its
// first packet, a silence longer than a second cut to one: a recording that waited for its
// player to load plays on), each command's view angles become the view the face turns its viewers
// to. A recording made from the start of a connection holds its own signon; one begun later must
// carry one written in front of it (the face waits for the server data).
//
// THE LOOP: at the end it starts again from the first record, the face's spawn count raised by
// one each time round, so every viewer loads the level again.
//
// ge:summary A client recording played at its pace into the classic clients' face, looping
// ge:keywords classic-tv tv demo client-recording recording playback loop old-clients protocol-28
// ge:keywords udp_tv_recording_open udp_tv_recording_update mvd multiview views-on-request
// ge:keywords lazy-view udp_tv_multiview_open udp_tv_multiview_update
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_tv_udp_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "shooters_edge/client/demo_convert.hpp"
#include "shooters_edge/net/classic_tv_udp.hpp"

namespace shooters_edge::net {

struct UdpTvRecording {
  std::vector<uint8_t> bytes;  // the recording unpacked: its records
  size_t first = 0;            // the first record (after a voice header)
  size_t at = 0;               // the next record
  int cmd_size = 24;           // the command struct's bytes: 24 or 28
  bool loop = true;
  int stream = 0;         // the face's stream it plays into
  float first_time = 0;   // the first record's time
  int64_t start_us = -1;  // when the round began; -1: at the first update
  double last_time = 0;   // the last record's time
  double skipped = 0;     // s of silence cut this round
  int64_t packets = 0, rounds = 0;
  bool ended = false;    // the end reached with no loop, or a packet that did not read
  bool growing = false;  // more records may come (a view converted as it plays): wait for them
  bool cut = true;       // cut long silences (off for views that share one clock)
};

// The recording in `bytes` opened (unpacked when packed or archived). False with `error` when it
// is not a multiplayer client's recording.
bool udp_tv_recording_open(UdpTvRecording& d, std::vector<uint8_t> bytes, bool loop,
                           std::string& error);
// The records due by `now_us` handed to `face`.
void udp_tv_recording_update(UdpTvRecording& d, ClassicTvUdp& face, int64_t now_us);

// A MULTIVIEW RECORDING (`.mvd`) AS THE TV FOR CLASSIC CLIENTS: every player's view on request.
// The face's first stream is the view the TV follows (the lowest slot playing), converted from the
// start (client/demo_convert.hpp: a multiview recording to one player's client recording, as it is
// read). Any other player's view is converted only while a viewer watches it or asks for it (the
// face's views on request: a spectator's track keys, or fire for the next player): a stream is
// turned to that player, read up to where the match is with no view written, then his view starts
// there with a whole signon, and the viewers who asked move to it. A stream nobody has watched for
// `kIdleS` seconds is let go. Every view rides one clock, the followed view's.
struct UdpTvMultiview {
  struct View {
    client::DemoConvert convert;
    UdpTvRecording play;  // its client recording, growing as it is converted
    size_t fed = 0;       // source bytes fed
    int slot = -1;        // the player whose view it is; -1 the lowest playing (the followed view)
    bool on = false;
    bool source_ended = false;
    double idle_since = -1;  // s when it was last watched
  };
  std::vector<uint8_t> source;  // the multiview recording, unpacked
  std::vector<View> views;      // one per face stream
  // Where the followed view's conversion stood: source bytes fed and its newest record's time, so
  // a view turned on later is read up to the match's moment before its own view is written.
  std::vector<std::pair<size_t, double>> marks;
  bool loop = true;
  int64_t start_us = -1;
  int64_t rounds = 0, started = 0, let_go = 0;
  bool ended = false;
  std::string error;
};

// The multiview recording in `bytes` opened, `streams` the face's streams (its views at once, at
// least 1). False with `error` when it is not a multiview recording.
bool udp_tv_multiview_open(UdpTvMultiview& d, std::vector<uint8_t> bytes, int streams, bool loop,
                           std::string& error);
// The views' records due by `now_us` handed to `face` (started with as many streams), the views
// viewers want turned on and the unwatched ones let go.
void udp_tv_multiview_update(UdpTvMultiview& d, ClassicTvUdp& face, int64_t now_us);

}  // namespace shooters_edge::net
