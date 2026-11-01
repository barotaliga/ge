// WHAT A FIGHTER HEARS: every sample the baseline mod plays for a pickup, a power or a spawn is in
// the table, and every item's pickup is read as the word the table gives its sample.
//
// ge:summary Tests: the mod's sample table and each item's pickup word
// ge:keywords test ears sound_table hears_as pickup-sound
// ge:skill shooter testing
#include "check.hpp"
#include "shooters_edge/bots/ears.hpp"

namespace bt = shooters_edge::bots;
namespace sh = ge::shooter;

TEST(every_item_kind_is_heard_by_a_word_in_the_table) {
  const bt::ItemKind kinds[] = {bt::ItemKind::health, bt::ItemKind::mega,    bt::ItemKind::armor1,
                                bt::ItemKind::armor2, bt::ItemKind::armor3,  bt::ItemKind::shells,
                                bt::ItemKind::nails,  bt::ItemKind::rockets, bt::ItemKind::cells,
                                bt::ItemKind::quad,   bt::ItemKind::pent,    bt::ItemKind::weapon};
  for (bt::ItemKind k : kinds) {
    bt::ItemSpot it;
    it.kind = k;
    const std::string_view word = bt::hears_as(it);
    bool found = false;
    for (const sh::SoundMeaning& m : bt::sound_table())
      found |=
          m.word == word && (m.cls == sh::SoundClass::pickup || m.cls == sh::SoundClass::power);
    CHECK_MSG(found, std::string("no pickup sample is read as ") + std::string(word));
  }
}

TEST(the_spawn_and_the_key_pickups_are_unique) {
  for (const char* sample : {"items/armor1.wav", "items/r_item2.wav", "items/damage.wav",
                             "misc/r_tele1.wav", "weapons/pkup.wav"}) {
    const sh::SoundMeaning* m = sh::sound_meaning(bt::sound_table(), sample);
    CHECK_MSG(m && m->unique, sample);
  }
  CHECK(sh::sound_meaning(bt::sound_table(), "weapons/rocket1i.wav")->cls == sh::SoundClass::fire);
}
