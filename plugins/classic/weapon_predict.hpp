// QUAKE'S HELD WEAPON PREDICTED ON THE CLIENT: the weapon's state as the server sends it in the
// QuakeWorld weapon-prediction block (net/qw_delta.hpp), run forward over the commands the client
// has sent since, so a shot's frame, sound and ammunition show when the button goes down rather
// than a round trip later. The client blends the predicted frames (client/held.cpp) as it blends
// the server's.
//
// THE CLOCK. Each player has a weapon clock in seconds; refire (`attack_finished`) and the next
// think are on it. A command advances it by its msec (a command over 50 ms split in halves until
// each part is 50 ms or less). While the player is dead or frozen it stands still, the refire is
// pushed 0.05 s past it and the pending impulse is cleared.
//
// ONE PART OF A COMMAND, in order: the clock advances; a nonzero impulse replaces the pending one;
// a weapon without ammunition (not the axe) is changed for the best one once the refire is past; a
// think that is due runs as if at its own time (at most one per part); then, once the clock reaches
// the refire, the pending impulse is taken (1..8 select, 10 and 12 cycle; any other is dropped)
// and, with attack held, the weapon fires.
//
// THE THINK STEP (`step`, sent with the state): 0 the weapon rests at the next think. The axe: the
// swing's count 1..3, the frame advanced by one each think, the fourth think wrapping it to 0. The
// shotguns and the launchers: the next frame to show, 2..6, then 0. The nailguns and the lightning
// gun: which barrel fires next, 1 (right) or 2 (left). A server works it out from the frame alone
// (`weapon_step_of`), so it needs no field of its own.
//
// THE RULES are Quake's: the super shotgun and the super nailgun take two of their ammunition a
// shot (one when only one is left, for the super nailgun); the nailguns fire a nail every 0.1 s
// think while held, frames 1..8; the lightning gun a cell every think, frames 1..4; the best weapon
// is the lightning gun out of water, then the super nailgun, the super shotgun, the nailgun, the
// shotgun, the axe, or the player's `w_rank` order. A nail or beam think stops on release or on an
// impulse pending. The axe picks its first or second swing by a fixed hash of the clock, which the
// next server frame corrects when the server's random choice differs.
//
// THE MISSION PACKS' RULES (`WeaponRules`, told by the view models the level lists:
// `weapon_rules_of`). Scourge of Armagon adds the laser cannon (8388608: a think every 0.1 s while
// held, a cell each, twin bolts and a single bolt in turn, weapon frames 1 and 4), Mjolnir (128:
// a swing of frames 1..4 over 0.3 s, held on 4 to 0.6 s, refire 0.8 s, then 0.7 s from the blow;
// it needs no cells and is never switched away from) and the proximity gun (65536: the grenade
// launcher's frames and refire, on rockets); key 6 toggles the launcher and the proximity gun,
// impulses 225 and 226 take the laser cannon and Mjolnir, and the cycle and the best weapon take
// them in. Dissolution of Eternity moves the axe to 2048 and adds the lava nailguns (4096, 8192),
// the multi launchers (16384, 32768) and the plasma gun (65536: one bolt a press, refire 1 s, no
// firing frame shown), each firing as its base gun does from the ammunition the bar shows; its
// four ammunition counts change meaning when the weapon in hand crosses 4096, so a change of
// weapon there (a key, a cycle, a switch when empty) is not predicted: the weapon is unknown
// (`kWeaponUnknown`) until the server's next state says.
//
// ge:summary Quake's held weapon predicted on the client: the clock, thinks, refire, ammo, sounds
// ge:keywords weapon-prediction predict weapon-clock attack-finished think-step weaponframe
// ge:keywords nailgun super-nailgun lightning axe shotgun refire best-weapon w-rank impulse
// ge:keywords predicted-sound mvd-weapon-prediction mission-pack scourge-of-armagon laser-cannon
// ge:keywords mjolnir proximity-gun dissolution-of-eternity lava-nailgun multi-rocket plasma-gun
// ge:skill classic net
// ge:test tests/classic_weapon_predict_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ge::classic {

// Quake's weapon item bits (the program's `weapon` holds one of them).
constexpr int kItShotgun = 1, kItSuperShotgun = 2, kItNailgun = 4, kItSuperNailgun = 8;
constexpr int kItGrenadeLauncher = 16, kItRocketLauncher = 32, kItLightning = 64, kItAxe = 4096;
// Scourge of Armagon's.
constexpr int kItMjolnir = 128, kItProximityGun = 65536, kItLaserCannon = 8388608;
// Dissolution of Eternity's (its axe moved to 2048).
constexpr int kItAxeEternity = 2048, kItLavaNailgun = 4096, kItLavaSuperNailgun = 8192;
constexpr int kItMultiGrenade = 16384, kItMultiRocket = 32768, kItPlasmaGun = 65536;
// A weapon the prediction cannot know until the server says (a Dissolution change).
constexpr int kWeaponUnknown = -1;

// Whose rules the weapons follow.
enum class WeaponRules : uint8_t { quake, scourge, dissolution };
// The rules a level's model list says (a pack's own view models listed: its rules).
WeaponRules weapon_rules_of(const std::vector<std::string>& models);

// The weapon's state, as the block carries it plus the frame drawn.
struct WeaponState {
  bool known = false;  // the server sent the state (its "state follows" byte was nonzero)
  int impulse = 0;     // pending, 0..255
  int weapon = 0;      // the item bit held
  float clock = 0;     // s
  float attack_finished = 0, next_think = 0;  // s on the clock; next_think 0: none
  int step = 0;                               // the think step (above)
  int ping_ms = 0;                            // 0..255
  int flags = 0;     // 1 midair, 2 coilgun (not predicted), 255 the server turns prediction off
  int ammo[4] = {};  // shells, nails, rockets, cells, 0..255
  int frame = 0;     // weaponframe
};

constexpr int kWeaponFlagsOff = 255;

// What a part of a command needs besides the weapon.
struct WeaponCmd {
  int msec = 0;
  bool attack = false;  // button bit 1
  int impulse = 0;
  bool frozen = false;  // dead, or no move (pm types dead, none, lock)
  int waterlevel = 0;   // the predicted move's, for the lightning gun's place in the best weapon
};

struct WeaponWorld {
  int items = 0;            // the item bits (the server's item stat)
  std::string_view w_rank;  // the player's `w_rank` (digits 1..8, best first); "" Quake's order
  WeaponRules rules = WeaponRules::quake;
};

// A sound the prediction plays, with the weapon's bit in `cl_predict_weaponsound`'s mask.
enum class WeaponSound : uint8_t {
  none,
  axe,
  shotgun,
  super_shotgun,
  nailgun,
  super_nailgun,
  grenade,
  rocket,
  lightning_start,
  lightning_hit,
  laser,      // Scourge's laser cannon, a bolt
  proximity,  // Scourge's proximity gun
  plasma,     // Dissolution's plasma gun, at half volume
};
struct WeaponEvent {
  WeaponSound sound = WeaponSound::none;
  float clock = 0;  // when on the weapon clock
  int side = 0;     // the nailgun's barrel its nail left from: 1 right, -1 left; 0 the middle
  int copies = 1;   // the server plays it this many times (the laser cannon's twin bolts: 2)
};
const char* weapon_sound_sample(WeaponSound s);  // "weapons/ax1.wav"; nullptr for none
int weapon_sound_channel(WeaponSound s);         // 1, 0 for the lightning gun's start
int weapon_sound_bit(WeaponSound s);             // 2 axe .. 256 lightning (a pack gun its key's)
float weapon_sound_volume(WeaponSound s);        // 1; the plasma gun's 0.5
// The mask bit of the weapon whose sound a server sample is on channel 1 or 0; 0 when none.
int weapon_sound_bit_of_sample(std::string_view sample);

// The ammunition the held weapon fires (0 for the axe and Mjolnir).
int weapon_current_ammo(const WeaponState& s, WeaponRules rules = WeaponRules::quake);
// The view model's name ("progs/v_nail.mdl"); nullptr for none.
const char* weapon_view_model(int weapon, WeaponRules rules = WeaponRules::quake);
// The best weapon held with ammunition to fire.
int weapon_best(const WeaponState& s, const WeaponWorld& w, int waterlevel);
// The think step a server sends for a weapon at `frame` (above).
int weapon_step_of(int weapon, int frame, WeaponRules rules = WeaponRules::quake);

// The weapon as the weapon-prediction block's 16-bit field carries it: a pack's weapon past 32767
// (Scourge of Armagon's laser cannon, 2^23; a pack gun at 2^16) as its bit's number negated, which
// no weapon value ever is, so a client that knows no such weapon predicts nothing for it.
int weapon_on_wire(int weapon);
int weapon_off_wire(int value);

// One command run on the weapon; its events written to `out` (at most `room`), their count
// returned. At most 4 events a command part.
int weapon_command(WeaponState& s, const WeaponCmd& c, const WeaponWorld& w, WeaponEvent* out,
                   int room);

}  // namespace ge::classic
