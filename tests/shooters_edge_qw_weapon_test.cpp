// THE JOINED WINDOW'S WEAPON PREDICTED: from the acknowledged state over the commands in flight,
// the shot heard and counted once at the shown command (never again on the next pass, nor when a
// correction moves it by a command), the server's copy of the sound dropped and the axe's hit
// kept, the bar's ammunition and the view model drawn from the prediction and put back to the
// server's before its next frame, nothing predicted for a spectator or under `cl_nopred_weapon 1`;
// and a whole session on the simulated network, where the predicted shot comes a round trip
// before the server's.
//
// ge:summary Tests: the joined window's weapon predicted: shots once, sounds, the bar, a session
// ge:keywords test weapon-prediction shown-command predicted-sound own_shots cl_nopred_weapon
// ge:skill net testing
#include <string>
#include <vector>

#include "check.hpp"
#include "classic/weapon_predict.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "progs/file.hpp"
#include "shooters_edge/net/qw_remote.hpp"
#include "shooters_edge/net/qw_server.hpp"
#include "shooters_edge/net/qw_weapon.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/mod_match.hpp"

namespace cn = shooters_edge::net;
namespace dc = shooters_edge::client;
namespace sh = ge::shooter;
namespace cl = ge::classic;
namespace gn = ge::net;
namespace sv = shooters_edge::server;

namespace {

// The window at slot 0 with a super nailgun and 50 nails, the server's state acknowledged at
// command 100 and commands 101.. sent with attack held.
struct Joined {
  cn::QwClientEnd c;
  dc::DemoState d;
  Joined() {
    c.playernum = 0;
    c.player_seen[0] = true;
    c.ext = cn::first_tier();
    cl::WeaponState& w = c.players[0].weapon;
    w.known = true;
    w.weapon = cl::kItSuperNailgun;
    w.clock = 5;
    w.ammo[1] = 50;
    c.ch.incoming_acknowledged = 100;
    c.ch.outgoing_sequence = 101;
    d.player = 0;
    d.format = dc::DemoFormat::client;
    d.players.resize(32);
    d.sounds = {"", "weapons/spike2.wav", "player/axhit2.wav", "weapons/guncock.wav"};
    d.models = {"", "progs/v_shot.mdl", "progs/v_nail2.mdl"};
    d.stats[dc::kStatItems] = cl::kItSuperNailgun | cl::kItShotgun | cl::kItAxe;
    d.stats[dc::kStatWeapon] = 1;  // the server still shows the shotgun
    d.stats[dc::kStatNails] = 50;
  }
  void send(bool attack) {
    const uint32_t q = c.ch.outgoing_sequence++;
    sh::MoveCmd m;
    m.msec = 13;
    m.buttons = attack ? sh::kButtonAttack : 0;
    c.sent[q & 63] = m;
    c.sent_seq[q & 63] = q;
  }
};

}  // namespace

TEST(a_shot_is_counted_once_at_the_shown_command) {
  Joined j;
  cn::RemoteWeapon w;
  j.send(true);
  cn::weapon_predict(w, j.c, j.d, 0, {}, "");
  CHECK(w.on);
  CHECK(j.d.own_shots == 1);  // one command in flight: the newest is shown
  CHECK(j.d.started.size() == 1 && j.d.started[0].sound == 1 && j.d.started[0].channel == 1);
  CHECK(j.d.stats[dc::kStatWeapon] == 2);  // the super nailgun's view model
  CHECK(j.d.stats[dc::kStatNails] == 48);
  CHECK(j.d.stats[dc::kStatActiveWeapon] == cl::kItSuperNailgun);
  // The next pass: put back, then the same shot not played again.
  cn::weapon_restore(w, j.d);
  CHECK(j.d.stats[dc::kStatWeapon] == 1 && j.d.stats[dc::kStatNails] == 50);
  j.d.started.clear();
  for (int k = 0; k < 4; k++) j.send(true);
  cn::weapon_predict(w, j.c, j.d, 0, {}, "");
  CHECK(j.d.own_shots == 1);  // 52 ms on: the next think at 0.1 s not yet shown
  CHECK(j.d.started.empty());
  // The server's frame after the shot, from rules that refire by the clock alone (no frames, the
  // player's own think pending): the next shot predicted once the refire is past, at 5.113.
  cn::weapon_restore(w, j.d);
  cl::WeaponState& s = j.c.players[0].weapon;
  s.clock = 5.026f;
  s.attack_finished = 5.113f;
  s.next_think = 5.1f;
  s.ammo[1] = 48;
  j.c.players[0].weaponframe = 0;
  j.c.ch.incoming_acknowledged = 102;
  for (int k = 0; k < 6; k++) j.send(true);
  cn::weapon_predict(w, j.c, j.d, 0, {}, "");
  CHECK(j.d.own_shots == 2);
  CHECK(j.d.players[0].weapon_frame == 0);  // frames left to the hand
  // The server's next frame puts that shot a command later: the same shot, not heard twice.
  cn::weapon_restore(w, j.d);
  s.clock = 5.052f;
  s.attack_finished = 5.13f;
  j.c.ch.incoming_acknowledged = 104;
  j.send(true);
  j.d.started.clear();
  cn::weapon_predict(w, j.c, j.d, 0, {}, "");
  CHECK(j.d.own_shots == 2);
  CHECK(j.d.started.empty());
}

TEST(the_server_s_copies_are_dropped_and_the_axe_s_hit_kept) {
  Joined j;
  cn::RemoteWeapon w;
  j.send(false);
  j.send(false);
  dc::DemoSound nail;
  nail.entity = 1;
  nail.channel = 1;
  nail.sound = 1;
  dc::DemoSound hit = nail;
  hit.sound = 2;
  dc::DemoSound other = nail;
  other.entity = 2;  // another player's: heard
  j.d.started = {nail, hit, other};
  cn::weapon_predict(w, j.c, j.d, 0, {}, "");
  CHECK(j.d.started.size() == 2);
  CHECK(j.d.started[0].sound == 2 && j.d.started[1].entity == 2);
  // Above 1, the super nailgun's bit (32) leaves its sound to the server; the axe's hit goes.
  cn::weapon_restore(w, j.d);
  w.sound_mask = 32;
  j.d.started = {nail, hit};
  cn::weapon_predict(w, j.c, j.d, 0, {}, "");
  CHECK(j.d.started.size() == 1 && j.d.started[0].sound == 1);
}

TEST(nothing_is_predicted_for_a_spectator_or_with_cl_nopred_weapon) {
  for (int k = 0; k < 3; k++) {
    Joined j;
    cn::RemoteWeapon w;
    if (k == 0) j.c.cfg.spectator = true;
    if (k == 1) w.off = true;
    if (k == 2) j.c.players[0].weapon.flags = cl::kWeaponFlagsOff;
    j.send(true);
    cn::weapon_predict(w, j.c, j.d, 0, {}, "");
    CHECK(!w.on);
    CHECK(j.d.own_shots == 0 && j.d.started.empty() && j.d.stats[dc::kStatNails] == 50);
  }
}

TEST(a_session_hears_the_predicted_shot_a_round_trip_before_the_server_s) {
  // The arena's rules served on the simulated network at 40 ms each way; the window joins holding
  // a super nailgun and presses attack once.
  const char* const entities = R"({
"classname" "worldspawn"
}
{
"classname" "info_player_deathmatch"
"origin" "-416 0 24"
}
{
"classname" "weapon_supernailgun"
"origin" "-416 0 0"
}
)";
  int heard_at[2] = {-1, -1};
  for (int on = 0; on < 2; on++) {
    ge::progs::Progs progs;
    std::string error;
    CHECK(sv::baseline_mod(progs, error));
    sv::QcServerOptions so;
    so.loop.max_edicts = 1024;
    std::unique_ptr<sv::QcServer> qc = sv::make_qc_server(progs, so, error);
    CHECK(qc != nullptr);
    if (!qc) return;
    sv::MapSpec map;
    map.name = "arena";
    map.models = sv::arena_models();
    map.entities = entities;
    CHECK(sv::load_map(*qc, map, error));
    gn::LinkConditions link;
    gn::conditions_preset("lan", link);
    link.latency_ms = 40;
    link.jitter_ms = 0;
    gn::SimNetwork net = gn::make_sim_network(1, link);
    gn::Transport st, ct;
    gn::open_sim_transport(st, net, gn::sim_address(0, cn::kPortServer));
    gn::open_sim_transport(ct, net, gn::sim_address(1, cn::kPortClient));
    sv::RulesRunner rules = sv::qc_runner_over(*qc);
    cn::QwServer server;
    CHECK(cn::qw_start(server, rules, st, cn::ServerConfig{}, error));
    cn::Remote remote;
    cn::ClientConfig wc;
    wc.name = "window";
    cn::remote_start(remote, ct, gn::sim_address(0, cn::kPortServer), wc);
    std::vector<sh::BrushModel> models = sv::arena_models();
    std::vector<std::string> said;
    int64_t now = 0, wall = 0;
    constexpr int64_t kWindowUs = 1000000 / 100;
    const auto frame = [&] {
      wall += kWindowUs;
      while (now + cn::kTickUs <= wall) {
        now += cn::kTickUs;
        gn::sim_advance(net, now);
        cn::qw_frame(server, now);
      }
      cn::remote_step(remote, 0.01, said);
    };
    for (int i = 0; i < 600 && remote.end.stage != cn::ClientStage::active; i++) frame();
    for (int i = 0; i < 100; i++) frame();
    CHECK(remote.end.stage == cn::ClientStage::active);
    CHECK(remote.end.ext.has(cn::ExtFamily::mvd, cn::kMvdWeaponPred));
    cn::remote_world(remote, &models);
    remote.weapon.off = on == 0;
    const dc::DemoState& d = remote.demo.state;
    const int shots = d.own_shots;
    for (int i = 0; i < 40 && heard_at[on] < 0; i++) {
      remote.hands.buttons = sh::kButtonAttack;
      frame();
      bool heard = d.own_shots > shots;
      for (const dc::DemoSound& snd : d.started)
        if (snd.entity == d.player + 1 && snd.sound > 0 &&
            d.sounds[static_cast<size_t>(snd.sound)] == "weapons/spike2.wav")
          heard = true;
      if (heard) heard_at[on] = i;
    }
    CHECK(heard_at[on] >= 0);
  }
  // 10 ms window frames: off a round trip (80 ms and a server frame) after the press, on within
  // three ticks.
  CHECK(heard_at[0] >= 8);
  CHECK(heard_at[1] >= 0 && heard_at[1] <= 4);
}
