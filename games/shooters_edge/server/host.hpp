// THE SERVER'S HOST: one Shooter's Edge server run on the classic loop (plugins/shooter), its
// slots and its input log. The same host runs a dedicated server (`shooters_edge_server`, the
// headless binary) and, later, a listen server inside the player's own process (M18q): a server
// frame is one step of the run clock, and nothing here reads a wall clock.
//
// SLOTS: up to `maxclients` (at most 32). A slot is EMPTY, REMOTE (a client's packets fill it,
// handed to `host_frame` in arrival order) or a BOT (a command source fills it: cmd_source.hpp).
// A slot taken is a client connected by the loop's `connect_client` (the program's connect and
// put-in-server hooks), a bot's with the bot flag, so its speed check is skipped; a slot freed
// is `disconnect_client`. Both are logged before the frame they happen in.
//
// A BOT'S COMMANDS: each bot keeps a command clock at the server's 77 a second (usercmd.hpp),
// advanced by the frame's length; when it comes due the source is asked for a command of the
// clock's msec, the angles and axes are put on the protocol's grid, and the slot's packet (its
// three newest commands, oldest first, a sequence number) goes into the frame after the remote
// packets, in slot order. So a bot's packets are made and run exactly as a person's.
//
// ONE FRAME (`host_frame`): the bots' packets built off the server as the last frame left it; the
// remote packets, then the bots', logged; `server_frame`; the frame's hash logged. A run is a pure
// function of its settings, its slot events and the packets it was handed: `replay_input_log`
// runs a log's frames again and names the first whose hash differs (-1: none).
//
// The host keeps the arena, the program and the server in place: views point into them, so a
// host is never copied or moved once created.
//
// ge:summary The server's host: the classic loop on the run clock, its slots, bots and input log
// ge:keywords host server dedicated-server listen-server slots bots remote connect disconnect
// ge:keywords host_frame host_add_bot host_add_remote host_drop replay_input_log input-log
// ge:keywords cmd-clock bot-packets server-frame
// ge:skill shooter
// ge:test tests/shooters_edge_host_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "shooter/cmd_source.hpp"
#include "shooter/usercmd.hpp"
#include "shooters_edge/server/input_log.hpp"
#include "shooters_edge/sim/arena.hpp"
#include "shooters_edge/sim/rules.hpp"

namespace shooters_edge::server {

enum class SlotKind : uint8_t { empty, remote, bot };

struct HostSettings {
  int maxclients = 8;  // 1..32
  int max_edicts = 256;
  uint32_t seed = 1;  // the run's seed, kept in the log (the server draws nothing)
  std::string map =
      "arena";         // the test arena, roofed: the only map until compiled maps load (M18b)
  bool record = true;  // keep the input log
};

struct BotSlot {
  sh::CmdSource source;
  sh::CmdClock clock;
  sh::MoveCmd recent[3];  // the three newest commands, oldest first
  uint32_t sequence = 0;
};

struct Host {
  Host() = default;
  Host(const Host&) = delete;
  Host& operator=(const Host&) = delete;

  HostSettings settings;
  std::unique_ptr<sim::Arena> arena;  // the models view into it
  std::vector<sh::BrushModel> models;
  sim::Rules rules;
  sh::Server sv;
  std::vector<SlotKind> kinds;      // per slot
  std::vector<BotSlot> bots;        // per slot (only a bot slot's is used)
  std::vector<SlotEvent> pending;   // slot events since the last frame
  std::vector<sh::Packet> packets;  // the frame's packets (reused)
  InputLog log;
};

// A host on its map with no slot taken; false with `error` when the settings cannot run.
bool host_create(Host& h, const HostSettings& settings, std::string& error);

// A slot for a client's packets (the lowest free one); -1 when the server is full.
int host_add_remote(Host& h, bool spectator = false);
// A slot a command source fills; -1 when the server is full.
int host_add_bot(Host& h, sh::CmdSource source);
// Frees a slot (its player leaves the server).
void host_drop(Host& h, int slot);

// One server frame: `remote` are the clients' packets since the last frame, in arrival order.
void host_frame(Host& h, const sh::Packet* remote, int count);

// The slots taken, by kind.
int host_count(const Host& h, SlotKind kind);

// Runs a log's frames again from its settings; the first frame whose hash differs from the log's
// (-1: every frame agrees), with each frame's hash in `hashes` when given. False with `error`
// when the log's settings cannot run.
bool replay_input_log(const InputLog& log, int& first_diverged, std::string& error,
                      std::vector<uint64_t>* hashes = nullptr);

}  // namespace shooters_edge::server
