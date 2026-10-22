---
name: multiplayer
description: "Use when this game talks over a network: the engine's ge::net (a node for a host, a client or a peer of up to 32, its channels, its events), testing a session on the simulated network under bad links, capturing a live node's traffic, and the ge net tools that read it (ge net inspect, ge net stats, ge net sim)."
---

# Multiplayer on ge::net

The engine's network is `ge::net` (link `ge::net`; headers `net/*.hpp`). Its own UDP protocol
runs on the caller's clock and a seeded stream, so a session on the simulated network is
deterministic: the same seed and the same sends give the same packets, losses and arrivals.
Real sockets (IPv4 and IPv6, on every desktop and mobile platform) and the simulated network
share one `Transport`, so code tested on the second runs unchanged on the first.

## A node, once a frame

```cpp
#include "net/node.hpp"
#include "net/packet.hpp"
namespace net = ge::net;

net::NodeConfig cfg;
cfg.protocol = net::protocol_id("my-game", 3);   // builds that cannot play together never talk
cfg.max_peers = 8;                               // the game's maximum, 1..32
cfg.accept = true;                               // a host; a client leaves it false
cfg.channels.count = 3;
cfg.channels.delivery[0] = net::Delivery::sequenced;   // inputs: the newest wins
cfg.channels.delivery[1] = net::Delivery::unreliable;  // snapshots
cfg.channels.delivery[2] = net::Delivery::reliable;    // events and chat: once, in order
cfg.channels.largest[2] = 64 * 1024;                   // and messages up to 64 KB, in pieces
cfg.channels.priority[0] = 2;                          // inputs first when room is short
cfg.channels.budget_kbps = 256;                        // each connection's most, kbit/s (0: none)
cfg.seed = net::os_entropy();                    // a test passes a fixed seed instead

net::Transport t;
std::string err;
net::Address bind;
net::parse_address("0.0.0.0:27500", bind);
if (!net::open_udp_transport(t, bind, &err)) { /* say err: the port, the next action */ }
net::Node node;
net::node_init(node, cfg, t);

// each frame, now_us from the game's own clock:
net::node_update(node, now_us);
for (const net::Event& e : node.events) {
  // connected, disconnected (e.reason), message (net::event_data(node, e), e.size), connect_failed
}
net::node_send(node, peer, 2, bytes, size);   // or node_broadcast
net::node_flush(node, now_us);
```

A client calls `node_connect(node, host_address, now_us)` and waits for `connected` or
`connect_failed` (full, closed, timed out). A message is at most `net::kMaxMessage` bytes, or
the channel's `largest` on a reliable channel (up to 64 KB, sent in pieces and delivered whole).
A reliable channel refuses a message when its pieces do not fit beside the ones still
unacknowledged (128 at most), so check `node_send`'s answer and try again next frame.

**TCP.** For a stream instead of datagrams (a turn-based game, a network that blocks UDP), open
the transport with `net::open_tcp_host_transport(t, bind, &err)` on the host and
`net::open_tcp_client_transport(t, host_address, &err)` on a client; the node code is the same.

## Test it on bad links

Use the simulated network in tests and labs, never sleeps or real sockets:

```cpp
net::SimNetwork sim = net::make_sim_network(seed, conditions);   // net/sim_network.hpp
net::LinkConditions bad;
net::conditions_preset("bad", bad);       // lan, broadband, wifi, mobile, bad, awful
net::sim_set_link(sim, 0, 3, bad);       // node 0 to node 3 only; sim_set_path sets both ways
// bad.stream = true: the link behaves as TCP (nothing lost; a drop arrives a resend late)
net::open_sim_transport(t, sim, net::sim_address(1, 1));
// each tick: sim_advance(sim, now_us), then every node's update and flush
```

A bot can play the game over a simulated link: give it a node like a player's and step the
network between the frames. Hold the game to what the player must never see, such as a
reliable event lost or an input applied twice, under every preset, and keep the digest of the
session in a golden.

## Capture and read the traffic

- **A capture.** Point `transport.capture` at a `net::Capture` (`net/capture.hpp`). On a
  live build, give it a `limit_bytes`. Then `net::save_capture(c, "runs/session.pcap", &err)`.
  The file is a normal pcap and Wireshark opens it.
- **`ge net inspect <pcap> [--peer=<addr>] [--type=payload]`**: every datagram on one line.
  It shows the time, the addresses, the type, the sequence and acks, and each message's
  channel, id and size.
- **`ge net stats <pcap>`**: one row per connection, as seen by the node that took the
  capture.
  - Columns: packets and kbit/s each way, round trip p50/p95, loss each way, duplicates,
    reorders and resends.
  - It also writes a series file and charts of kbps, rtt and loss over time; read them with
    `ge graph`.
  - A warning means a link lost more than 5%.
- **`ge net sim [--presets=wifi,bad] [--mix=action|download] [--link=latency=80,loss=3%] [--transport=tcp] [--budget=256] [--capture=FILE]`**:
  fake clients and a host, with no game.
  - It prints delivery, round trip, loss, bandwidth and a digest per session.
  - Use it to see what a link does before blaming the game, and to reproduce a player's link
    from the numbers `ge net stats` gave.

**The tick is part of the round trip.** A node reads its socket once a frame, so each way
takes its latency rounded up to whole frames. At 60 Hz, a 0.25 ms LAN measures 33 ms. A
round trip far beyond `2 × ceil(latency / frame)` is a queue: look at `kbps out` against the
link's bandwidth.
