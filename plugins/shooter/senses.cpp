// ge:summary The bot's senses: the client's view of a frame, sight in a cone by trace, hearing
// ge:keywords senses client_view sense sees_player freshest contact
// ge:skill shooter bots
// ge:test tests/shooter_senses_test.cpp
#include "shooter/senses.hpp"

#include <limits>

#include "shooter/trace.hpp"
#include "shooter/vec.hpp"

namespace ge::shooter {

namespace {

constexpr double kFrameGap_s = 0.05;  // two sightings this near in time give a velocity

Vec3f middle_of(const Edict& e) {
  return {(e.absmin.x + e.absmax.x) * 0.5f, (e.absmin.y + e.absmax.y) * 0.5f,
          (e.absmin.z + e.absmax.z) * 0.5f};
}

bool is_player(const Server& sv, int ent) { return ent >= 1 && ent <= sv.set.maxclients; }

Vec3f at_of(const ai::Memory& m) {
  return {static_cast<float>(m.x), static_cast<float>(m.y), static_cast<float>(m.z)};
}

float dist2(Vec3f a, Vec3f b) {
  const Vec3f d = a - b;
  return d.x * d.x + d.y * d.y + d.z * d.z;
}

Contact& contact_for(Senses& s, int ent) {
  for (Contact& c : s.contacts)
    if (c.ent == ent) return c;
  for (Contact& c : s.contacts)
    if (c.ent < 0) {
      c = Contact{};
      c.ent = ent;
      return c;
    }
  s.contacts.emplace_back();
  s.contacts.back().ent = ent;
  return s.contacts.back();
}

bool clear_line(const Server& sv, int viewer, Vec3f eye, Vec3f to) {
  const HullTrace tr = trace_move(sv, eye, {}, {}, to, kTraceNoMonsters, viewer);
  return !tr.startsolid && tr.fraction >= 1;
}

}  // namespace

void client_view(const Server& sv, int viewer, ClientView& out) {
  out.entities.clear();
  out.sounds.clear();
  const Edict& me = sv.edicts[static_cast<size_t>(viewer)];
  for (int e = 1; e < sv.num_edicts; e++) {
    const Edict& d = sv.edicts[static_cast<size_t>(e)];
    // A player in the game is sent whatever its model (a slot's player always is).
    const bool player = is_player(sv, e) && sv.clients[static_cast<size_t>(e - 1)].active &&
                        !sv.clients[static_cast<size_t>(e - 1)].spectator;
    if (d.free || e == viewer || (d.modelindex == 0 && !player)) continue;
    out.entities.push_back({e, d.origin, d.angles, d.modelindex, player && d.health <= 0});
  }
  for (int i = 0; i < sv.sound_count; i++) {
    const SoundEvent& se = sv.sounds[i];
    if (se.ent < 0 || se.ent >= sv.num_edicts) continue;
    const Edict& d = sv.edicts[static_cast<size_t>(se.ent)];
    const Vec3f at = d.solid == kSolidBsp ? middle_of(d) : d.origin;
    if (se.attenuation > 0) {
      const float reach = kHearAt / se.attenuation;
      if (dist2(at, me.origin) > reach * reach) continue;
    }
    out.sounds.push_back({se.ent, at, se.channel, se.sample, se.volume, se.attenuation});
  }
}

bool sees_player(const Server& sv, const SenseSettings& set, int viewer, Vec3f eye, Vec3f angles,
                 Vec3f origin) {
  const Vec3f head{origin.x, origin.y, origin.z + set.view_height};
  const Vec3f to = head - eye;
  const float d2 = to.x * to.x + to.y * to.y + to.z * to.z;
  if (d2 > set.range * set.range) return false;
  if (d2 > 1) {
    const Vec3f fwd = angle_vectors(angles).forward;
    const float d = root_of(d2);
    const float c = (fwd.x * to.x + fwd.y * to.y + fwd.z * to.z) / d;
    // The cone's edge: the cosine of half the field of view, off the move's own angle vectors
    // (the forward of a yaw that wide is its cosine across x).
    const float edge = angle_vectors({0, set.fov * 0.5f, 0}).forward.x;
    if (c < edge) return false;
  }
  return clear_line(sv, viewer, eye, head) || clear_line(sv, viewer, eye, origin);
}

void sense(Senses& s, const Server& sv, const ClientView& view, int viewer, Vec3f angles, double t,
           ai::Why* why) {
  s.stats.frames++;
  const Edict& me = sv.edicts[static_cast<size_t>(viewer)];
  const Vec3f eye{me.origin.x, me.origin.y, me.origin.z + s.set.view_height};
  for (Contact& c : s.contacts) {
    c.was_visible = c.visible;
    c.visible = false;
  }
  const auto friendly = [&](int ent) {
    return ent >= 1 && ent <= 32 && (s.friends >> (ent - 1) & 1u) != 0;
  };
  for (const ViewEntity& v : view.entities) {
    if (!is_player(sv, v.ent) || friendly(v.ent)) continue;
    if (v.dead) {
      for (Contact& c : s.contacts) {
        if (c.ent != v.ent) continue;
        if (why) {
          why->decide("memory");
          why->fact("ent", c.ent);
          why->choose("dropped", "its player is dead");
        }
        c.ent = -1;
      }
      continue;
    }
    if (!sees_player(sv, s.set, viewer, eye, angles, v.origin)) continue;
    Contact& c = contact_for(s, v.ent);
    if (c.was_visible && ai::age(c.seen, t) <= kFrameGap_s) {
      const double dt = t - c.seen.at;
      if (dt > 0) c.velocity = (v.origin - at_of(c.seen)) * static_cast<float>(1.0 / dt);
    } else {
      c.velocity = {};
      s.stats.acquired++;
      if (why) {
        why->decide("sight");
        why->fact("ent", v.ent);
        why->choose("acquired", "in the cone with a clear line to it");
      }
    }
    ai::see(c.seen, v.origin.x, v.origin.y, v.origin.z, t);
    c.visible = true;
    c.since = t;
    s.stats.seen++;
  }
  for (const ViewSound& snd : view.sounds) {
    if (!s.set.hear_players) break;
    if (!is_player(sv, snd.ent) || snd.ent == viewer || friendly(snd.ent)) continue;
    bool dead = false;  // a death cry is no contact
    for (const ViewEntity& v : view.entities) dead |= v.ent == snd.ent && v.dead;
    if (dead) continue;
    Contact& c = contact_for(s, snd.ent);
    ai::see(c.heard, snd.origin.x, snd.origin.y, snd.origin.z, t);
    if (!c.visible) c.since = t;
    s.stats.heard++;
    if (why && !c.visible) {
      why->decide("hearing");
      why->fact("ent", snd.ent);
      why->choose("heard", snd.sample);
    }
  }
  for (Contact& c : s.contacts) {
    if (c.ent < 0) continue;
    if (c.was_visible && !c.visible) {
      s.stats.lost++;
      if (why) {
        why->decide("sight");
        why->fact("ent", c.ent);
        why->choose("lost", "out of the cone or behind the world");
      }
    }
    if (t - c.since > s.set.forget_s) {
      if (why) {
        why->decide("memory");
        why->fact("ent", c.ent);
        why->choose("forgotten", "neither seen nor heard for forget_s");
      }
      s.stats.forgotten++;
      c.ent = -1;
    }
  }
}

void sense_heard(Senses& s, int ent, int viewer, Vec3f at, double t, ai::Why* why) {
  if (ent < 1 || ent == viewer || (ent <= 32 && (s.friends >> (ent - 1) & 1u) != 0)) return;
  Contact& c = contact_for(s, ent);
  ai::see(c.heard, at.x, at.y, at.z, t);
  if (!c.visible) c.since = t;
  s.stats.heard++;
  if (why && !c.visible) {
    why->decide("hearing");
    why->fact("ent", ent);
    why->choose("heard", "taken in by its ears");
  }
}

const Contact* freshest(const Senses& s, Vec3f from) {
  const Contact* best = nullptr;
  double best_at = -std::numeric_limits<double>::infinity();
  float best_d2 = 0;
  for (const Contact& c : s.contacts) {
    if (c.ent < 0) continue;
    const double at = ai::known(c.seen) ? c.seen.at : c.heard.at;
    const float d2 = dist2(at_of(ai::known(c.seen) ? c.seen : c.heard), from);
    // Seen beats heard at the same moment; the nearer of two at the same moment.
    const bool seen_now = c.visible, best_seen = best && best->visible;
    if (!best || (seen_now && !best_seen) ||
        (seen_now == best_seen && (at > best_at || (at == best_at && d2 < best_d2)))) {
      best = &c;
      best_at = at;
      best_d2 = d2;
    }
  }
  return best;
}

}  // namespace ge::shooter
