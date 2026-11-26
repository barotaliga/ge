// LAB `se-remix-q1-arms`: QUAKE'S WEAPONS FIRED WHOLE BY QUAKE II'S RULES (classic remix's
// `cr_items q1`, server/q2_quake_arms.hpp). Each of Quake's eight is held for three seconds by a
// player on Quake II's rules wearing Quake (`sv_wear q1`), a target standing ahead, and measured:
// the shots, the time between them, the ammunition, the damage the target took, the sound each
// shot made and the looks its hits were drawn by. Beside each, the weapon of Quake II that stood
// for it before (Quake II's weapon in Quake's files) measured the same way: the period, the damage
// and the looks that were Quake II's. The picture puts the time between shots side by side:
// Quake's own number, the weapon fired now, the renamed one before. A second picture follows the
// looks over the Quake II wire: what reached a remote client before, and a client of ours now.
//
// ge:summary Lab: Quake's eight weapons on Quake II's rules measured against Quake's numbers
// ge:keywords lab se-remix-q1-arms cr_items sv_wear q1-arms quake-weapons remix-weapons period
// ge:keywords real-weapons reskin before-after shot-looks remote-looks quake-looks
// ge:skill shooter labs
// ge:test tests/shooters_edge_remix_q1_arms_test.cpp
#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "shooters_edge/profiles/arsenal.hpp"
#include "shooters_edge/server/q2_combat.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_quake_arms.hpp"
#include "shooters_edge/server/q2_server.hpp"
#include "shooters_edge/server/q2_wire.hpp"

namespace {

namespace gl = ge::lab;
namespace sv = shooters_edge::server;
namespace br = ge::brush;
namespace sh = ge::shooter;
namespace pf = shooters_edge::profiles;

constexpr double kHeld = 3.0;   // s each weapon is held
constexpr float kFull = 10000;  // the target's health, kept up each frame

// Each of Quake's weapons: the item that gives it, how far the target stands, and the weapon of
// Quake II that stood for it when Quake II's weapons only wore Quake's files (-1: none).
struct Case {
  const char* item;  // nullptr: in the spawn's kit
  float range;
  int before;
};
constexpr Case kCases[8] = {
    {nullptr, 40, -1},
    {nullptr, 200, int(sv::SqWeapon::blaster)},
    {"weapon_supershotgun", 200, int(sv::SqWeapon::super_shotgun)},
    {"weapon_machinegun", 200, int(sv::SqWeapon::machinegun)},
    {"weapon_chaingun", 200, int(sv::SqWeapon::chaingun)},
    {"weapon_grenadelauncher", 300, int(sv::SqWeapon::grenade_launcher)},
    {"weapon_rocketlauncher", 300, int(sv::SqWeapon::rocket_launcher)},
    {"weapon_railgun", 200, int(sv::SqWeapon::hyperblaster)},
};

const gl::Rgba kPaper{246, 244, 238, 255};
const gl::Rgba kInk{30, 30, 34, 255};
const gl::Rgba kFaint{150, 150, 150, 255};
const gl::Rgba kQuake{150, 82, 40, 255};
const gl::Rgba kNow{196, 128, 60, 255};
const gl::Rgba kBefore{40, 96, 150, 255};

std::shared_ptr<const br::Map> hall() {
  br::BrushWorld w;
  w.models.resize(1);
  const float lo[3] = {-1024, -1024, -16}, hi[3] = {1024, 1024, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  br::Entity start;
  start.pairs = {{"classname", "info_player_deathmatch"}, {"origin", "0 0 24"}, {"angle", "0"}};
  w.entities.push_back(start);
  std::vector<uint8_t> bytes;
  std::string error;
  auto map = std::make_shared<br::Map>();
  if (!br::write_brush_world(w, &bytes, &error) || !br::read_bsp(bytes, map.get(), &error))
    return nullptr;
  return map;
}

struct Stage {
  sv::RulesRunner r;
  uint32_t sequence = 0;
  sv::Q2Server& s() const { return *sv::q2_of(r); }
  sv::Q2Player& p(int i) const { return s().players[i]; }
};

void put(sv::Q2Player& p, float x) {
  p.move.origin[0] = static_cast<int16_t>(x * 8);
  p.move.origin[1] = 0;
  p.move.origin[2] = static_cast<int16_t>(24 * 8);
  for (int32_t& v : p.move.velocity) v = 0;
  for (int16_t& v : p.move.delta_angles) v = 0;
  p.forced = false;
}

void tick(Stage& d, uint8_t buttons, int impulse = 0) {
  sh::Packet packets[2];
  ++d.sequence;
  for (int c = 0; c < 2; c++) {
    packets[c].client = c;
    packets[c].sequence = d.sequence;
    sh::MoveCmd cmd;
    cmd.msec = 13;
    if (c == 0) {
      cmd.buttons = buttons;
      cmd.impulse = static_cast<uint8_t>(impulse);
    }
    packets[c].cmds[0] = packets[c].cmds[1] = packets[c].cmds[2] = cmd;
  }
  d.r.frame(packets, 2);
}

struct Measured {
  int shots = 0;
  double first = -1, last = -1;
  int spent = 0;
  float damage = 0;
  std::string sound;
  std::set<std::string> looks;
  std::set<std::string> remote, stock;  // the looks that reach a remote client of ours; a stock one
  double period() const { return shots > 1 ? (last - first) / (shots - 1) : 0; }
};

// Holds weapon `w` (by its impulse, the item given first) for kHeld s at a target `range` ahead.
Measured measure(bool worn, const Case& c, int w, std::string& error) {
  Measured m;
  Stage d;
  d.r = sv::q2_runner(sv::make_q2_server({}));
  d.r.console("deathmatch 1");
  if (worn) d.r.console("set sv_wear q1");
  sv::MapSpec spec;
  spec.name = "hall";
  spec.brushes = hall();
  if (spec.brushes == nullptr || !d.r.start(spec, error)) return m;
  const float at = -300;
  d.r.connect(0, "Ana", false, false);
  tick(d, 0);
  put(d.p(0), at);
  d.r.connect(1, "Ben", false, false);
  for (int i = 0; i < 160; i++) {
    put(d.p(0), at);
    put(d.p(1), at + c.range);
    tick(d, 0);
  }
  sv::Q2Player& p = d.p(0);
  const char* item = worn ? c.item : sv::kSqWeapons[size_t(w)].item;
  if (item != nullptr) sv::sq_pick_up(sv::sq_item(item), p.body, true, 0, 0);
  for (int a = 1; a < sv::kSqAmmoKinds; a++) p.body.ammo[a] = 200;
  const int impulse = worn ? w - sv::kSqQuakeFirst + 1 : w + 1;
  for (int i = 0; i < 80; i++) {
    put(p, at);
    put(d.p(1), at + c.range);
    tick(d, 0, impulse);
  }
  if (p.hand.weapon != w) {
    error = std::string("the hand holds ") + sv::kSqWeapons[size_t(p.hand.weapon)].name + ", not " +
            sv::kSqWeapons[size_t(w)].name;
    return m;
  }
  const sv::SqAmmo ammo = sv::kSqWeapons[size_t(w)].ammo;
  const int had = p.body.ammo[int(ammo)];
  const double began = d.s().level_time;
  int64_t shot = p.q1_last_shot;
  int64_t seen = d.s().out.events_total;
  while (d.s().level_time - began < kHeld - 1e-6) {
    put(p, at);
    put(d.p(1), at + c.range);
    d.p(1).health = kFull;
    tick(d, sh::kButtonAttack);
    m.damage += kFull - std::max(0.0f, d.p(1).health);
    const double t = d.s().level_time - began;
    bool fired = worn && p.q1_last_shot != shot;
    shot = p.q1_last_shot;
    const std::vector<sv::NetEvent>& events = d.s().out.events;
    const auto fresh = static_cast<size_t>(
        std::min<int64_t>(d.s().out.events_total - seen, static_cast<int64_t>(events.size())));
    seen = d.s().out.events_total;
    for (size_t e = events.size() - fresh; e < events.size(); e++) {
      const sv::NetEvent& ev = events[e];
      const bool own = ev.text == "q2_flash" || ev.text == "sq_login" ||
                       ev.text == "te_damage_dealt" || ev.text == "te_teleport";
      if (!worn && ev.kind == sv::EventKind::point_effect && ev.text == "q2_flash" && ev.ent == 1)
        fired = true;
      if (ev.kind == sv::EventKind::sound && ev.ent == 1 && m.sound.empty() && worn)
        m.sound = ev.text;
      if ((ev.kind == sv::EventKind::point_effect || ev.kind == sv::EventKind::trail_effect) &&
          !own && !ev.text.empty()) {
        m.looks.insert(ev.text);
        // Over the Quake II wire: to a remote client of ours, and to a stock one (as ours was
        // sent before Quake's looks had their own temporary entities).
        std::vector<uint8_t> bytes;
        if (sv::q2_write_effect(bytes, ev, true)) m.remote.insert(ev.text);
        bytes.clear();
        if (sv::q2_write_effect(bytes, ev, false)) m.stock.insert(ev.text);
      }
    }
    if (!fired) continue;
    if (m.shots++ == 0) m.first = t;
    m.last = t;
  }
  if (ammo != sv::SqAmmo::none) m.spent = had - p.body.ammo[int(ammo)];
  return m;
}

std::string fixed(double v, int places) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.*f", places, v);
  return buf;
}

std::string joined(const std::set<std::string>& names) {
  std::string out;
  for (const std::string& n : names) out += (out.empty() ? "" : " ") + n;
  return out.empty() ? "-" : out;
}

void run(gl::Lab& lab) {
  std::vector<std::vector<std::string>> rows, before_rows;
  constexpr int kRow = 58, kLeft = 200, kScale = 500;  // pixels for 1 s between shots
  constexpr int kWidth = kLeft + kScale + kScale / 2 + 80;
  gl::Picture pic = gl::create_picture(kWidth, (8 + 2) * kRow + 20, kPaper);
  gl::draw_text(pic, 8, 16, "TIME BETWEEN SHOTS (s), EACH WEAPON HELD 3 s", kInk);
  gl::draw_text(pic, kLeft, 34, "Quake's own number", kQuake);
  gl::draw_text(pic, kLeft + 200, 34, "fired by Quake II's rules now", kNow);
  gl::draw_text(pic, kLeft + 450, 34, "Quake II's weapon renamed (before)", kBefore);
  int matched = 0, quake_looks = 0, q2_looks = 0, remote_looks = 0, stock_looks = 0;
  std::vector<std::vector<std::string>> remote_rows;
  for (int i = 0; i < 8; i++) {
    const Case& c = kCases[i];
    const int w = sv::kSqQuakeFirst + i;
    const pf::WeaponRow& own = pf::kQ1Weapons[i];
    std::string error;
    const Measured now = measure(true, c, w, error);
    if (!error.empty()) lab.note(std::string(sv::kSqWeapons[size_t(w)].name) + ": " + error);
    Measured was;
    if (c.before >= 0) {
      was = measure(false, c, c.before, error);
      if (!error.empty())
        lab.note(std::string(sv::kSqWeapons[size_t(c.before)].name) + ": " + error);
    }
    const bool same =
        now.shots > 1 && now.period() > own.refire - 0.051 && now.period() < own.refire + 0.051;
    matched += same;
    for (const std::string& l : now.looks) quake_looks += l.rfind("q1_", 0) == 0;
    for (const std::string& l : was.looks) q2_looks += l.rfind("q1_", 0) != 0;
    remote_looks += static_cast<int>(now.remote.size());
    stock_looks += static_cast<int>(now.stock.size());
    remote_rows.push_back({std::string(sv::kSqWeapons[size_t(w)].name), joined(now.looks),
                           joined(now.stock), joined(now.remote)});
    rows.push_back({std::string(sv::kSqWeapons[size_t(w)].name), fixed(own.refire, 2),
                    fixed(now.period(), 2), std::to_string(now.shots), std::to_string(now.spent),
                    fixed(now.damage, 0), now.sound.empty() ? "-" : now.sound, joined(now.looks)});
    if (c.before >= 0)
      before_rows.push_back({std::string(sv::kSqWeapons[size_t(w)].name),
                             std::string(sv::kSqWeapons[size_t(c.before)].name),
                             fixed(was.period(), 2), std::to_string(was.shots),
                             fixed(was.damage, 0), joined(was.looks)});
    // The bars.
    const int y = (i + 1) * kRow + 18;
    if (i % 2 == 0) gl::fill_rect(pic, 0, y, kWidth, kRow, {236, 233, 224, 255});
    gl::draw_text(pic, 8, y + 18, sv::kSqWeapons[size_t(w)].name, kInk);
    const auto bar = [&](int k, double s, gl::Rgba col) {
      const int len = static_cast<int>(s * kScale);
      gl::fill_rect(pic, kLeft, y + 6 + k * 16, std::max(len, 1), 12, col);
      gl::draw_text(pic, kLeft + len + 6, y + 9 + k * 16, s > 0 ? fixed(s, 2) : "none", col);
    };
    bar(0, own.refire, kQuake);
    bar(1, now.period(), kNow);
    if (c.before >= 0)
      bar(2, was.period(), kBefore);
    else
      gl::draw_text(pic, kLeft, y + 41, "(Quake II has no axe: there was none)", kFaint);
  }
  lab.shot({"q1-arms-periods",
            "QUAKE'S WEAPONS ON QUAKE II'S RULES",
            "Each of Quake's weapons held for three seconds on Quake II's rules wearing Quake: "
            "the time between its shots as Quake's own rules have it, as Quake II's rules fire it "
            "now, and as the Quake II weapon that wore its files fired before.",
            "the first two bars match for every weapon; the third does not where Quake II's own "
            "differs (the super shotgun's 0.7 s against 1.2 s, the grenade launcher's 0.6 s "
            "against 1.2 s, the rocket launcher's 0.8 s against 0.9 s)",
            {}},
           pic);
  // Quake's looks over the Quake II wire: what a remote client got before (the names the protocol
  // knows, as a stock client still gets) and what a client of ours gets now.
  {
    constexpr int kLine = 22, kW = 1100;
    const int cols[4] = {8, 200, 560, 760};
    gl::Picture wire =
        gl::create_picture(kW, static_cast<int>(remote_rows.size() + 2) * kLine + 12, kPaper);
    gl::draw_text(wire, cols[0], 14, "WEAPON", kInk);
    gl::draw_text(wire, cols[1], 14, "LOOKS ITS HITS SEND", kInk);
    gl::draw_text(wire, cols[2], 14, "REMOTE, BEFORE", kBefore);
    gl::draw_text(wire, cols[3], 14, "REMOTE CLIENT OF OURS, NOW", kNow);
    for (size_t i = 0; i < remote_rows.size(); i++) {
      const int y = static_cast<int>(i + 1) * kLine + 8;
      if (i % 2 == 0) gl::fill_rect(wire, 0, y - 4, kW, kLine, {236, 233, 224, 255});
      const std::vector<std::string>& r = remote_rows[i];
      gl::draw_text(wire, cols[0], y + 10, r[0], kInk);
      gl::draw_text(wire, cols[1], y + 10, r[1], kFaint);
      gl::draw_text(wire, cols[2], y + 10, r[2], kBefore);
      gl::draw_text(wire, cols[3], y + 10, r[3], kNow);
    }
    lab.shot(
        {"q1-arms-remote-looks",
         "QUAKE'S LOOKS OVER THE QUAKE II WIRE",
         "Each of Quake's weapons fired by Quake II's rules: the looks its hits send, which of "
         "them reached a remote client before (none: the Quake II wire has no name for them, "
         "as a stock client still gets), and which reach a remote client of ours now.",
         "every look in the second column is in the last, and the before column is empty",
         {}},
        wire);
  }
  lab.metric("quake-looks-remote", remote_looks, "names");
  lab.metric("quake-looks-stock", stock_looks, "names");
  lab.table({"Quake's looks over the Quake II wire",
             {"weapon", "looks sent", "to a stock client (and ours before)", "to ours now"},
             remote_rows});
  lab.metric("weapons-at-quakes-period", matched, "weapons");
  lab.metric("quake-looks", quake_looks, "names");
  lab.metric("q2-looks-before", q2_looks, "names");
  lab.table({"Quake's weapons fired by Quake II's rules (sv_wear q1), 3 s held",
             {"weapon", "Quake's period (s)", "measured (s)", "shots", "ammo spent",
              "damage to the target", "first sound", "hit looks sent"},
             rows});
  lab.table(
      {"before: Quake II's weapon that wore Quake's files, the same 3 s",
       {"Quake's weapon", "Quake II's weapon", "period (s)", "shots", "damage", "hit looks sent"},
       before_rows});
}

const gl::LabSpec kSpec{
    "se-remix-q1-arms",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/server/q2_quake_arms: Quake's weapons fired whole by Quake II's rules",
    "shooter",
    "Does each of Quake's weapons fire on Quake II's rules at Quake's own period, spend Quake's "
    "ammunition, and send Quake's own looks, where the renamed Quake II weapon did not?",
    {},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
