// The bot's ears (hearing.hpp): each sound taken in or missed by skill, read, its distance judged.
//
// ge:summary The bot's ears: sounds taken in by skill, read by their sample, the distance guessed
// ge:keywords hear hear_skill_of sound_meaning range-error unique-sound fight-scale
// ge:skill shooter bots
// ge:test tests/shooter_hearing_test.cpp
#include "shooter/hearing.hpp"

#include <cmath>

namespace ge::shooter {

namespace {

float draw(uint32_t& rng) {
  rng = rng * 1664525u + 1013904223u;
  return static_cast<float>(rng >> 8) / 16777216.f;
}

}  // namespace

const char* sound_class_name(SoundClass c) {
  switch (c) {
    case SoundClass::other:
      return "other";
    case SoundClass::move:
      return "move";
    case SoundClass::fire:
      return "fire";
    case SoundClass::pain:
      return "pain";
    case SoundClass::death:
      return "death";
    case SoundClass::pickup:
      return "pickup";
    case SoundClass::power:
      return "power";
    case SoundClass::spawn:
      return "spawn";
  }
  return "?";
}

HearSkill hear_skill_of(AimSkill skill) {
  HearSkill h;
  switch (skill) {
    case AimSkill::perfect:
    case AimSkill::expert:
      break;  // every sound, every meaning, the distance to a tenth
    case AimSkill::average:
      h.unique_near = 0.9f;
      h.unique_far = 0.45f;
      h.common_near = 0.6f;
      h.common_far = 0.1f;
      h.fight_scale = 0.5f;
      h.range_error = 0.3f;
      break;
    case AimSkill::novice:
      h.near = 400;
      h.unique_near = 0.35f;
      h.unique_far = 0;
      h.common_near = 0.35f;
      h.common_far = 0;
      h.fight_scale = 0.4f;
      h.range_error = 0.5f;
      h.knows_samples = false;
      break;
  }
  return h;
}

const SoundMeaning* sound_meaning(std::span<const SoundMeaning> table, std::string_view sample) {
  for (const SoundMeaning& m : table)
    if (m.sample == sample) return &m;
  return nullptr;
}

void hear(const ClientView& view, int viewer, Vec3f ear, const HearSkill& skill,
          std::span<const SoundMeaning> table, bool fighting, uint32_t& rng,
          std::vector<HeardSound>& out, HearStats* stats) {
  out.clear();
  for (const ViewSound& snd : view.sounds) {
    if (snd.ent == viewer) continue;
    const SoundMeaning* m = sound_meaning(table, snd.sample);
    const Vec3f d = snd.origin - ear;
    const float dist = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    const bool close = dist <= skill.near;
    // A sound a novice cannot read is common to him, whatever it is.
    const bool unique = m && m->unique && skill.knows_samples;
    float chance = unique ? (close ? skill.unique_near : skill.unique_far)
                          : (close ? skill.common_near : skill.common_far);
    if (fighting) chance *= skill.fight_scale;
    if (stats) stats->sounds++;
    const float roll = draw(rng);
    if (chance < 1 && roll >= chance) {
      if (stats) stats->missed++;
      continue;
    }
    HeardSound h;
    h.ent = snd.ent;
    h.at = snd.origin;
    h.dist = dist;
    // The direction is heard true; the distance is off by up to `range_error` of it.
    const float off = 1 + skill.range_error * (2 * draw(rng) - 1);
    h.guess = ear + d * off;
    // A novice knows a shot and a cry for what they are; nothing else.
    const bool readable = m && (skill.knows_samples || m->cls == SoundClass::fire ||
                                m->cls == SoundClass::pain || m->cls == SoundClass::death);
    if (readable) {
      h.meaning = m;
      h.cls = m->cls;
      if (stats) stats->read++;
    }
    if (stats) stats->taken++;
    out.push_back(h);
  }
}

}  // namespace ge::shooter
