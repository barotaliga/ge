// PROXY (docs/tv.md § Proxy): two hops that understand the slow link between them. A player runs
// the NEAR proxy beside the game's client (on the same machine or LAN) and a FAR proxy runs near
// the server; the client connects to the near one as if it were the server, and the far one opens
// a socket of its own to the real server for that client. Between the two, every game datagram
// travels in a LINK FRAME that may be:
//
//   repeated  sent `repeat` times (the copies `gap_us` apart, so a short burst of loss takes only
//             some), and the receiving end passes on the first copy only (a window by number)
//   packed    XORed with the newest datagram the other end said it has (its number in the frame),
//             then its runs of zero bytes written as counts: a game whose next datagram is much
//             like its last sends a fraction of the bytes. A frame whose reference the receiver no
//             longer holds is dropped and counted; a frame that would not get smaller goes plain.
//   coded     XORed with the same reference and range coded by models both ends learn along the
//             references (link_code.hpp); a coding that would not get smaller packs instead.
//
// A FRAME is its kind (which says plain, packed or coded), the session, its number and the newest
// the other end has as their low 16 bits (each widened to the nearest the reader knows), and for
// a packed or coded frame its reference as a distance back: ten bytes on a coded frame.
//
// Both are chosen per direction at the sending end (`LinkConfig`), the classic proxies' rule: only
// the slow hop needs them. The game's datagrams to and from the client and the server stay as the
// game sent them. Each end tells the other the newest datagram it received in every frame (and in
// an ack frame when it has nothing to send for `ack_us`), which is what packing refers to.
//
// THE SESSION: the near proxy opens one per client with the far one, by the forward's pattern: an
// open request (the client's salt, the cookie, the server's address) padded past every answer, a
// cookie only the far proxy can make, then `opened` or a refusal; the far proxy forwards only to
// the servers on its allow list. A session silent both ways for `idle_us` is dropped.
//
//   proxy_near_init(n, config, client_port, link_port, far)   every frame: proxy_near_update(n,
//   now) proxy_far_init(f, config, link_port, open_upstream)        every frame:
//   proxy_far_update(f, now)
//
// RECORDING: `ProxyConfig::tap` sees every game datagram either proxy passes on, with its
// session and direction; `record.hpp` keeps them as a replay file.
//
// Lag and loss for practice are the transport's own (`net::transport_fake` on the client socket).
// The clock is the caller's: a pair on the simulated network replays to the bit (the `proxy` lab).
//
// ge:summary Proxy: a near and a far hop that repeat and pack a game's datagrams over a slow link
// ge:keywords proxy repeat redundancy packet-loss link-codec pack xor zero-run near far session
// ge:keywords range-coded learned-models frame-header
// ge:keywords lossy-link compression allow-list
// ge:skill net
// ge:test tests/proxy_test.cpp
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "net/address.hpp"
#include "net/sim_network.hpp"
#include "net/transport.hpp"
#include "proxy/link_code.hpp"

namespace ge::proxy {

inline constexpr size_t kOpenSize = 128;  // an open request, padded: no answer is bigger
inline constexpr int kKept = 64;          // datagrams each end keeps to pack against
inline constexpr size_t kMostDatagram = net::kMaxDatagram - 32;  // a game datagram the link takes

// One direction's choices, at its sending end.
struct LinkConfig {
  int repeat = 1;          // copies of each frame, 1..5
  int64_t gap_us = 4000;   // between copies
  bool pack = false;       // XOR against the last datagram the other end has, zero runs counted
  bool code = false;       // XOR against it, range coded by learned models (link_code.hpp)
  int64_t ack_us = 50000;  // an ack frame when nothing else went for this long
};

// The datagram's packing: zero runs and literal runs of its XOR with `ref`, into `out`; its size,
// 0 when it would not fit or not be smaller.
size_t pack_datagram(std::span<const uint8_t> datagram, std::span<const uint8_t> ref, uint8_t* out,
                     size_t capacity);
// The datagram back from its packing and the same `ref`; false when the bytes are broken.
bool unpack_datagram(std::span<const uint8_t> packed, std::span<const uint8_t> ref,
                     std::vector<uint8_t>& out);

struct Kept {
  uint32_t seq = 0;
  uint16_t size = 0;
  std::array<uint8_t, kMostDatagram> bytes{};
};

struct LinkStats {
  uint64_t datagrams_out = 0, frames_out = 0, bytes_out = 0, payload_out = 0, packed = 0;
  uint64_t coded = 0;
  uint64_t datagrams_in = 0, frames_in = 0, duplicates = 0, late = 0, no_ref = 0, broken = 0;
};

// ONE END OF THE LINK: what it sent (to pack against) and what it received (the window, the
// newest, the kept datagrams the other end packs against).
struct LinkEnd {
  LinkConfig config;
  uint32_t session = 0;
  uint32_t next_seq = 1;
  uint32_t acked = 0;   // the newest the other end said it received
  uint32_t newest = 0;  // the newest received
  uint64_t window = 0;  // bit i: newest - i received
  std::array<Kept, kKept> sent{}, got{};
  // The coder's models beside each kept datagram (empty until the link codes: every model fresh).
  std::vector<LinkModels> sent_models, got_models;
  struct Copy {
    int64_t at_us = 0;
    uint16_t size = 0;
    std::array<uint8_t, net::kMaxDatagram> bytes{};
  };
  std::vector<Copy> copies;  // repeats waiting their turn
  int64_t last_out_us = 0;
  LinkStats stats;
};

// A game datagram onto the link: its frame now (by `out`), its repeats later.
using FrameOut = std::function<void(std::span<const uint8_t> frame)>;
void link_send(LinkEnd& e, std::span<const uint8_t> datagram, int64_t now_us, const FrameOut& out);
// Repeats due and an ack when the link has been quiet.
void link_update(LinkEnd& e, int64_t now_us, const FrameOut& out);
// A frame from the other end: 1 and `datagram` when it carried a new one, 0 when it carried none
// (a repeat, an ack, a frame that cannot be unpacked), -1 when it is not a link frame.
int link_receive(LinkEnd& e, std::span<const uint8_t> frame, std::vector<uint8_t>& datagram);
// A frame's session (its first word after the kind), or 0.
uint32_t frame_session(std::span<const uint8_t> frame);

using OpenUpstream = std::function<bool(net::Transport& out)>;

// A game datagram as a proxy passes it on, for a recorder (record.hpp) or an observer: a near
// proxy's from its client and to it, a far proxy's to its server and from it.
struct Passed {
  uint32_t session = 0;
  net::Address client;  // near: the game's client; far: the near proxy
  net::Address server;  // far: the server; near: the server its sessions go to
  bool to_server = false;
  std::span<const uint8_t> datagram;
  int64_t now_us = 0;
};
using Tap = std::function<void(const Passed& p)>;

struct ProxyConfig {
  std::string name = "proxy";
  LinkConfig link;                  // this end's sending choices
  std::vector<net::Address> allow;  // far: the servers it reaches (port 0: any port on the host)
  bool open = false;                // far: any server
  int max_sessions = 64;
  int64_t idle_us = 15000000;
  int64_t retry_us = 500000, open_timeout_us = 10000000;
  uint64_t seed = 0;
  Tap tap;  // every game datagram passed on, when set
};

// THE NEAR PROXY: the game's client on one socket, the far proxy on another.
struct NearClient {
  net::Address from;
  LinkEnd link;
  uint64_t salt = 0, cookie = 0;
  bool opened = false, refused = false;
  std::string why;
  int64_t since_us = 0, last_us = 0, next_open_us = 0;
};

struct ProxyNear {
  ProxyConfig config;
  net::Transport* client_port = nullptr;
  net::Transport* link_port = nullptr;
  net::Address far;
  net::Address server;  // where every client's session goes
  std::vector<std::unique_ptr<NearClient>> clients;
  uint64_t draws = 0;
  uint64_t strays = 0, refused = 0, idle_drops = 0;
  std::string last_refusal;  // the far proxy's words the last time it refused a session
};

void proxy_near_init(ProxyNear& n, const ProxyConfig& config, net::Transport& client_port,
                     net::Transport& link_port, const net::Address& far,
                     const net::Address& server);
void proxy_near_update(ProxyNear& n, int64_t now_us);

// THE FAR PROXY: near proxies on one socket, a socket of its own to the server per session.
struct FarSession {
  net::Address near;  // the near proxy's socket
  net::Address server;
  LinkEnd link;
  std::unique_ptr<net::Transport> up;
  int64_t since_us = 0, last_us = 0;
};

struct ProxyFar {
  ProxyConfig config;
  net::Transport* link_port = nullptr;
  OpenUpstream open_upstream;
  uint64_t secret = 0;
  std::vector<std::unique_ptr<FarSession>> sessions;
  uint64_t cookies = 0, refused = 0, strays = 0, idle_drops = 0;
};

void proxy_far_init(ProxyFar& f, const ProxyConfig& config, net::Transport& link_port,
                    OpenUpstream open_upstream);
void proxy_far_update(ProxyFar& f, int64_t now_us);
void proxy_far_close(ProxyFar& f);

}  // namespace ge::proxy
