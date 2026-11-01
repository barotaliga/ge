// THE BOT'S EARS: an expert takes in every sound and reads it, its distance judged within a tenth;
// an average player misses far common sounds more than close unique ones; a novice takes in
// nothing far off and reads a pickup as no more than a noise.
//
// ge:summary Tests: sounds taken in by skill, read by their sample, the distance guessed
// ge:keywords test hearing hear hear_skill_of sound_meaning range-error unique-sound
// ge:skill shooter testing
#include "check.hpp"
#include "shooter/hearing.hpp"

namespace sh = ge::shooter;

namespace {

const sh::SoundMeaning kTable[] = {
    {"pickup.wav", sh::SoundClass::pickup, true, "mh"},
    {"shot.wav", sh::SoundClass::fire, false, ""},
};

sh::ClientView view_of(const char* sample, float x, int n) {
  sh::ClientView v;
  for (int i = 0; i < n; i++) v.sounds.push_back({2, {x, 0, 0}, 0, sample, 1, 1});
  return v;
}

int64_t taken(sh::AimSkill skill, const char* sample, float x, bool fighting = false) {
  uint32_t rng = 7;
  std::vector<sh::HeardSound> out;
  sh::HearStats st;
  sh::hear(view_of(sample, x, 400), 1, {0, 0, 0}, sh::hear_skill_of(skill), kTable, fighting, rng,
           out, &st);
  CHECK(st.sounds == 400 && st.taken == static_cast<int64_t>(out.size()));
  return st.taken;
}

}  // namespace

TEST(an_expert_takes_in_and_reads_every_sound) {
  uint32_t rng = 1;
  std::vector<sh::HeardSound> out;
  sh::hear(view_of("pickup.wav", 900, 50), 1, {0, 0, 0}, sh::hear_skill_of(sh::AimSkill::expert),
           kTable, true, rng, out);
  CHECK(out.size() == 50);
  for (const sh::HeardSound& h : out) {
    CHECK(h.meaning && h.meaning->word == "mh" && h.cls == sh::SoundClass::pickup);
    CHECK_MSG(h.guess.x >= 810 && h.guess.x <= 990, "the guess is within a tenth of 900");
    CHECK(h.guess.y == 0 && h.dist == 900);
  }
}

TEST(an_average_ear_favours_close_and_unique_sounds) {
  const int64_t near_unique = taken(sh::AimSkill::average, "pickup.wav", 300);
  const int64_t far_unique = taken(sh::AimSkill::average, "pickup.wav", 900);
  const int64_t far_common = taken(sh::AimSkill::average, "shot.wav", 900);
  const int64_t fighting = taken(sh::AimSkill::average, "pickup.wav", 300, true);
  CHECK(near_unique > far_unique && far_unique > far_common && near_unique > fighting);
  CHECK(far_common > 0 && far_common < 100);
}

TEST(a_novice_hears_only_what_is_close_and_not_what_it_means) {
  CHECK(taken(sh::AimSkill::novice, "pickup.wav", 900) == 0);
  uint32_t rng = 3;
  std::vector<sh::HeardSound> out;
  sh::hear(view_of("pickup.wav", 200, 200), 1, {0, 0, 0}, sh::hear_skill_of(sh::AimSkill::novice),
           kTable, false, rng, out);
  CHECK(!out.empty() && out.size() < 200);
  for (const sh::HeardSound& h : out) CHECK(!h.meaning && h.cls == sh::SoundClass::other);
  out.clear();
  sh::hear(view_of("shot.wav", 200, 200), 1, {0, 0, 0}, sh::hear_skill_of(sh::AimSkill::novice),
           kTable, false, rng, out);
  CHECK(!out.empty() && out[0].cls == sh::SoundClass::fire);
}

TEST(a_players_own_sounds_are_not_heard) {
  uint32_t rng = 1;
  std::vector<sh::HeardSound> out;
  sh::hear(view_of("shot.wav", 10, 5), 2, {0, 0, 0}, sh::hear_skill_of(sh::AimSkill::expert),
           kTable, false, rng, out);
  CHECK(out.empty());
}
