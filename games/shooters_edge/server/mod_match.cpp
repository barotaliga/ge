// A mod's match with bots (mod_match.hpp): loading the progs, the bots' hands, the run and its
// report.
//
// ge:summary A mod's match with bots: the progs from a path, the bots' commands, the report
// ge:keywords run_mod_match load_mod arena_models bot-command bots digest missile-paths
// ge:skill progs shooter
// ge:test tests/shooters_edge_mod_match_test.cpp
#include "shooters_edge/server/mod_match.hpp"

#include <fstream>
#include <iterator>
#include <sstream>

#include "core/libm/libm.hpp"
#include "core/num.hpp"
#include "core/prng.hpp"
#include "progs/text.hpp"
#include "shooter/client.hpp"
#include "shooters_edge/server/commands.hpp"
#include "shooters_edge/server/test_mod.hpp"
#include "shooters_edge/sim/arena.hpp"

namespace shooters_edge::server {

namespace {

namespace libm = ge::core::libm;

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = 180.0 / kPi;
constexpr float kDoorX = 136;     // the dividing wall's middle; the doorway is at y = -64..64
constexpr float kRunSpeed = 320;  // units/s a bot asks for

struct Bot {
  ge::core::Rng rng{1};
  float yaw = 0, pitch = 0;
  float strafe = 0;  // -1, 0 or 1
  double strafe_until = 0;
  sh::MoveCmd cmds[3];  // the three newest, oldest first
  uint32_t sequence = 0;
};

float wrap180(double a) {
  while (a > 180) a -= 360;
  while (a < -180) a += 360;
  return static_cast<float>(a);
}

float field_float(const QcServer& s, int e, int f) {
  return ge::progs::as_float(
      s.q.vm.entities[static_cast<size_t>(e) * static_cast<size_t>(s.q.vm.entityfields) +
                      static_cast<size_t>(f)]);
}

// One bot's command for this frame.
sh::MoveCmd bot_command(QcServer& s, Bot& b, int slot, int bots) {
  const int me = slot + 1;
  const sh::Edict& ed = s.sv.edicts[static_cast<size_t>(me)];
  const sh::Vec3f at = ed.origin;
  int target = 0;
  double best = 1e30;
  for (int k = 0; k < bots; k++) {
    if (k == slot || !s.clients[k].active) continue;
    const sh::Vec3f o = s.sv.edicts[static_cast<size_t>(k) + 1].origin;
    const double dx = o.x - at.x, dy = o.y - at.y;
    const double d = dx * dx + dy * dy;
    if (d < best) {
      best = d;
      target = k + 1;
    }
  }
  sh::MoveCmd c;
  c.msec = static_cast<uint8_t>(ge::core::num::round(s.sv.set.mintic * 1000));
  if (target == 0) {
    c.angles = {b.pitch, b.yaw, 0};
    return c;
  }
  const sh::Vec3f to = s.sv.edicts[static_cast<size_t>(target)].origin;
  const double dx = to.x - at.x, dy = to.y - at.y;
  const double dist = std::sqrt(dx * dx + dy * dy);

  // The aim: at the target's feet, a little noise from the bot's own generator.
  const double want_yaw = libm::atan2(dy, dx) * kDeg + (ge::core::next(b.rng) - 0.5) * 6;
  const double dz = (to.z - 20) - (at.z + 16);
  const double want_pitch = -libm::atan2(dz, dist) * kDeg;  // down positive
  const float turn = kBotTurn * static_cast<float>(s.sv.set.mintic);
  const float dyaw = wrap180(want_yaw - b.yaw);
  b.yaw = wrap180(b.yaw + (dyaw > turn ? turn : dyaw < -turn ? -turn : dyaw));
  const float dpitch = static_cast<float>(want_pitch) - b.pitch;
  b.pitch += dpitch > turn ? turn : dpitch < -turn ? -turn : dpitch;
  c.angles = {b.pitch, b.yaw, 0};

  // The way: straight at the target, or the doorway first when the wall is between them.
  double gx = to.x, gy = to.y;
  const bool other_side = (at.x < kDoorX) != (to.x < kDoorX);
  if (other_side && (at.y < -40 || at.y > 40)) {
    gx = at.x < kDoorX ? kDoorX - 48 : kDoorX + 48;
    gy = 0;
  }
  const double move_yaw = libm::atan2(gy - at.y, gx - at.x) * kDeg;
  const double rel = (move_yaw - b.yaw) / kDeg;
  double fwd = libm::cos(rel), side = -libm::sin(rel);
  if (!other_side && dist < 160) {
    fwd = -fwd;
    side = -side;
  }
  if (s.sv.time >= b.strafe_until) {
    const double r = ge::core::next(b.rng);
    b.strafe = r < 0.35 ? -1.0f : r < 0.7 ? 1.0f : 0.0f;
    b.strafe_until = s.sv.time + 0.6 + ge::core::next(b.rng) * 1.2;
  }
  side += b.strafe * 0.7;
  c.forward = static_cast<int16_t>(fwd * kRunSpeed);
  c.side = static_cast<int16_t>(side * kRunSpeed);
  if (std::fabs(dyaw) < 8 && !other_side) c.buttons = 1;
  return c;
}

uint64_t fold(uint64_t h, uint32_t w) {
  for (int i = 0; i < 4; i++) {
    h ^= (w >> (8 * i)) & 0xffu;
    h *= 1099511628211ull;
  }
  return h;
}

// The loop's hash folded with the program's globals and every entity in use.
uint64_t digest(const QcServer& s) {
  uint64_t h = sh::server_hash(s.sv);
  const ge::progs::Vm& vm = s.q.vm;
  for (uint32_t w : vm.globals) h = fold(h, w);
  const size_t words = static_cast<size_t>(s.sv.num_edicts) * static_cast<size_t>(vm.entityfields);
  for (size_t i = 0; i < words && i < vm.entities.size(); i++) h = fold(h, vm.entities[i]);
  return h;
}

bool is_missile(const QcServer& s, int e) {
  if (s.sv.edicts[static_cast<size_t>(e)].free) return false;
  const int32_t cn = static_cast<int32_t>(
      s.q.vm.entities[static_cast<size_t>(e) * static_cast<size_t>(s.q.vm.entityfields) +
                      static_cast<size_t>(ge::progs::kFClassname)]);
  const char* name = ge::progs::vm_string(s.q.vm, cn);
  return name != nullptr && std::string(name) == "missile";
}

// `name value` or `set name value` naming sv_progs among the console lines: its value.
std::string progs_from_console(const std::vector<std::string>& lines) {
  std::string path;
  ge::progs::Words w;
  for (const std::string& l : lines) {
    ge::progs::tokenize_words(l.c_str(), w);
    if (w.count >= 2 && std::string(ge::progs::word(w, 0)) == "sv_progs")
      path = ge::progs::word(w, 1);
    if (w.count >= 3 && std::string(ge::progs::word(w, 0)) == "set" &&
        std::string(ge::progs::word(w, 1)) == "sv_progs")
      path = ge::progs::word(w, 2);
  }
  return path;
}

}  // namespace

std::vector<sh::BrushModel> arena_models() {
  static const sim::Arena arena = [] {
    sim::Arena a;
    sim::build_arena(a);
    return a;
  }();
  return {arena.models()[0]};
}

bool load_mod(const std::string& path, ge::progs::Progs& out, std::string& source,
              std::string& error) {
  if (path.empty()) {
    source = "the test mod";
    return build_test_mod(out, error);
  }
  source = path;
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    error = "load_mod: cannot read the progs " + path +
            "; next: check the path (relative to where the game runs), or leave --progs and "
            "sv_progs empty for the test mod";
    return false;
  }
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),
                                   std::istreambuf_iterator<char>());
  if (!ge::progs::load_progs(bytes.data(), bytes.size(), out, error)) {
    error = "load_mod: " + path + ": " + error + "; next: `ge qc disasm " + path +
            "` names what the checker refused";
    return false;
  }
  std::ifstream lf(path + ".lines");
  if (lf) {
    std::stringstream ss;
    ss << lf.rdbuf();
    std::vector<int32_t> lines;
    if (!ge::progs::parse_lines(ss.str(), lines, error) ||
        !ge::progs::set_lines(out, std::move(lines), error)) {
      error = "load_mod: " + path + ".lines: " + error +
              "; next: write the line table again beside the progs, or delete it";
      return false;
    }
  }
  return true;
}

ModMatchReport run_mod_match(const ModMatchOptions& opt) {
  ModMatchReport r;
  if (opt.bots < 1 || opt.bots > sh::kMaxClients) {
    r.error = "run_mod_match: bots must be 1..32; next: pass --bots=4";
    return r;
  }
  std::string path = opt.progs.empty() ? progs_from_console(opt.console) : opt.progs;
  ge::progs::Progs progs;
  if (!load_mod(path, progs, r.source, r.error)) return r;

  QcServerOptions so;
  so.seed = opt.seed;
  so.loop.max_edicts = 1024;
  std::string error;
  std::unique_ptr<QcServer> sp = make_qc_server(progs, so, error);
  if (!sp) {
    r.error = error;
    return r;
  }
  QcServer& s = *sp;
  for (const std::string& line : opt.console) server_command(s, line);
  s.cvars.set("sv_progs", path, ge::console::Source::code);

  MapSpec map;
  map.name = "arena";
  map.models = arena_models();
  map.entities = kArenaEntities;
  if (!load_map(s, map, error)) {
    r.error = error + "; next: read the map's messages (" + std::to_string(s.map_messages.size()) +
              ") and the console";
    r.console = s.console;
    r.map_messages = s.map_messages;
    return r;
  }

  std::vector<Bot> bots(static_cast<size_t>(opt.bots));
  ge::core::Rng seeds = ge::core::make_rng(opt.seed + 1);
  r.players.resize(bots.size());
  for (int k = 0; k < opt.bots; k++) {
    bots[static_cast<size_t>(k)].rng =
        ge::core::make_rng(static_cast<uint32_t>(ge::core::next(seeds) * 4294967296.0));
    r.players[static_cast<size_t>(k)].name = "bot" + std::to_string(k + 1);
    connect(s, k, r.players[static_cast<size_t>(k)].name, false, true);
    bots[static_cast<size_t>(k)].yaw = s.sv.edicts[static_cast<size_t>(k) + 1].angles.y;
  }

  std::vector<int> live(static_cast<size_t>(s.sv.set.max_edicts), -1);  // entity -> missile
  std::vector<sh::Packet> packets(bots.size());
  for (int frame = 0; frame < opt.max_frames; frame++) {
    for (int k = 0; k < opt.bots; k++) {
      Bot& b = bots[static_cast<size_t>(k)];
      b.cmds[0] = b.cmds[1];
      b.cmds[1] = b.cmds[2];
      b.cmds[2] = bot_command(s, b, k, opt.bots);
      sh::Packet& p = packets[static_cast<size_t>(k)];
      p.client = k;
      p.sequence = ++b.sequence;
      for (int i = 0; i < 3; i++) p.cmds[i] = b.cmds[i];
    }
    qc_frame(s, packets.data(), static_cast<int>(packets.size()));
    r.frames = frame + 1;
    r.end_time = s.sv.time;

    for (int k = 0; k < opt.bots; k++) {
      PlayerPath& pp = r.players[static_cast<size_t>(k)];
      pp.points.push_back(s.sv.edicts[static_cast<size_t>(k) + 1].origin);
      pp.frags_at.push_back(field_float(s, k + 1, ge::progs::kFFrags));
    }
    for (int e = sh::kMaxClients + 1; e < s.sv.num_edicts; e++) {
      int& m = live[static_cast<size_t>(e)];
      if (is_missile(s, e)) {
        if (m < 0) {
          m = static_cast<int>(r.missiles.size());
          r.missiles.push_back({e, frame, {}});
        }
        r.missiles[static_cast<size_t>(m)].points.push_back(
            s.sv.edicts[static_cast<size_t>(e)].origin);
      } else {
        m = -1;
      }
    }
    for (const NetEvent& ev : s.out.events) {
      if (ev.kind == EventKind::print) r.prints.push_back(ev.text);
      if (ev.kind == EventKind::multicast)
        r.multicast_bytes += static_cast<int64_t>(ev.bytes.size());
    }
    clear_frame(s.out);
    r.digests.push_back(digest(s));
    if (s.map_stopped) {
      r.error = "the mod stopped the map: " + s.stop_reason +
                "; next: read the console's last lines and the fault's QC stack";
      break;
    }
    if (!s.pending_map.empty()) {
      r.ended = true;
      break;
    }
  }

  for (int k = 0; k < opt.bots; k++) {
    PlayerPath& pp = r.players[static_cast<size_t>(k)];
    pp.frags = field_float(s, k + 1, ge::progs::kFFrags);
  }
  // Deaths off the frag log: a line is "\killer\victim\" (the long form goes on).
  for (const std::string& line : s.frag_log) {
    const size_t a = line.find('\\', 1);
    const size_t b = a == std::string::npos ? a : line.find('\\', a + 1);
    if (b == std::string::npos) continue;
    const std::string victim = line.substr(a + 1, b - a - 1);
    for (PlayerPath& pp : r.players)
      if (pp.name == victim) pp.deaths++;
  }
  r.hook_log = s.hook_log;
  for (int i = 0; i < static_cast<int>(HookId::count); i++) r.hook_counts[i] = s.hook_counts[i];
  const ge::progs::Vm& vm = s.q.vm;
  for (size_t n = 1; n < vm.builtin_calls.size(); n++) {
    if (vm.builtin_calls[n] == 0) continue;
    const char* name = n < vm.builtins.size() ? vm.builtins[n].name : "?";
    r.builtins.push_back({static_cast<int>(n), name, vm.builtin_calls[n]});
  }
  r.frag_log = s.frag_log;
  r.console = s.console;
  r.map_messages = s.map_messages;
  r.events_total = s.out.events_total;
  r.faults = s.q.faults;
  r.last_fault = s.q.last_fault;
  r.inhibited = s.inhibited;
  r.statements = vm.statements_run;
  r.final_digest = r.digests.empty() ? 0 : r.digests.back();
  r.ok = r.error.empty();
  return r;
}

}  // namespace shooters_edge::server
