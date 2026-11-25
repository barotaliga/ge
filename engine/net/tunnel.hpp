// A TUNNEL: a game's own datagram protocol carried inside a node (node.hpp), so an older protocol
// gets the node's handshake, its sealing with connect tokens (M17i), its flood guards and its ways
// through home routers without changing a byte of what it sends. The game keeps talking to a
// transport, the tunnel's INNER one (a pipe, transport.hpp): what it sends to a peer of the node
// goes on the tunnel's channel, and every datagram that came, carried or not, is read from it
// with the address it came from.
//
// ONE PORT SERVES BOTH. The node keeps the datagrams that are not its protocol's
// (`NodeConfig::keep_foreign`), and the tunnel hands them to the game as they came: a host serves
// the clients that speak its protocol plainly and the ones that came through a node at once, on
// one socket, and the game cannot tell them apart but by `tunnel_carried`. A datagram the game
// sends to an address with no connected peer leaves plainly on the outer transport.
//
// A CLIENT connects its node to the host and holds what the game sends until it is accepted (a
// few datagrams; a game's handshake asks again anyway). A connect that fails (a host of the older
// protocol alone) leaves `failed` set, and the inner transport carries the game's datagrams plainly
// from then on: the game keeps the one transport whichever way it joined.
//
// PIECES. The node's messages are smaller than a game's largest datagram (kMaxMessage against
// kMaxDatagram), so a datagram goes as up to kTunnelPieces pieces, each with its number, its
// place and the count; the far end puts one together when every piece is in and drops what is
// left of an older one, as the IP layer would: a lost piece loses the datagram, which the game's
// own protocol already lives with.
//
// A frame's use:
//   tunnel_update(t, now)    the node's update: messages and foreign datagrams queued for the game
//   ... the game reads and writes t.inner ...
//   tunnel_flush(t, now)     the node's flush: the pieces sent
//
// Like the node, nothing here reads a clock or draws randomness.
//
// ge:summary A tunnel: a game's own datagram protocol carried inside a node, sharing its port
// ge:keywords net tunnel native-protocol carry wrap legacy-protocol pipe keep_foreign pieces
// ge:keywords reassembly secure-link one-port both-protocols
// ge:skill net
// ge:test tests/net_tunnel_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "net/node.hpp"
#include "net/transport.hpp"

namespace ge::net {

constexpr size_t kTunnelHeader = 4;  // the datagram's number (2), the piece's place (1), the count
constexpr size_t kTunnelPiece = kMaxMessage - kTunnelHeader;
constexpr int kTunnelPieces = 4;
constexpr int kTunnelHeld = 16;  // datagrams a client holds until its node is accepted

struct TunnelConfig {
  uint32_t protocol = 0;  // the node's (protocol_id(game, version))
  int max_peers = kMaxPeers;
  uint64_t seed = 0;
  NodeSecurity security;  // a host's; a client connects with `token` when set
  NodeGuards guards;
  int64_t timeout_us = 5000000;
  int64_t connect_timeout_us = 3000000;
};

struct TunnelStats {
  uint64_t carried_in = 0, carried_out = 0;  // datagrams through the node
  uint64_t plain_in = 0, plain_out = 0;      // datagrams around it, on the outer transport
  uint64_t lost = 0;                         // datagrams a piece of which never came
  uint64_t refused = 0;  // datagrams the node would not queue, or a client could not hold
};

struct Tunnel {
  bool host = false;
  Transport* outer = nullptr;
  Transport inner;  // the game's
  std::unique_ptr<Node> node;
  int server_peer = -1;  // a client's
  Address server;        // a client's host
  bool failed = false;   // a client's connect failed or was dropped
  int64_t now_us = 0;    // the last update's or flush's, for the outer transport's captures
  // What came, for the game to read, oldest first: each datagram's sender and its bytes.
  struct Came {
    Address from;
    uint32_t offset = 0, size = 0;
  };
  std::vector<Came> came;
  std::vector<uint8_t> came_bytes;
  size_t next = 0;  // the next to hand the game
  // A client's datagrams waiting for the accept.
  std::vector<std::vector<uint8_t>> held;
  // Per peer: the datagram being put together.
  struct Partial {
    uint16_t number = 0;
    uint8_t count = 0, have = 0;  // `have`: a bit a piece
    uint32_t size = 0;
    uint8_t data[kTunnelPieces * kTunnelPiece];
  };
  std::vector<Partial> partial;
  std::vector<uint16_t> next_number;  // per peer
  TunnelStats stats;
};

// A host's tunnel on `outer`, its node taking connects. False when the config is unusable.
bool tunnel_host(Tunnel& t, Transport& outer, const TunnelConfig& config);
// A client's on `outer`, its node connecting to `host` (with `token` when it is not null).
bool tunnel_connect(Tunnel& t, Transport& outer, const Address& host, const TunnelConfig& config,
                    int64_t now_us, const ConnectToken* token = nullptr);
void tunnel_update(Tunnel& t, int64_t now_us);
void tunnel_flush(Tunnel& t, int64_t now_us);
// Tell every peer the tunnel closes (the node's disconnects); the inner transport stays open.
void tunnel_close(Tunnel& t, int64_t now_us);
// Whether datagrams to and from `address` go through the node (a connected peer).
bool tunnel_carried(const Tunnel& t, const Address& address);
// A client's: its node accepted by the host.
bool tunnel_joined(const Tunnel& t);

}  // namespace ge::net
