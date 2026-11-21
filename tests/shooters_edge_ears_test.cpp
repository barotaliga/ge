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
namespace sb = ge::shooterbots;

TEST(every_item_kind_is_heard_by_a_word_in_the_table) {
  const bt::ItemKind kinds[] = {bt::ItemKind::health, bt::ItemKind::mega,    bt::ItemKind::armor1,
                                bt::ItemKind::armor2, bt::ItemKind::armor3,  bt::ItemKind::shells,
                                bt::ItemKind::nails,  bt::ItemKind::rockets, bt::ItemKind::cells,
                                bt::ItemKind::quad,   bt::ItemKind::pent,    bt::ItemKind::weapon};
  const bt::BotArsenal& a = bt::first_arsenal();
  for (bt::ItemKind k : kinds) {
    const ge::shooterbots::ItemWords* w = ge::shooterbots::item_words(a.heard->words, k, 0);
    CHECK(w != nullptr);
    const std::string_view word = w != nullptr ? w->heard : std::string_view();
    bool found = false;
    for (const sb::SoundMeaning& m : bt::sound_table(a))
      found |=
          m.word == word && (m.cls == sb::SoundClass::pickup || m.cls == sb::SoundClass::power);
    CHECK_MSG(found, std::string("no pickup sample is read as ") + std::string(word));
  }
}

TEST(the_spawn_and_the_key_pickups_are_unique) {
  for (const char* sample : {"items/armor1.wav", "items/r_item2.wav", "items/damage.wav",
                             "misc/r_tele1.wav", "weapons/pkup.wav"}) {
    const sb::SoundMeaning* m = sb::sound_meaning(bt::sound_table(bt::first_arsenal()), sample);
    CHECK_MSG(m && m->unique, sample);
  }
  CHECK(sb::sound_meaning(bt::sound_table(bt::first_arsenal()), "weapons/rocket1i.wav")->cls ==
        sb::SoundClass::fire);
}
