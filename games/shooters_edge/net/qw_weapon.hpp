// THE JOINED WINDOW'S WEAPON PREDICTED (QuakeWorld's weapon prediction, mvd bit 0x80): the
// player's weapon state as the server last acknowledged it (net/qw_delta.hpp), run forward over
// every command sent since by Quake's rules (classic/weapon_predict.hpp), so a shot is drawn and
// heard when the button goes down, not a round trip later.
//
// THE SHOWN COMMAND: the one `cl_predict_buffer` + 1 before the next to send (1: the second
// newest), never past the newest, never before the second after the acknowledged one. The weapon
// drawn, its frame, the ammunition on the bar and the active weapon are the prediction's at that
// command; each pass rebuilds them from the server's state, so the server corrects a mistake at
// its next frame.
//
// THE FRAME is drawn from the prediction only once the server has sent a weapon frame other than 0
// (rules that animate it); rules that never do leave the hand to play its own shot.
//
// SHOTS AND SOUNDS: a shot is played once, when the shown command first reaches its command; a
// shot a correction moves by a command is not played twice (one of the same sound within 0.05 s
// of weapon clock is the same shot), and the lightning gun's hit at most once per 0.6 s. Each
// counts in `DemoState::own_shots`, which the window's hand fires on (client/held.cpp). The
// server's own copies on the player's entity (channel 1's weapon sounds, the axe's hit kept under
// `cl_predict_weaponsound` 1, and channel 0's lightning start) are matched to the sounds the
// prediction played, by kind and command (`WeaponPlayed`, net/weapon_shown.hpp), and dropped; a
// copy that matches none is a shot the prediction missed and plays late. With the setting above 1
// its bits name the weapons left to the server (2 axe, 4 shotgun, 8 super
// shotgun, 16 nailgun, 32 super nailgun, 64 grenades, 128 rockets, 256 lightning).
//
// OFF: `cl_nopred_weapon 1`, `cl_nopred 1`, a spectator, a server that did not agree the bit or
// that sends flags 255, the intermission, a pause. Off, the drawn state is the server's.
//
// THE DRAWN STATE PUT BACK (`weapon_restore`): the stats and frame the prediction drew over are
// put back to the server's before the next frames are read, since a server sends a stat only when
// it changes.
//
// ge:summary The joined window's weapon predicted: the shown command, shots, sounds, ammo drawn
// ge:keywords weapon-prediction cl_nopred_weapon cl_predict_weaponsound cl_predict_buffer
// ge:keywords cl_pext_weaponprediction predicted-sound shown-command own_shots
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_weapon_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "classic/weapon_predict.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/net/qw_client.hpp"
#include "shooters_edge/net/weapon_shown.hpp"

namespace shooters_edge::net {

struct RemoteWeapon {
  // The settings (bound by add_remote_commands).
  bool off = false;    // cl_nopred_weapon
  int sound_mask = 1;  // cl_predict_weaponsound
  int buffer = 1;      // cl_predict_buffer
  // The last pass.
  bool on = false;                 // it predicted
  ge::classic::WeaponState shown;  // the state at the shown command
  uint32_t played_to = 0;          // the newest command whose shots have played
  float last_shot[10] = {};        // the weapon clock of each sound's last play
  bool frames_live = false;        // the server has sent a weapon frame other than 0
  WeaponPlayed played;             // the sounds played, by command, for the server's copies
  uint32_t copies_from = 0;        // the acknowledged command at the last pass
  uint32_t filled_late = 0;        // the server's copies that matched none and played (labs)
  // The shots this pass played first, with the command each was fired on (own_missiles.hpp
  // throws the nails, rockets and grenades among them).
  struct Fired {
    ge::classic::WeaponEvent shot;
    uint32_t command = 0;
    bool midair = false;  // the state's midair flag: the rocket flies at twice the speed
  };
  std::vector<Fired> fired;
  // What the drawn state held before the prediction drew over it (`applied`).
  bool applied = false;
  int32_t stats[client::kStatCount] = {};
  int frame = 0;
};

// The drawn stats and frame put back to the server's (before the next frames are read).
void weapon_restore(RemoteWeapon& w, client::DemoState& d);

// One pass: the weapon run forward from `c`'s acknowledged state to the shown command and drawn
// into `d` (the stats, the player's frame), its new shots added to `d.started` and counted in
// `d.own_shots`, the server's copies of them dropped (any other plays late). `waterlevel` the
// predicted move's, `origin` the player's drawn place; `w_rank` the player's best-weapon order (""
// Quake's).
void weapon_predict(RemoteWeapon& w, const QwClientEnd& c, client::DemoState& d, int waterlevel,
                    const ge::shooter::Vec3f& origin, const std::string& w_rank);

}  // namespace shooters_edge::net
