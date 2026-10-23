// THE SIMULATED DISCORD CLIENT: the Discord app on the player's machine, held in memory, so a
// game's Discord presence is tested and measured headless with no Discord and no account. It
// speaks the same frames as the real client over a `DiscordLink` (`link()`): it answers the
// handshake with READY, takes subscriptions, holds the activity to Discord's limits and its rate
// limit (a sixth update inside 20 s is refused with an ERROR, so a game that floods it fails a
// test), and closes the link on a bad client id. What the player and their friends do in the
// Discord app is a call on the sim: start or quit the app, a friend asking to join, the player
// pressing Join, a ping. Every frame either side sends is written to the journal with the time
// the game's poll passed in (or `tick` set), so a script replays to the same journal.
//
// ge:summary The simulated Discord client: handshake, subscriptions, activity limits, asks, joins
// ge:keywords services discord sim simulated client fake mock ipc ready subscribe set-activity
// ge:keywords ask-to-join join rate-limit journal running quit
// ge:skill services
// ge:test tests/services_discord_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "services/discord.hpp"

namespace ge::services {

class SimDiscord {
 public:
  // The player signed in to the app, by Discord id and name.
  explicit SimDiscord(UserId player = 1000000000000001, std::string name = "player");
  ~SimDiscord();

  // The game's end of the link (one game at a time; the sim may be dropped before it).
  std::unique_ptr<DiscordLink> link();

  // The app's clock, for the journal: a call on the sim between two polls happens at it. Each
  // exchange over the link moves it to the poll's time too.
  void tick(int64_t now_us);

  // The player starts or quits the Discord app: a live link closes, and the profile's activity
  // and the subscriptions go with it.
  void set_running(bool running);
  bool running() const;
  // A friend presses "Ask to Join" on the player's activity: delivered when the activity has a
  // join secret and the game subscribed, else ignored as Discord would.
  void ask_to_join(UserId friend_id, const std::string& name);
  // The player presses "Join" on a friend's activity (or accepts their invite): the game gets
  // ACTIVITY_JOIN with the friend's secret.
  void join(const std::string& secret);
  // The client pings the game; the game owes a pong.
  void ping();

  // The activity the player's friends see, as its JSON ("" for none).
  std::string shown() const;
  // The answers the game gave to asks: (friend, accepted).
  std::vector<std::pair<UserId, bool>> answers() const;
  int updates() const;   // activity updates the client took
  int refusals() const;  // ERRORs and closes it sent
  int pongs() const;     // pongs the game sent back
  const std::vector<std::string>& journal() const;

  struct State;  // the client's side, shared with the link (sim_discord.cpp)

 private:
  std::shared_ptr<State> state_;
};

}  // namespace ge::services
