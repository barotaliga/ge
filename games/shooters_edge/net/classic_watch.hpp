// WATCHING A CLASSIC BROADCAST: our client as a viewer of a classic stream port (our server's,
// our TV node's, or any relay's that speaks the stream protocol; docs/tv.md § The classic face).
//
// The client dials over TCP, asks for the match (`QTV`, `VERSION: 1`, `RECEIVE`, the player's name
// in `USERINFO`, every method it knows in `AUTH` when a password is given), answers a challenge
// with the request again and the response, and reads the records after `BEGIN` as a multiview
// recording that grows: whole frames only are handed to the demo reader (client/demo.hpp), so it
// never meets a frame half come. The window draws the match as it draws a joined game
// (classic_remote.hpp's `watch` branch: the same view, sounds and lines, riding the lowest player
// playing).
//
// Up from the viewer: what it says (`say <text>`), sent as the stream protocol's command.
//
// ge:summary Our client watching a classic broadcast: the dial, the answer, the stream as a demo
// ge:keywords watch viewer qtv broadcast stream-port relay tv-node multiview live-demo say
// ge:keywords watch_open watch_take watch_say watch_close
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_watch_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "net/address.hpp"
#include "net/tcp_socket.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/net/classic_qtv.hpp"

namespace shooters_edge::net {

enum class WatchState : uint8_t { asking, challenged, watching, ended };

struct StreamWatch {
  gn::TcpStream tcp;
  gn::Address to;
  WatchState state = WatchState::asking;
  std::string name, password;
  std::string text;           // the answer's text so far
  std::vector<uint8_t> held;  // records come but not yet a whole frame's
  std::string why;            // why it ended
  int64_t records = 0, frames = 0;
};

// The broadcast at `to` asked for, as `name` (and `password` when one is needed).
bool watch_open(StreamWatch& w, const gn::Address& to, const std::string& name,
                const std::string& password, std::string& error);
// What has come: the answer read, and every whole frame since appended to `demo.bytes` (a
// multiview recording opened by `watch_demo`). False once the broadcast refused or ended
// (`why`); a recording that ends with its own disconnect is handed over whole first.
bool watch_take(StreamWatch& w, client::Demo& demo);
// The reader for a broadcast: a multiview recording with nothing read yet.
void watch_demo(client::Demo& d);
// `text` said to the other viewers.
void watch_say(StreamWatch& w, const std::string& text);
void watch_close(StreamWatch& w);

}  // namespace shooters_edge::net
