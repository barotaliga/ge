// Quake II's net server (net/q2_net_server.hpp) to clients of its later protocols: a raw client of
// protocol 36 on the newer channel and one of 35 on the old channel join over the simulated
// network: the challenge lists every protocol, the connect is answered with the channel kind, the
// signon comes whole (streams or batches, compressed), the frames come in the later form, the
// batched or compact moves walk the player, and the client settings leave out what they ask.
//
// ge:summary Test: Quake II's server to protocol 35 and 36 clients, joined and walking
// ge:keywords test protocol-35 protocol-36 newer-channel zpacket gamestate batched-moves
// ge:skill net shooter testing
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classic/protocol/channel.hpp"
#include "classic/protocol/wire.hpp"
#include "core/inflate.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "server/settingban.hpp"
#include "shooters_edge/net/q2_net.hpp"
#include "shooters_edge/net/q2_net_ext.hpp"
#include "shooters_edge/net/q2_net_server.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace gn = ge::net;
namespace sp = ge::classic::protocol;
namespace sv = shooters_edge::server;
namespace cn = shooters_edge::net;

std::shared_ptr<const br::Map> hall() {
  br::BrushWorld w;
  w.models.resize(1);
  auto box = [](float x0, float y0, float z0, float x1, float y1, float z1) {
    const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
    return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
  };
  auto& m = w.models[0];
  m.push_back(box(-1024, -1024, -16, 1024, 1024, 0));
  m.push_back(box(-1040, -1024, -16, -1024, 1024, 256));
  m.push_back(box(1024, -1024, -16, 1040, 1024, 256));
  m.push_back(box(-1040, 1024, -16, 1040, 1040, 256));
  m.push_back(box(-1040, -1040, -16, 1040, -1024, 256));
  br::Entity spot;
  spot.pairs = {
      {"classname", "info_player_deathmatch"}, {"origin", "-400 -400 24"}, {"angle", "0"}};
  w.entities.push_back(spot);
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

// A client of a later protocol, written from the protocol's facts alone.
struct Raw {
  gn::Transport t;
  gn::Address server;
  int protocol = 36, minor = 1026, chan = 1;
  uint16_t qport = 77;
  sp::Channel ch;
  bool connected = false, begun = false;
  std::string connect_answer, refused;
  std::vector<std::string> prints;
  int server_minor = -1, server_state = -1;
  int config_strings = 0, baselines = 0, zpackets = 0, streams = 0;
  int frames = 0;
  int32_t last_frame = -1;
  int16_t origin[3] = {};
  bool saw_gun_index = false, saw_view_angles = false;
  std::vector<std::string> reliable_lines;  // string commands to send
  int64_t now = 0;
};

void send_oob(Raw& c, const std::string& text) {
  std::vector<uint8_t> b = cn::q2_oob(text);
  gn::transport_send(c.t, c.server, b.data(), b.size(), c.now);
}

void send_cmd(Raw& c, const std::string& line) {
  std::vector<uint8_t> b = {cn::kClcStringCmd};
  b.insert(b.end(), line.begin(), line.end());
  b.push_back(0);
  sp::channel_reliable(c.ch, b.data(), b.size());
}

// A player state of 35 or 36 (no id), the extra flags given.
bool player_state(Raw& c, gn::Reader& r, int extra) {
  const int f = sp::read_short(r) & 0xFFFF;
  if (f & (1 << 0)) sp::read_byte(r);
  if (f & (1 << 1)) {
    c.origin[0] = static_cast<int16_t>(sp::read_short(r));
    c.origin[1] = static_cast<int16_t>(sp::read_short(r));
  }
  if (extra & (1 << 3)) c.origin[2] = static_cast<int16_t>(sp::read_short(r));
  if (f & (1 << 2)) {
    sp::read_short(r);
    sp::read_short(r);
  }
  if (extra & (1 << 2)) sp::read_short(r);
  if (f & (1 << 3)) sp::read_byte(r);
  if (f & (1 << 4)) sp::read_byte(r);
  if (f & (1 << 5)) sp::read_short(r);
  if (f & (1 << 6))
    for (int k = 0; k < 3; k++) sp::read_short(r);
  if (f & (1 << 7))
    for (int k = 0; k < 3; k++) sp::read_byte(r);
  if (f & (1 << 8)) {
    c.saw_view_angles = true;
    sp::read_short(r);
    sp::read_short(r);
  }
  if (extra & (1 << 4)) sp::read_short(r);
  if (f & (1 << 9))
    for (int k = 0; k < 3; k++) sp::read_byte(r);
  if (f & (1 << 12)) {
    c.saw_gun_index = true;
    sp::read_byte(r);
  }
  if (f & (1 << 13)) sp::read_byte(r);
  if (extra & (1 << 0))
    for (int k = 0; k < 3; k++) sp::read_byte(r);
  if (extra & (1 << 1))
    for (int k = 0; k < 3; k++) sp::read_byte(r);
  if (f & (1 << 10))
    for (int k = 0; k < 4; k++) sp::read_byte(r);
  if (f & (1 << 11)) sp::read_byte(r);
  if (f & (1 << 14)) sp::read_byte(r);
  if (extra & (1 << 5)) {
    const uint32_t mask = static_cast<uint32_t>(sp::read_long(r));
    for (int i = 0; i < 32; i++)
      if (mask & (1u << i)) sp::read_short(r);
  }
  return !r.bad;
}

// One entity entry; its number (0 the list's end), -1 when it reads wrong.
int entity(gn::Reader& r, bool long_solid) {
  {
    uint32_t bits = static_cast<uint32_t>(sp::read_byte(r));
    if (bits & 0x80) bits |= static_cast<uint32_t>(sp::read_byte(r)) << 8;
    if (bits & 0x8000) bits |= static_cast<uint32_t>(sp::read_byte(r)) << 16;
    if (bits & 0x800000) bits |= static_cast<uint32_t>(sp::read_byte(r)) << 24;
    const int number = (bits & 0x100) ? sp::read_short(r) : sp::read_byte(r);
    if (r.bad) return -1;
    if (number == 0 || (bits & 0x40) != 0) return number;  // the end, or removed
    const auto sized = [&r](bool lo, bool hi) {
      if (lo && hi)
        sp::read_long(r);
      else if (hi)
        sp::read_short(r);
      else if (lo)
        sp::read_byte(r);
    };
    for (const uint32_t m : {1u << 11, 1u << 20, 1u << 21, 1u << 22})
      if (bits & m) sp::read_byte(r);
    if (bits & (1u << 4)) sp::read_byte(r);
    if (bits & (1u << 17)) sp::read_short(r);
    sized(bits & (1u << 16), bits & (1u << 25));
    sized(bits & (1u << 14), bits & (1u << 19));
    sized(bits & (1u << 12), bits & (1u << 18));
    for (const uint32_t m : {1u << 0, 1u << 1, 1u << 9})
      if (bits & m) sp::read_short(r);
    for (const uint32_t m : {1u << 10, 1u << 2, 1u << 3})
      if (bits & m) sp::read_byte(r);
    if (bits & (1u << 24))
      for (int k = 0; k < 3; k++) sp::read_short(r);
    if (bits & (1u << 26)) sp::read_byte(r);
    if (bits & (1u << 5)) sp::read_byte(r);
    if (bits & (1u << 27)) {
      if (long_solid)
        sp::read_long(r);
      else
        sp::read_short(r);
    }
    return r.bad ? -1 : number;
  }
}

// An entity list (no id) to its end; false when it reads wrong.
bool entities(gn::Reader& r, bool long_solid) {
  for (;;) {
    const int number = entity(r, long_solid);
    if (number < 0) return false;
    if (number == 0) return true;
  }
}

// The server's messages in `r` until the frame has been read (what follows it is not looked at).
void messages(Raw& c, gn::Reader& r) {
  while (gn::remaining(r) > 0) {
    const int cmd = sp::read_byte(r);
    const int id = cmd & 0x1F;
    if (id == 21) {  // zpacket
      const int in = sp::read_short(r) & 0xFFFF, out = sp::read_short(r) & 0xFFFF;
      const uint8_t* z = gn::read_view(r, static_cast<size_t>(in));
      CHECK(z != nullptr);
      if (z == nullptr) return;
      const auto plain =
          ge::core::inflate_raw(std::span<const uint8_t>(z, size_t(in)), size_t(out));
      CHECK(plain.has_value());
      if (!plain) return;
      c.zpackets++;
      gn::Reader inner = gn::make_reader(plain->data(), plain->size());
      messages(c, inner);
    } else if (id == 10) {
      sp::read_byte(r);
      c.prints.push_back(sp::read_string(r));
    } else if (id == 11) {
      const std::string line = sp::read_string(r);
      if (line.rfind("cmd ", 0) == 0) {
        std::string rest = line.substr(4);
        while (!rest.empty() && rest.back() == '\n') rest.pop_back();
        const size_t v = rest.find("$version");
        if (v != std::string::npos) rest.replace(v, 8, "Raw client 1.0");
        c.reliable_lines.push_back(rest);
      } else if (line.rfind("precache ", 0) == 0) {
        c.reliable_lines.push_back("begin " + line.substr(9, line.find('\n') - 9));
        c.begun = true;
      }
    } else if (id == 12) {  // serverdata
      sp::read_long(r);
      sp::read_long(r);
      sp::read_byte(r);
      sp::read_string(r);
      sp::read_short(r);
      sp::read_string(r);
      if (c.protocol == 36) {
        c.server_minor = sp::read_short(r);
        c.server_state = sp::read_byte(r);
        if (c.server_minor >= 1024)
          sp::read_short(r);
        else
          for (int k = 0; k < 3; k++) sp::read_byte(r);
      } else {
        sp::read_byte(r);
        c.server_minor = sp::read_short(r);
        sp::read_byte(r);
        sp::read_byte(r);
      }
    } else if (id == 13) {  // a configstring
      sp::read_short(r);
      sp::read_string(r);
      c.config_strings++;
    } else if (id == 14) {  // a baseline: one entity entry
      CHECK(entity(r, c.protocol == 36 || c.minor >= 1905) > 0);
      c.baselines++;
    } else if (id == 25 || id == 23) {  // configstring stream (or the game state's first half)
      c.streams++;
      for (;;) {
        const int index = sp::read_short(r) & 0xFFFF;
        if (r.bad || index == sv::kQ2ConfigStrings) break;
        sp::read_string(r);
        c.config_strings++;
      }
      if (id == 23) {
        CHECK(entities(r, true));
        c.baselines++;
      }
    } else if (id == 26) {
      c.streams++;
      CHECK(entities(r, true));
      c.baselines++;
    } else if (id == 20) {
      const int extra_hi = cmd >> 5;
      const uint32_t word = static_cast<uint32_t>(sp::read_long(r));
      const int flags = sp::read_byte(r);
      const int extra = (flags >> 4) | (extra_hi << 4);
      const int areas = sp::read_byte(r);
      for (int k = 0; k < areas; k++) sp::read_byte(r);
      CHECK(player_state(c, r, extra));
      if (extra & (1 << 6)) {
        if (c.protocol == 36 && c.minor >= 1022)
          sp::read_short(r);
        else
          sp::read_byte(r);
      }
      const bool long_solid = c.protocol == 36 || c.minor >= 1905;
      CHECK(entities(r, long_solid));
      c.frames++;
      c.last_frame = static_cast<int32_t>(word & 0x07FFFFFFu);
      return;
    } else {
      return;  // something the test does not read: the rest of this datagram
    }
    if (r.bad) return;
  }
}

void raw_tick(Raw& c, int64_t now, const sh::MoveCmd* walk) {
  c.now = now;
  uint8_t buf[gn::kMaxDatagram];
  gn::Address from;
  for (;;) {
    const int n = gn::transport_receive(c.t, buf, sizeof buf, from, now);
    if (n <= 0) break;
    const auto size = static_cast<size_t>(n);
    if (sp::is_out_of_band(buf, size)) {
      const std::string text = cn::q2_oob_text(buf, size);
      const std::vector<std::string> w = cn::q2_words(text);
      if (w.empty()) continue;
      if (w[0] == "challenge") {
        CHECK(text.find("p=34,35,36") != std::string::npos);
        std::string connect = "connect " + std::to_string(c.protocol) + " " +
                              std::to_string(c.qport) + " " + w[1] +
                              " \"\\name\\Raw\\rate\\25000\" 1390";
        if (c.protocol == 36)
          connect += " " + std::to_string(c.chan) + " 1 " + std::to_string(c.minor);
        else
          connect += " " + std::to_string(c.minor);
        send_oob(c, connect);
      } else if (w[0] == "client_connect") {
        c.connect_answer = text;
        c.connected = true;
        sp::channel_setup(c.ch, true, c.qport, static_cast<double>(now) / 1e6);
        c.ch.qport_bytes = 1;
        c.ch.fragments = c.protocol == 36 && c.chan == 1;
        c.ch.packet_len = 1390;
        send_cmd(c, "new");
      } else if (w[0] == "print") {
        c.refused = text.substr(6);
      }
      continue;
    }
    if (!c.connected) continue;
    std::span<const uint8_t> payload;
    if (!sp::channel_read(c.ch, buf, size, static_cast<double>(now) / 1e6, payload)) continue;
    gn::Reader r = gn::make_reader(payload.data(), payload.size());
    messages(c, r);
  }
  if (!c.connected) return;
  for (const std::string& line : c.reliable_lines) send_cmd(c, line);
  c.reliable_lines.clear();
  uint8_t move[256];
  gn::Writer w = gn::make_writer(move, sizeof move);
  if (c.frames > 0 && walk != nullptr) {
    cn::Q2Cmd cmd = cn::q2_cmd(*walk);
    cmd.msec = 13;
    if (c.protocol == 36) {
      cn::Q2Batch b;
      b.frame = c.last_frame;
      b.frames = {{cmd}};
      cn::write_q2_batch(w, b);
    } else {
      const cn::Q2Cmd cmds[3] = {cmd, cmd, cmd};
      cn::write_q2_move_ext(w, c.protocol, c.minor, c.last_frame, cmds);
    }
  }
  uint8_t out[sp::kMaxPacket];
  const size_t n = sp::channel_transmit(c.ch, move, w.size, static_cast<double>(now) / 1e6, out);
  gn::transport_send(c.t, c.server, out, n, now);
}

struct Rig {
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st;
  cn::Q2NetServer server;
  std::vector<std::unique_ptr<Raw>> clients;
  int64_t now = 0;
};

std::unique_ptr<Rig> rig(const cn::Q2NetConfig& cfg) {
  auto r = std::make_unique<Rig>();
  r->rules = sv::q2_runner(sv::make_q2_server({}));
  r->rules.console("deathmatch 1");
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall();
  std::string error;
  CHECK_MSG(r->rules.start(map, error), error);
  gn::LinkConditions link;
  CHECK(gn::conditions_preset("lan", link));
  r->net = gn::make_sim_network(7, link);
  gn::open_sim_transport(r->st, r->net, gn::sim_address(0, cn::kPortQ2));
  CHECK_MSG(cn::q2_net_start(r->server, r->rules, r->st, cfg, error), error);
  return r;
}

Raw& join(Rig& r, int protocol, int minor, int chan) {
  const int n = static_cast<int>(r.clients.size());
  r.clients.push_back(std::make_unique<Raw>());
  Raw& c = *r.clients.back();
  c.protocol = protocol;
  c.minor = minor;
  c.chan = chan;
  gn::open_sim_transport(c.t, r.net, gn::sim_address(uint32_t(n + 1), uint16_t(27901 + n)));
  c.server = gn::sim_address(0, cn::kPortQ2);
  send_oob(c, "getchallenge");
  return c;
}

void run(Rig& r, int ticks, const sh::MoveCmd* walk = nullptr) {
  for (int i = 0; i < ticks; i++) {
    r.now += 13000;
    gn::sim_advance(r.net, r.now);
    for (auto& c : r.clients) raw_tick(*c, r.now, walk);
    cn::q2_net_frame(r.server, r.now);
  }
}

}  // namespace

TEST(a_protocol_36_client_on_the_newer_channel_joins_and_walks) {
  auto r = rig({});
  Raw& c = join(*r, 36, 1026, 1);
  run(*r, 120);
  CHECK(c.connect_answer == "client_connect nc=1");
  CHECK(c.server_minor == 1026 && c.server_state == 2);
  CHECK(c.streams >= 2);   // configstrings then baselines, as streams
  CHECK(c.zpackets >= 1);  // compressed
  CHECK(c.config_strings > 5);
  CHECK(c.begun);
  CHECK_MSG(c.frames > 3, "frames: " + std::to_string(c.frames));
  CHECK(r->server.clients[0].stage == cn::Q2Slot::active);
  CHECK(r->server.clients[0].protocol == 36 && r->server.clients[0].ch.fragments);
  const int16_t x0 = c.origin[0];
  sh::MoveCmd walk;
  walk.forward = 300;
  run(*r, 120, &walk);
  CHECK_MSG(c.origin[0] > x0 + 8 * 50,
            "x " + std::to_string(x0) + " -> " + std::to_string(c.origin[0]));
  CHECK(r->server.clients[0].stage == cn::Q2Slot::active);
  // Alive and predicting: the view angles are left out.
  CHECK(!c.saw_view_angles);
}

TEST(a_protocol_36_client_of_an_older_minor_gets_one_game_state) {
  auto r = rig({});
  Raw& c = join(*r, 36, 1020, 1);
  run(*r, 120);
  CHECK(c.server_minor == 1020);
  CHECK(c.streams == 1);  // the game state alone
  CHECK(c.begun && c.frames > 3);
}

TEST(a_protocol_35_client_on_the_old_channel_joins_and_walks) {
  auto r = rig({});
  Raw& c = join(*r, 35, 1905, 0);
  run(*r, 120);
  CHECK(c.connect_answer == "client_connect");
  CHECK(c.server_minor == 1905);
  CHECK(c.config_strings > 5);
  CHECK(c.zpackets >= 1);
  CHECK(c.begun && c.frames > 3);
  CHECK(!r->server.clients[0].ch.fragments);
  const int16_t x0 = c.origin[0];
  sh::MoveCmd walk;
  walk.forward = 300;
  run(*r, 120, &walk);
  CHECK_MSG(c.origin[0] > x0 + 8 * 50,
            "x " + std::to_string(x0) + " -> " + std::to_string(c.origin[0]));
}

TEST(a_bad_connect_tail_is_refused_in_the_servers_words) {
  auto r = rig({});
  Raw& c = join(*r, 36, 1026, 2);
  run(*r, 20);
  CHECK(c.refused == "Invalid netchan type.\n");
  CHECK(!c.connected);
}

TEST(status_shows_the_later_protocols_numbers) {
  auto r = rig({});
  join(*r, 36, 1024, 1);
  join(*r, 35, 1904, 0);
  run(*r, 120);
  const std::string misc = cn::q2_misc_lines(r->server, 0);
  CHECK(misc.find("36/1024") != std::string::npos);
  CHECK(misc.find("new") != std::string::npos);
  const std::string misc35 = cn::q2_misc_lines(r->server, 1);
  CHECK(misc35.find("35/1904") != std::string::npos);
}
