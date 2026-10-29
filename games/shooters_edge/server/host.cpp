// The server's host: slots, the bots' packets and the frame (host.hpp states the order).
//
// ge:summary The server's host: slots taken and freed, bots' packets, the frame, the replay
// ge:keywords host host_frame bot-packets slots replay input-log
// ge:skill shooter
// ge:test tests/shooters_edge_host_test.cpp
#include "shooters_edge/server/host.hpp"

#include "shooter/frame.hpp"

namespace shooters_edge::server {

namespace {

bool create_world(Host& h, const HostSettings& s, std::string& error) {
  if (s.maxclients < 1 || s.maxclients > sh::kMaxClients) {
    error = "maxclients " + std::to_string(s.maxclients) + " is out of range; next: pass 1.." +
            std::to_string(sh::kMaxClients);
    return false;
  }
  if (s.max_edicts < s.maxclients + 16 || s.max_edicts > sh::kMaxEdicts) {
    error = "max_edicts " + std::to_string(s.max_edicts) + " is out of range; next: pass " +
            std::to_string(s.maxclients + 16) + ".." + std::to_string(sh::kMaxEdicts);
    return false;
  }
  if (s.map != "arena") {
    error = "map '" + s.map +
            "' cannot load: only the test arena runs until compiled maps "
            "load (M18b); next: pass --map=arena";
    return false;
  }
  h.settings = s;
  h.arena = std::make_unique<sim::Arena>();
  sim::build_arena(*h.arena, /*roofed=*/true);
  h.models = h.arena->models();
  sh::ServerSettings set;
  set.maxclients = s.maxclients;
  set.max_edicts = s.max_edicts;
  sim::create_rules(h.rules, *h.arena, set.max_edicts);
  sh::create_server(h.sv, set, h.models, sim::rules_program(h.rules));
  sim::spawn_arena_entities(h.sv, h.rules);
  h.kinds.assign(static_cast<size_t>(s.maxclients), SlotKind::empty);
  h.bots.assign(static_cast<size_t>(s.maxclients), BotSlot{});
  h.pending.clear();
  h.packets.clear();
  h.packets.reserve(static_cast<size_t>(s.maxclients) * 4);
  h.log = InputLog{};
  h.log.settings = {s.maxclients, s.max_edicts, s.seed, s.map};
  return true;
}

int free_slot(const Host& h) {
  for (size_t i = 0; i < h.kinds.size(); i++)
    if (h.kinds[i] == SlotKind::empty) return static_cast<int>(i);
  return -1;
}

void take(Host& h, int slot, SlotKind kind, bool spectator) {
  h.kinds[static_cast<size_t>(slot)] = kind;
  uint8_t flags = 0;
  if (spectator) flags |= kSlotSpectator;
  if (kind == SlotKind::bot) flags |= kSlotBot;
  h.pending.push_back({SlotEventKind::connect, static_cast<uint8_t>(slot), flags});
  sh::connect_client(h.sv, slot, spectator, kind == SlotKind::bot);
}

// A bot's packet when its clock comes due this frame.
bool bot_packet(Host& h, int slot, double frame_s, sh::Packet& out) {
  BotSlot& b = h.bots[static_cast<size_t>(slot)];
  if (!sh::cmd_due(b.clock, frame_s)) return false;
  const uint8_t msec = sh::next_msec(b.clock);
  sh::MoveCmd c = b.source.next(b.source.ctx, h.sv, slot, msec);
  c.msec = msec;
  c.angles = {sh::quantize_angle(c.angles.x), sh::quantize_angle(c.angles.y),
              sh::quantize_angle(c.angles.z)};
  c.forward = sh::quantize_axis(c.forward);
  c.side = sh::quantize_axis(c.side);
  c.up = sh::quantize_axis(c.up);
  b.recent[0] = b.recent[1];
  b.recent[1] = b.recent[2];
  b.recent[2] = c;
  b.sequence++;
  out.client = slot;
  out.sequence = b.sequence;
  for (int i = 0; i < 3; i++) out.cmds[i] = b.recent[i];
  return true;
}

void log_frame(InputLog& log, const std::vector<SlotEvent>& events,
               const std::vector<sh::Packet>& packets, uint64_t hash) {
  LogFrame f;
  f.first_event = static_cast<uint32_t>(log.events.size());
  f.events = static_cast<uint32_t>(events.size());
  f.first_packet = static_cast<uint32_t>(log.packets.size());
  f.packets = static_cast<uint32_t>(packets.size());
  f.hash = hash;
  log.events.insert(log.events.end(), events.begin(), events.end());
  log.packets.insert(log.packets.end(), packets.begin(), packets.end());
  log.frames.push_back(f);
}

}  // namespace

bool host_create(Host& h, const HostSettings& settings, std::string& error) {
  return create_world(h, settings, error);
}

int host_add_remote(Host& h, bool spectator) {
  const int slot = free_slot(h);
  if (slot >= 0) take(h, slot, SlotKind::remote, spectator);
  return slot;
}

int host_add_bot(Host& h, sh::CmdSource source) {
  const int slot = free_slot(h);
  if (slot < 0 || source.next == nullptr) return -1;
  BotSlot& b = h.bots[static_cast<size_t>(slot)];
  b = BotSlot{};
  b.source = source;
  b.clock = sh::create_cmd_clock();
  take(h, slot, SlotKind::bot, false);
  return slot;
}

void host_drop(Host& h, int slot) {
  if (slot < 0 || slot >= static_cast<int>(h.kinds.size())) return;
  if (h.kinds[static_cast<size_t>(slot)] == SlotKind::empty) return;
  h.kinds[static_cast<size_t>(slot)] = SlotKind::empty;
  h.pending.push_back({SlotEventKind::disconnect, static_cast<uint8_t>(slot), 0});
  sh::disconnect_client(h.sv, slot);
}

void host_frame(Host& h, const sh::Packet* remote, int count) {
  h.packets.clear();
  for (int i = 0; i < count; i++)
    if (remote[i].client >= 0 && remote[i].client < static_cast<int>(h.kinds.size()) &&
        h.kinds[static_cast<size_t>(remote[i].client)] == SlotKind::remote)
      h.packets.push_back(remote[i]);
  const double frame_s = h.sv.set.mintic;
  for (int slot = 0; slot < static_cast<int>(h.kinds.size()); slot++) {
    if (h.kinds[static_cast<size_t>(slot)] != SlotKind::bot) continue;
    sh::Packet p;
    if (bot_packet(h, slot, frame_s, p)) h.packets.push_back(p);
  }
  sh::server_frame(h.sv, h.packets.data(), static_cast<int>(h.packets.size()));
  if (h.settings.record) log_frame(h.log, h.pending, h.packets, sh::server_hash(h.sv));
  h.pending.clear();
}

int host_count(const Host& h, SlotKind kind) {
  int n = 0;
  for (const SlotKind k : h.kinds) n += k == kind ? 1 : 0;
  return n;
}

bool replay_input_log(const InputLog& log, int& first_diverged, std::string& error,
                      std::vector<uint64_t>* hashes) {
  first_diverged = -1;
  Host h;
  HostSettings s;
  s.maxclients = log.settings.maxclients;
  s.max_edicts = log.settings.max_edicts;
  s.seed = log.settings.seed;
  s.map = log.settings.map;
  s.record = false;
  if (!create_world(h, s, error)) return false;
  if (hashes != nullptr) {
    hashes->clear();
    hashes->reserve(log.frames.size());
  }
  for (size_t f = 0; f < log.frames.size(); f++) {
    const LogFrame& lf = log.frames[f];
    for (uint32_t i = 0; i < lf.events; i++) {
      const SlotEvent& e = log.events[lf.first_event + i];
      if (e.kind == SlotEventKind::connect)
        sh::connect_client(h.sv, e.slot, (e.flags & kSlotSpectator) != 0,
                           (e.flags & kSlotBot) != 0);
      else
        sh::disconnect_client(h.sv, e.slot);
    }
    sh::server_frame(h.sv, log.packets.data() + lf.first_packet, static_cast<int>(lf.packets));
    const uint64_t hash = sh::server_hash(h.sv);
    if (hashes != nullptr) hashes->push_back(hash);
    if (hash != lf.hash && first_diverged < 0) first_diverged = static_cast<int>(f);
  }
  return true;
}

}  // namespace shooters_edge::server
