// Our TV for the sequel end to end (sequel_tv_relay.hpp): the watcher's new blocks broadcast,
// the node's delayed stream unpacked, the blocks served by the face.
//
// ge:summary The sequel TV relay's steps: the watcher's blocks through a TV node to the face
// ge:keywords tv sequel watcher broadcast tv-node delay unpack relay redial
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_tv_face_test.cpp
#include "shooters_edge/net/sequel_tv_relay.hpp"

#include <cmath>
#include <span>
#include <utility>

#include "core/libm/libm.hpp"

namespace shooters_edge::net {

namespace {

constexpr double kRedial = 5;
constexpr size_t kLogKept = 256;

void note(SequelTvRelay& r, std::string line) {
  if (r.log.size() < kLogKept) r.log.push_back(std::move(line));
}

uint32_t length_at(const std::vector<uint8_t>& b, size_t at) {
  return static_cast<uint32_t>(b[at]) | static_cast<uint32_t>(b[at + 1]) << 8 |
         static_cast<uint32_t>(b[at + 2]) << 16 | static_cast<uint32_t>(b[at + 3]) << 24;
}

float wrap(float a) {
  while (a > 180) a -= 360;
  while (a < -180) a += 360;
  return a;
}

// THE CAMERA: the watcher, a spectator flying free, follows one player at a time from behind and
// above, looking at them; it keeps a player for 20 s, or until they are gone, then takes the next.
// The move's angles are steered by what the server shows back (the view it sends holds the
// server's own turn on top of the move's), so the camera needs no knowledge of that turn.
sh::MoveCmd director(SequelTvRelay& r, int tick) {
  const client::DemoState& s = r.watcher.demo.state;
  sh::MoveCmd m;
  m.angles = r.aim;
  const int own = s.player + 1;
  auto shown = [&](int n) {
    return n > 0 && n != own && n < static_cast<int>(s.entities.size()) &&
           s.entities[static_cast<size_t>(n)].present && s.entities[static_cast<size_t>(n)].model;
  };
  if (!shown(r.target) || tick - r.target_since > 20 * 77) {
    const int from = r.target;
    for (int k = 1; k <= 32; k++) {
      const int n = (from + k - 1) % 32 + 1;
      if (shown(n)) {
        r.target = n;
        r.target_since = tick;
        break;
      }
    }
  }
  if (!shown(r.target)) return m;
  const client::DemoEntity& t = s.entities[static_cast<size_t>(r.target)];
  const double yaw = static_cast<double>(t.angles.y) * 3.14159265358979323846 / 180;
  const sh::Vec3f spot = {t.origin.x - static_cast<float>(ge::core::libm::cos(yaw)) * 140,
                          t.origin.y - static_cast<float>(ge::core::libm::sin(yaw)) * 140,
                          t.origin.z + 64};
  const sh::Vec3f look = {t.origin.x - s.eye.x, t.origin.y - s.eye.y, t.origin.z + 16 - s.eye.z};
  const double flat = std::sqrt(static_cast<double>(look.x * look.x + look.y * look.y));
  const auto want_yaw =
      static_cast<float>(ge::core::libm::atan2(look.y, look.x) * 180 / 3.14159265358979323846);
  const auto want_pitch =
      static_cast<float>(-ge::core::libm::atan2(look.z, flat) * 180 / 3.14159265358979323846);
  r.aim.y = wrap(r.aim.y + 0.3f * wrap(want_yaw - s.angles.y));
  r.aim.x = r.aim.x + 0.3f * wrap(want_pitch - s.angles.x);
  m.angles = r.aim;
  // The way to the spot in the view's own axes.
  const sh::Vec3f to = {spot.x - s.eye.x, spot.y - s.eye.y, spot.z - s.eye.z};
  const double vy = static_cast<double>(s.angles.y) * 3.14159265358979323846 / 180;
  const auto c = static_cast<float>(ge::core::libm::cos(vy));
  const auto n = static_cast<float>(ge::core::libm::sin(vy));
  auto axis = [](float v) { return static_cast<int16_t>(v > 400 ? 400 : v < -400 ? -400 : v); };
  m.forward = axis(3 * (to.x * c + to.y * n));
  m.side = axis(3 * (to.x * n - to.y * c));
  m.up = axis(3 * to.z);
  return m;
}

void dial(SequelTvRelay& r, int64_t now_us) {
  SequelClientConfig wc;
  wc.name = r.cfg.watcher;
  wc.spectator = true;
  wc.password = r.cfg.server_password;
  wc.whole_every = r.cfg.whole_every;
  wc.qport = static_cast<uint16_t>(0x7000 + (r.dials & 0xfff));
  wc.brain = [&r](int tick) { return director(r, tick); };
  sequel_client_start(r.watcher, *r.watcher_port, r.server, wc, static_cast<double>(now_us) / 1e6);
  r.taken = 0;
  r.dials++;
}

}  // namespace

void sequel_tv_relay_open(SequelTvRelay& r, gn::Transport& watcher_port, gn::Transport& face_port,
                          const gn::Address& server, const SequelTvRelayConfig& cfg,
                          int64_t now_us) {
  r.cfg = cfg;
  r.server = server;
  r.watcher_port = &watcher_port;
  r.face_port = &face_port;
  r.broadcast = std::make_unique<client::SequelBroadcast>();
  client::sequel_broadcast_begin(*r.broadcast, 0);
  r.node.config.name = cfg.face.name;
  r.node.config.delay_ms = static_cast<int64_t>(cfg.delay_s * 1000);
  ge::tv::tv_join(r.node, 0);
  ge::tv::tv_receive(r.node, 0, ge::tv::tv_hello("sequel face", false));
  sequel_tv_start(r.face, face_port, cfg.face);
  dial(r, now_us);
}

void sequel_tv_relay_update(SequelTvRelay& r, int64_t now_us) {
  const double now = static_cast<double>(now_us) / 1e6;
  const int64_t now_ms = now_us / 1000;
  // The watcher, and its new blocks into the broadcast.
  if (r.watcher.stage == ClientStage::dropped) {
    if (r.retry_at <= 0) {
      r.retry_at = now + kRedial;
      note(r, "the server let the watcher go (" + r.watcher.refused + "); dialling again in 5 s");
    } else if (now >= r.retry_at) {
      r.retry_at = 0;
      dial(r, now_us);
    }
  } else {
    sequel_client_tick(r.watcher, now_us);
  }
  const std::vector<uint8_t>& rec = r.watcher.demo.bytes;
  while (r.taken + 4 <= rec.size()) {
    const uint32_t len = length_at(rec, r.taken);
    if (len == 0xffffffffu || r.taken + 4 + len > rec.size()) break;
    client::sequel_broadcast_block(*r.broadcast,
                                   std::span<const uint8_t>(rec.data() + r.taken + 4, len));
    r.taken += 4 + len;
  }
  const std::span<const uint8_t> fresh = client::sequel_broadcast_fresh(*r.broadcast);
  if (!fresh.empty() && !ge::tv::tv_feed(r.node, fresh, now_ms) && r.error != r.node.error) {
    r.error = r.node.error;
    note(r, "the broadcast stopped: " + r.error);
  }
  // The node's delayed stream back into blocks, and the blocks to the face.
  bool more = false;
  ge::tv::tv_update(r.node, now_ms, [&](int, std::span<const uint8_t> msg) {
    if (!ge::tv::watch_receive(r.watch, msg)) return false;
    more = true;
    return true;
  });
  if (more && r.watch.have_header) {
    std::string error;
    if (!client::unpack_more(r.unpacker, r.watch.stream, r.blocks, error) && error != r.error) {
      r.error = error;
      note(r, "the delayed stream did not unpack: " + error);
    }
  }
  while (r.served + 4 <= r.blocks.size()) {
    const uint32_t len = length_at(r.blocks, r.served);
    if (len == 0xffffffffu || r.served + 4 + len > r.blocks.size()) break;
    sequel_tv_block(r.face, std::span<const uint8_t>(r.blocks.data() + r.served + 4, len));
    r.served += 4 + len;
  }
  if (r.served > (1u << 20)) {  // what was served is let go
    r.blocks.erase(r.blocks.begin(), r.blocks.begin() + static_cast<std::ptrdiff_t>(r.served));
    r.served = 0;
  }
  sequel_tv_frame(r.face, now_us);
}

void sequel_tv_relay_close(SequelTvRelay& r) {
  sequel_tv_stop(r.face);
  sequel_client_stop(r.watcher);
}

}  // namespace shooters_edge::net
