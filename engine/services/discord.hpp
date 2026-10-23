// DISCORD: the player's game shown on their Discord profile (rich presence), friends asking to
// join it, and the player joining a friend's game from Discord. It talks to the Discord client
// already running on the player's machine over its LOCAL IPC (a local pipe named
// `discord-ipc-<n>`, net/local_pipe.hpp), so it needs no SDK, no secret and no network: only the
// game's application id from Discord's developer portal, which is public. With no client
// running it retries quietly with backoff, and a game works the same without it.
//
// It is a presence service beside the store, not a store: a game keeps its `Backend` for
// achievements, boards and cloud files and adds a `Discord` for its profile line. Both deliver
// through the same `Event`, and `poll` is the same one pump (once a frame, main thread, the
// frame's time). Discord's friends, lobbies, voice and account linking need its Social SDK and
// are a separate row (docs/services.md § Discord, docs/goals.md M20j).
//
// THE SHAPE. `set_activity` checks the activity against Discord's limits at once (`invalid`)
// and keeps it; `poll` sends the latest one when the client is ready, paced to one every 4 s so
// Discord's rate limit (5 updates in 20 s) always holds, so a game may call it every time
// something changes: the updates in between are coalesced and the profile is never more than
// 4 s behind. A join secret on the activity turns on "Ask to Join": a
// friend's ask arrives as `join_asked` (answer it with `answer_join`), and the player's own
// "Join" on a friend's activity, or an accepted invite, as `join_requested` carrying that
// friend's secret. `connection_restored` says a client was found, `connection_lost` that it
// went away; the activity is sent again when it comes back. A refusal from the client arrives as
// `call_refused` with Discord's message.
//
// The wire, stated once here: a frame is its op (uint32, little endian), its length (uint32,
// little endian) and that many bytes of JSON. Op 0 is the handshake (`{"v":1,"client_id":...}`),
// 1 a command or an event, 2 a close (`{"code":..,"message":..}`), 3 a ping and 4 its pong.
//
// ge:summary Discord rich presence, ask-to-join and joins over the client's local IPC, no SDK
// ge:keywords services discord rich-presence activity party join-secret ask-to-join invite join
// ge:keywords ipc discord-ipc local-pipe handshake frame set-activity subscribe rate-limit
// ge:keywords client-id backoff reconnect presence social
// ge:skill services
// ge:test tests/services_discord_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "services/backend.hpp"

namespace ge::services {

// What the player's profile shows. Every text is empty (not shown) or 2 to 128 bytes of UTF-8.
struct Activity {
  std::string details;   // the first line ("Downhill, seed 38")
  std::string state;     // the second ("Gate 12 of 40")
  int64_t start_ms = 0;  // unix time, ms: Discord counts up from it ("05:12 elapsed"); 0 none
  int64_t end_ms = 0;    // unix time, ms: Discord counts down to it; 0 none
  std::string large_image, large_text;  // an art asset's key (or an https URL) and its hover text
  std::string small_image, small_text;
  std::string party_id;  // the group the player is in; needed for a join secret
  int party_size = 0;    // "(2 of 4)": both 0, or 1 <= size <= max
  int party_max = 0;
  std::string join_secret;  // set: friends may ask to join; what a joiner's game is handed
  bool operator==(const Activity&) const = default;
};

constexpr size_t kActivityTextMin = 2;
constexpr size_t kActivityTextMax = 128;
constexpr size_t kActivityImageMax = 256;
constexpr int kDiscordUpdates = 5;                    // activity updates Discord takes...
constexpr int64_t kDiscordUpdateWindowUs = 20000000;  // ...in this long
constexpr int64_t kDiscordUpdateGapUs = kDiscordUpdateWindowUs / kDiscordUpdates;  // paced
constexpr size_t kDiscordMaxFrame = size_t{64} * 1024;  // a frame's JSON, at most

// "" when Discord would take the activity, else what is wrong with it (agent-actionable).
std::string check_activity(const Activity& a);
// The activity as Discord's JSON object.
std::string activity_json(const Activity& a);
// A string as a JSON string literal, quotes included.
std::string json_quote(std::string_view s);

enum class DiscordOp : uint32_t { handshake = 0, frame = 1, close = 2, ping = 3, pong = 4 };
std::string discord_frame(DiscordOp op, std::string_view json);
enum class FrameRead : uint8_t { none, whole, bad };
// Take the next whole frame off the front of `buffer`: `none` until one is whole, `bad` (and the
// link should close) for an unknown op or a length past kDiscordMaxFrame.
FrameRead discord_next_frame(std::string& buffer, DiscordOp& op, std::string& json);

// The way to the Discord client: the OS's local pipe, or the simulated client (sim_discord.hpp).
class DiscordLink {
 public:
  virtual ~DiscordLink() = default;
  // Reach the client; false (and why) when none is running.
  virtual bool open(std::string* error) = 0;
  // Send `out` whole and append what arrived to `in`; false once the link is gone.
  virtual bool exchange(int64_t now_us, const std::string& out, std::string& in) = 0;
  virtual void close() = 0;
  // The game's process id, which the client asks for with every activity.
  virtual int64_t process_id() const = 0;
};

// Where a Discord client listens on this machine, in the order to try: `discord-ipc-0` to `-9`
// under $XDG_RUNTIME_DIR, $TMPDIR, $TMP, $TEMP and /tmp, and the Flatpak's and the Snap's
// folders under each; the named pipes on Windows. $GE_DISCORD_IPC, when set, is the only one.
std::vector<std::string> discord_ipc_paths();
std::unique_ptr<DiscordLink> discord_os_link(std::vector<std::string> paths = discord_ipc_paths());

struct DiscordConfig {
  std::string client_id;            // the application id (digits), from the developer portal
  int64_t retry_us = 2000000;       // the first wait before looking for the client again
  int64_t retry_max_us = 60000000;  // the wait doubles up to this
};

// What the link did, for labs and tests.
struct DiscordCounts {
  int connects = 0;   // clients found (handshakes answered)
  int attempts = 0;   // times the link was opened or tried
  int sent = 0;       // activity updates sent (clears included)
  int coalesced = 0;  // updates replaced before they were sent
  int held = 0;       // polls an update waited on the rate limit
};

class Discord {
 public:
  Discord(DiscordConfig config, std::unique_ptr<DiscordLink> link);
  ~Discord();
  Discord(const Discord&) = delete;
  Discord& operator=(const Discord&) = delete;

  // The pump: find the client, send what is due, deliver what arrived into `out`, appended.
  void poll(int64_t now_us, std::vector<Event>& out);

  // Keep `a` as the profile's activity (`invalid` when check_activity refuses it).
  Result set_activity(const Activity& a);
  void clear_activity();
  // Let a friend who asked (`join_asked`) in, or turn them away; `offline` with no client.
  Result answer_join(UserId user, bool accept);

  bool connected() const;  // a client answered the handshake
  UserInfo user() const;   // the Discord user, once connected
  DiscordCounts counts() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace ge::services
