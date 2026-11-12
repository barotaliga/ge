// THE BOT'S SENSES: what a bot knows of the others, learned only from what the server sends a
// player's client, and checked against the world the way a player's eyes are.
//
// WHAT A CLIENT IS SENT (`client_view`), each server frame: every entity in use with a model, and
// every other player in the game whatever its model (its number, origin, angles and model index),
// and the frame's sounds (the entity that started each, where it stands, the sample, volume and
// attenuation). A sound reaches a client only within `kHearAt / attenuation` units of it (where the
// classic mix fades it to silence); an attenuation of 0 (a level-wide sound) reaches everyone. A
// map's PVS would cut both lists to what the client's leaf can see; the box-brush maps have none,
// so everything is sent. Nothing a client is not sent (health, ammo, a think or a touch) is ever
// read.
//
// WHAT THE BOT TAKES FROM IT (`sense`), for each other player sent:
// - SEEN when it lies inside the view (a round cone `fov` degrees across about the view's yaw
//   and pitch, or a screen `fov` wide and `fov_y` high, as a player's monitor shows it), within
//   `range`, and the line from the eye (`view_height` over the origin) to its
//   head or its middle is clear of the world and every brush entity (a shut door hides; another
//   player does not) and crosses no water surface: an eye in the air does not see a point under
//   water, nor an eye under water one in the air (`water_opaque`; slime and lava alike). Seen, its
//   place is remembered (`ai::Memory`), and its velocity is a player's as the server sent it
//   (sightings across a network come bunched as the packets do), else taken off the last two
//   sightings when they are a frame apart.
// - DEAD (the protocol's flag on a player at no health): no contact; one kept is dropped, so a bot
//   never aims at a body.
// - HEARD when it started a sound this frame that reached the client: its place is remembered as
//   heard (a footstep, a shot, a jump), not as seen.
// - A FOE that is not a player (a monster in a co-op game): an entity the game marks in
//   `Senses::foes` is taken as a contact like a player, seen by the same cone and line and heard
//   when it starts a sound; one the game unmarks (dead, or no threat any more) is dropped. What
//   marks one is the game's choice, from what its client is sent.
// A contact is forgotten `forget_s` after it was last seen or heard. Every change (seen, lost,
// heard, forgotten) is a decision on the `ai::Why` handed in, so `ge why` reads why a bot turned.
//
// Pure functions of the server and the senses: no random draw, and the only allocation is the
// view's and the contacts' vectors growing to the largest frame seen.
//
// ge:summary The bot's senses: what a client is sent, sight by the hull trace in a cone, sounds
// ge:keywords senses sight hearing client-view sent entities sounds attenuation fov cone
// ge:keywords line-of-sight trace memory contact forget seen heard velocity pvs friends team
// ge:keywords foes monsters co-op
// ge:skill shooter bots
// ge:test tests/shooter_senses_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "ai/memory.hpp"
#include "ai/why.hpp"
#include "shooter/server.hpp"

namespace ge::shooter {

constexpr float kHearAt = 1000;  // a sound at attenuation 1 falls silent this far away, units

struct ViewEntity {
  int ent = 0;
  Vec3f origin, angles;
  int modelindex = 0;
  bool dead = false;  // a player at no health (the protocol's dead flag)
};

struct ViewSound {
  int ent = 0;
  Vec3f origin;  // its entity's origin (a brush entity's middle)
  int channel = 0;
  const char* sample = "";
  float volume = 1, attenuation = 1;
};

// One frame of what the server sends one client.
struct ClientView {
  std::vector<ViewEntity> entities;
  std::vector<ViewSound> sounds;
};

// What the server sends the client whose player is entity `viewer`, this frame.
void client_view(const Server& sv, int viewer, ClientView& out);

struct SenseSettings {
  float view_height = 22;  // the eye over the origin, units
  float fov = 100;         // the view across, degrees (360: all round)
  // The view's height, degrees: set, the view is a screen's rectangle `fov` wide and `fov_y`
  // high about the view's axes, as a player's monitor shows it; 0, a round cone `fov` across.
  float fov_y = 0;
  float range = 4096;  // units
  float forget_s = 6;  // a contact neither seen nor heard this long is forgotten
  // Every player's sound that reaches the client is heard, exactly where it is. A bot with ears of
  // its own (hearing.hpp) turns this off and hands in what it takes in with `sense_heard`.
  bool hear_players = true;
  // A change of course is read this late, s (`Contact::course`): a person sees a strafer turn
  // round a reaction after he does. 0: at once.
  float course_s = 0;
  // A liquid's surface hides what is on its other side, as a player's screen draws it opaque: the
  // eye and the point seen must be in the same contents (air, or the same liquid).
  bool water_opaque = true;
};

// A contact's drift (`Contact::drift`, `leadmean`) follows its course this slowly, s.
inline constexpr float kDrift_s = 0.5f;
// A contact's turn rate (`Contact::turn`, `leadarc`) averaged this long, s, read only past this
// speed across, units/s.
inline constexpr float kTurn_s = 0.15f, kTurnSpeed = 200;

struct Contact {
  int ent = -1;     // a player's entity; -1 a free contact
  ai::Memory seen;  // where and when it was last seen
  Vec3f velocity;   // a player's as sent; else off the last two sightings a frame apart
  Vec3f course;     // the velocity as the eye takes it in: `velocity` caught up over `course_s`
  Vec3f drift;      // and his drift: `course` averaged over `kDrift_s` (a dodger's swing's middle)
  float turn = 0;   // his course's turn, degrees a second (+ counter-clockwise), over `kTurn_s`
  bool visible = false;      // seen this frame
  bool was_visible = false;  // seen the frame before
  ai::Memory heard;          // where and when it was last heard
  double since = 0;          // when it was last seen or heard
};

struct SenseStats {
  int64_t frames = 0, seen = 0, heard = 0, acquired = 0, lost = 0, forgotten = 0;
};

struct Senses {
  SenseSettings set;
  // Players this bot does not take as contacts (its team): bit k is the player in slot k (entity
  // k + 1). A teammate is neither seen nor heard as someone to fight.
  uint32_t friends = 0;
  // Entities other than players this bot takes as contacts: `foes[e]` non-zero for entity `e`
  // (empty: players only). The game sets and clears them between frames.
  std::vector<uint8_t> foes;
  std::vector<Contact> contacts;
  SenseStats stats;
};

// Whether the eye at `eye` with the view `angles` (pitch down positive, yaw) sees the player at
// `origin`: in the view, in range, and a clear line to its head or its middle that crosses no
// liquid's surface.
bool sees_player(const Server& sv, const SenseSettings& set, int viewer, Vec3f eye, Vec3f angles,
                 Vec3f origin);

// One frame's senses for the bot whose player is entity `viewer`, its view at `angles`, at time
// `t`, from what its client was sent (`view`). The world is read only for sight's trace.
void sense(Senses& s, const Server& sv, const ClientView& view, int viewer, Vec3f angles, double t,
           ai::Why* why = nullptr);

// A sound from the player entity `ent` taken in at time `t`, believed to come from `at`: remembered
// as heard (a teammate's, or the bot's own, is not a contact).
void sense_heard(Senses& s, int ent, int viewer, Vec3f at, double t, ai::Why* why = nullptr);

// The contact remembered most recently (seen over heard, the nearer of two seen this frame), or
// null when there is none.
const Contact* freshest(const Senses& s, Vec3f from);

}  // namespace ge::shooter
