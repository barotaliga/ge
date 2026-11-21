// Configs over a session (net/session_config.hpp): the infos and stuffed text applied.
//
// ge:summary The server info sent and applied, the user info sent and read, stuffed text run
// ge:keywords net session config server-info user-info stuffed apply
// ge:skill net console
// ge:test tests/net_session_config_test.cpp
#include "net/session_config.hpp"

namespace ge::net {

bool session_host_follow_cvars(SessionHost& host, const console::Cvars& cvars) {
  const std::string info = cvars.info_string(console::kServerInfo);
  return session_host_server_info(host, info.data(), info.size());
}

std::string session_user_value(const SessionHost& host, int player, std::string_view key) {
  if (player < 0 || player >= kMaxPlayers) return {};
  const std::string& info = host.user_info[player];
  // `\key\value` pairs: walk them two by two.
  size_t at = 0;
  while (at < info.size() && info[at] == '\\') {
    const size_t k_end = info.find('\\', at + 1);
    if (k_end == std::string::npos) break;
    size_t v_end = info.find('\\', k_end + 1);
    if (v_end == std::string::npos) v_end = info.size();
    if (std::string_view(info).substr(at + 1, k_end - at - 1) == key)
      return info.substr(k_end + 1, v_end - k_end - 1);
    at = v_end;
  }
  return {};
}

void session_client_follow_cvars(SessionClient& client, console::Console& console,
                                 ClientConfig& state) {
  for (const SessionEvent& e : client.events) {
    if (e.type == SessionEventType::server_info) {
      state.infos++;
      state.moved += static_cast<uint64_t>(
          console.cvars().apply_info(session_text(client.event_bytes, e), console::Source::server));
    } else if (e.type == SessionEventType::stuffed) {
      const size_t before = console.problems().size();
      console.execute(session_text(client.event_bytes, e), console::Source::server);
      state.stuffed++;
      state.refused += console.problems().size() - before;
    }
  }
  if (client.state != ClientState::seated) return;
  const std::string user = console.cvars().info_string(console::kUserInfo);
  if (user != state.sent_user && session_client_user_info(client, user.data(), user.size()))
    state.sent_user = user;
}

}  // namespace ge::net
