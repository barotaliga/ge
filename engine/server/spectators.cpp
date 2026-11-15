// Spectators on the server itself (spectators.hpp).
//
// ge:summary Spectators on the server itself: its own TV node, defined
// ge:keywords spectators server-tv sv_tvport sv_tvdelay
// ge:skill replay net
// ge:test tests/server_spectators_test.cpp
#include "server/spectators.hpp"

#include "core/num.hpp"
#include "net/address.hpp"

namespace ge::server {

void server_tv_open(ServerTv& t, const Server& s, net::Transport& transport) {
  const ServerConfig& c = s.config;
  tv::Config tc;
  tc.name = c.hostname;
  tc.delay_ms = static_cast<int64_t>(core::num::round(c.tv_delay_s * 1000));
  tc.max_viewers = c.tv_max_viewers;
  tc.game = s.game != nullptr ? s.game->binary : "";
  // Every link the net node takes: the TV node turns a viewer past the cap away with the reason.
  tv::tv_host_init(t.host, tc, transport, kMaxClients, uint64_t{c.seed} * 7919u + 11u);
  if (s.port != nullptr)
    t.host.source = c.public_addr.empty() ? net::to_string(s.port->local) : c.public_addr;
  if (s.game != nullptr && s.game->protocol != 0) t.host.public_protocol = s.game->protocol;
  if (!s.beat.config.masters.empty())
    tv::tv_host_list(t.host, s.beat.config.masters, s.beat.config.version,
                     uint64_t{c.seed} * 7919u + 12u);
  t.on = true;
  t.error.clear();
}

bool server_tv_frame(ServerTv& t, std::span<const uint8_t> fresh, int64_t now_us) {
  if (!t.on) return false;
  bool fed = true;
  if (!fresh.empty() && t.error.empty() && !tv::tv_feed(t.host.tv, fresh, now_us / 1000)) {
    t.error = t.host.tv.error;
    fed = false;
  }
  tv::tv_host_update(t.host, now_us);
  return fed && t.error.empty();
}

void server_tv_end(ServerTv& t) {
  if (t.on) tv::tv_end(t.host.tv);
}

void server_tv_close(ServerTv& t, int64_t now_us) {
  if (!t.on) return;
  tv::tv_host_unlist(t.host, now_us);
  t.on = false;
}

int64_t server_tv_watching(const ServerTv& t) { return t.on ? tv::tv_viewer_count(t.host.tv) : 0; }

std::string server_tv_status(const ServerTv& t) {
  return t.on ? "tv=" + std::to_string(server_tv_watching(t)) : std::string();
}

}  // namespace ge::server
