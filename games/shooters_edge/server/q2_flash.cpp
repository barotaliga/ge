// Quake II's muzzle flashes as data (q2_flash.hpp). The values and what each shows and plays:
// porting/shooters-edge/q2-flashes.md; the guns' places are rounded to 8 units, which is all a
// light 200 units wide asks (the window puts the look at the model's gun tip itself).
//
// ge:summary Quake II's muzzle flashes by value: the player's and the monsters' tables
// ge:keywords q2 muzzle-flash flash-table monster-flash-value
// ge:skill shooter
// ge:test tests/shooters_edge_q2_flash_test.cpp
#include "shooters_edge/server/q2_flash.hpp"

#include <array>

namespace shooters_edge::server {

namespace {

using S = Q2FlashSound;

constexpr float kYellow[3] = {1, 1, 0}, kOrange[3] = {1, 0.5f, 0}, kRocket[3] = {1, 0.5f, 0.2f};
constexpr float kRail[3] = {0.5f, 0.5f, 1}, kGreen[3] = {0, 1, 0};

constexpr Q2Flash make(const float (&c)[3], const char* look, S a = {}, S b = {}, S d = {}) {
  Q2Flash f;
  for (int i = 0; i < 3; i++) f.rgb[i] = c[i];
  f.look = look;
  f.sounds[0] = a;
  f.sounds[1] = b;
  f.sounds[2] = d;
  return f;
}

constexpr S kMachineGun{"weapons/machgf%db.wav", 5};

std::array<Q2Flash, 40> player_table() {
  std::array<Q2Flash, 40> t{};
  t[0] = make(kYellow, "blaster", {"weapons/blastf1a.wav"});
  t[1] = make(kYellow, "gun", kMachineGun);
  t[2] = make(kYellow, "gun", {"weapons/shotgf1b.wav"}, {"weapons/shotgr1b.wav", 0, 0, 0.1f});
  const float cg1[3] = {1, 0.25f, 0}, cg2[3] = {1, 0.5f, 0};
  t[3] = make(cg1, "gun", kMachineGun);
  t[4] = make(cg2, "gun", kMachineGun, {"weapons/machgf%db.wav", 5, 1, 0.05f});
  t[4].radius = 225;
  t[5] = make(kYellow, "gun", kMachineGun, {"weapons/machgf%db.wav", 5, 1, 0.033f},
              {"weapons/machgf%db.wav", 5, 1, 0.066f});
  t[5].radius = 250;
  t[6] = make(kRail, "rail", {"weapons/railgf1a.wav"}, {"weapons/railgr1b.wav", 0, 0, 0.4f});
  t[7] = make(kRocket, "rocket", {"weapons/rocklf1a.wav"}, {"weapons/rocklr1b.wav", 0, 0, 0.1f});
  t[8] = make(kOrange, "rocket", {"weapons/grenlf1a.wav"}, {"weapons/grenlr1b.wav", 0, 0, 0.1f});
  t[12] = make(kGreen, "bfg", {"weapons/bfg__f1y.wav"});
  t[13] = make(kYellow, "gun", {"weapons/sshotf1b.wav"});
  t[14] = make(kYellow, "blaster", {"weapons/hyprbf1a.wav"});
  // The mission packs' guns (a mod's game library sends them).
  const float ripper[3] = {1, 0.5f, 0.5f}, blue[3] = {0, 0, 1}, etf[3] = {0.9f, 0.7f, 0};
  t[16] = make(ripper, "blaster", {"weapons/rippfire.wav"});
  t[17] = make(blue, "blaster", {"weapons/hyprbf1a.wav"});
  t[18] = make(ripper, "rocket", {"weapons/plasshot.wav"});
  t[19] = make(kGreen, nullptr);
  t[20] = make(ripper, nullptr);
  t[30] = make(etf, "gun", {"weapons/nail1.wav"});
  t[31] = make(kOrange, "rocket", {"weapons/grenlf1a.wav"}, {"weapons/proxlr1a.wav", 0, 0, 0.1f});
  t[32] = make(kYellow, "gun", {"weapons/shotg2.wav"});
  t[33] = make(kYellow, nullptr);
  t[33].light_s = 0.1f;
  t[34] = make(kGreen, "blaster", {"weapons/blastf1a.wav"});
  const float dark[3] = {-1, -1, -1}, red[3] = {1, 0, 0}, cyan[3] = {0, 1, 1};
  t[35] = make(dark, nullptr, {"weapons/disint2.wav"});
  t[36] = make(red, nullptr);
  t[37] = make(kYellow, nullptr);
  t[38] = make(blue, nullptr);
  t[39] = make(cyan, nullptr);
  for (int v = 36; v <= 39; v++) t[size_t(v)].light_s = 0.1f;
  return t;
}

// A monster's gun group: its values, what it shows and plays, where the gun is.
struct Group {
  int first, last;
  Q2Flash flash;
};

Q2Flash gun(const float (&c)[3], const char* look, S sound, float ahead, float right, float up,
            bool puff = false, bool smoke = false) {
  Q2Flash f = make(c, look, sound);
  f.ahead = ahead;
  f.right = right;
  f.up = up;
  f.puff = puff;
  f.smoke = smoke;
  return f;
}

const std::array<Q2Flash, 289>& monster_table() {
  static const std::array<Q2Flash, 289> table = [] {
    std::array<Q2Flash, 289> t{};
    const auto put = [&t](int first, int last, const Q2Flash& f) {
      for (int v = first; v <= last; v++) t[size_t(v)] = f;
    };
    const S infantry{"infantry/infatck1.wav"}, none{};
    const S soldier_mg{"soldier/solatck3.wav"}, soldier_bolt{"soldier/solatck2.wav"};
    const S soldier_sg{"soldier/solatck1.wav"};
    put(1, 3, gun(kYellow, "blaster", {"tank/tnkatck3.wav"}, 16, -24, 32));
    put(4, 22, gun(kYellow, "gun", {"tank/tnkatk2%c.wav", 5}, 16, 24, 24, true, true));
    put(23, 25, gun(kRocket, "rocket", {"tank/tnkatck1.wav"}, 8, 24, 48));
    put(26, 38, gun(kYellow, "gun", infantry, 8, 16, 16, true, true));
    // The soldier: blaster, shotgun, machine gun in each posture.
    for (const int v : {39, 40, 83, 86, 89, 92, 95, 98})
      put(v, v, gun(kYellow, "blaster", soldier_bolt, 24, 8, 0));
    for (const int v : {41, 42, 84, 87, 90, 93, 96, 99})
      put(v, v, gun(kYellow, "gun", soldier_sg, 24, 8, 0, false, true));
    for (const int v : {43, 44, 85, 88, 91, 94, 97, 100})
      put(v, v, gun(kYellow, "gun", soldier_mg, 24, 8, 0, true, true));
    put(45, 52, gun(kYellow, "gun", {"gunner/gunatck2.wav"}, 32, 0, 24, true, true));
    put(53, 56, gun(kOrange, "rocket", {"gunner/gunatck3.wav"}, 8, -16, 8));
    put(57, 57, gun(kRocket, "rocket", {"chick/chkatck2.wav"}, 24, -8, 40));
    put(58, 59, gun(kYellow, "blaster", {"flyer/flyatck3.wav"}, 16, 0, -16));
    put(60, 60, gun(kYellow, "blaster", {"medic/medatck1.wav"}, 16, 8, 16));
    put(61, 61, gun(kRail, "rail", none, 32, 16, 32));
    put(62, 62, gun(kYellow, "blaster", {"hover/hovatck1.wav"}, 32, 0, 8));
    put(63, 63, gun(kYellow, "gun", infantry, 16, 8, 8, true, true));
    put(64, 69, gun(kYellow, "gun", infantry, 32, 32, 88, true, true));
    put(70, 72, gun(kRocket, "rocket", {"tank/rocket.wav"}, 16, -32, 88));
    put(73, 77,
        gun(kYellow, "gun", {"infantry/infatck1.wav", 0, 1, 0, 0}, 32, -40, 72, true, true));
    put(78, 81, gun(kRocket, "rocket", {"tank/rocket.wav"}, 24, 0, 8));
    put(82, 82, gun(kYellow, "blaster", {"floater/fltatck1.wav"}, 32, 0, 8));
    put(101, 101, gun({0.5f, 1, 0.5f}, "bfg", none, 16, -16, 64));
    put(102, 118, gun(kYellow, "blaster", {"makron/blaster.wav"}, 0, 0, 56));
    put(119, 119, gun(kRail, "rail", none, -16, 8, 72));
    put(120, 125, gun(kYellow, "gun", {"boss3/xfire.wav"}, 80, -48, 96, true, true));
    put(126, 131, gun(kYellow, "gun", none, 80, 48, 96, true, true));
    put(132, 132, gun({0.5f, 1, 0.5f}, "bfg", none, 8, -8, 112));
    put(133, 137, gun(kYellow, "gun", none, 32, 40, 72, true, true));
    // The mission packs' monsters (a mod's game library sends them).
    const S carrier_l{"infantry/infatck1.wav", 0, 1, 0, 0};
    put(138, 138, gun(kYellow, "gun", carrier_l, 56, -32, 24, true, true));
    put(152, 152, gun(kYellow, "gun", carrier_l, 56, -32, 24, true, true));
    put(139, 139, gun(kYellow, "gun", none, 56, 32, 24, true, true));
    put(153, 153, gun(kYellow, "gun", none, 56, 32, 24, true, true));
    put(141, 141, gun(kYellow, "gun", infantry, 16, 0, 0, true, true));
    put(142, 142, gun(kRocket, "rocket", {"chick/chkatck2.wav"}, 16, 0, 0));
    put(143, 143, gun(kYellow, "blaster", soldier_bolt, 16, 0, 0));
    const S green_bolt{"tank/tnkatck3.wav"};
    put(144, 144, gun(kGreen, "blaster", green_bolt, 24, 0, 8));
    put(145, 145, gun(kGreen, "blaster", green_bolt, 32, 0, 8));
    put(146, 146, gun(kGreen, "blaster", green_bolt, 16, 8, 16));
    put(147, 147, gun(kRail, "rail", none, 32, 0, 8));
    put(148, 148, gun({-1, -1, -1}, nullptr, {"weapons/disint2.wav"}, 56, 16, 88));
    put(149, 149, gun(kGreen, "blaster", green_bolt, 32, 56, 80));
    put(150, 150, gun(kRail, "rail", none, 64, -16, 80));
    put(156, 164, gun(kGreen, "blaster", green_bolt, 64, 16, 88));
    put(165, 175, gun(kGreen, "blaster", green_bolt, 32, 56, 80));
    put(176, 182, gun(kGreen, "blaster", green_bolt, 56, -8, 80));
    put(183, 190, gun(kGreen, "blaster", green_bolt, 72, 8, 88));
    put(191, 191, gun(kRocket, "rocket", {"tank/rocket.wav"}, 0, 0, -8));
    Q2Flash beam = gun(kYellow, nullptr, none, 32, 0, 8);
    beam.radius = 300;
    beam.light_s = 0.2f;
    put(151, 151, beam);
    beam.ahead = 72;
    beam.right = -16;
    beam.up = 96;
    put(195, 210, beam);
    // The newer game's monsters (values past 210).
    put(211, 218, gun({1, 0.5f, 0.5f}, "blaster", {"weapons/rippfire.wav"}, 32, 8, 0));
    put(219, 226, gun({0, 0, 1}, "blaster", {"weapons/hyprbf1a.wav"}, 32, 8, 0));
    put(227, 227, gun(kYellow, "blaster", {"weapons/hyprbf1a.wav"}, 88, 48, 64));
    put(228, 229, gun(kRail, "rail", none, 64, 0, 24));
    put(230, 231, gun(kRail, "rail", none, 48, -8, 72));
    put(232, 239, gun(kYellow, "gun", infantry, 16, 16, 16, true, true));
    put(260, 260, gun(kYellow, "gun", infantry, 16, 16, 16, true, true));
    put(240, 241, gun({0, 0, 1}, "gun", {"guncmdr/gcdratck2.wav"}, 24, 8, 24));
    put(242, 247, gun(kOrange, "rocket", {"guncmdr/gcdratck3.wav"}, 24, 8, 16));
    put(251, 251, t[39]);
    put(252, 252, t[41]);
    put(253, 253, t[43]);
    put(254, 254, t[211]);
    put(255, 255, t[219]);
    put(256, 259, gun(kOrange, "rocket", {"gunner/gunatck3.wav"}, 32, -8, 16));
    put(261, 262, gun(kOrange, "rocket", {"gunner/gunatck3.wav"}, 32, 0, 56));
    put(263, 263, t[62]);
    put(264, 264, t[145]);
    put(265, 276, gun(kYellow, "blaster", {"medic/medatck1.wav"}, 32, 8, 16));
    put(277, 288, gun(kGreen, "blaster", green_bolt, 32, 8, 16));
    return t;
  }();
  return table;
}

}  // namespace

const Q2Flash* q2_player_flash(int value) {
  static const std::array<Q2Flash, 40> table = player_table();
  if (value < 0 || value >= static_cast<int>(table.size())) return nullptr;
  const Q2Flash& f = table[size_t(value)];
  return f.rgb[0] == 0 && f.rgb[1] == 0 && f.rgb[2] == 0 ? nullptr : &f;
}

const Q2Flash* q2_monster_flash(int value) {
  const std::array<Q2Flash, 289>& table = monster_table();
  if (value < 1 || value >= static_cast<int>(table.size())) return nullptr;
  const Q2Flash& f = table[size_t(value)];
  return f.rgb[0] == 0 && f.rgb[1] == 0 && f.rgb[2] == 0 ? nullptr : &f;
}

int q2_monster_flash_value(std::string_view classname, std::string_view look) {
  struct Row {
    std::string_view classname, look;
    int value;
  };
  static constexpr Row kRows[] = {
      {"monster_tank", "blaster", 1},      {"monster_tank_commander", "blaster", 1},
      {"monster_tank", "gun", 4},          {"monster_tank_commander", "gun", 4},
      {"monster_tank", "rocket", 23},      {"monster_tank_commander", "rocket", 23},
      {"monster_infantry", "gun", 26},     {"monster_soldier_light", "blaster", 39},
      {"monster_soldier", "gun", 41},      {"monster_soldier_ss", "gun", 43},
      {"monster_gunner", "gun", 45},       {"monster_gunner", "rocket", 53},
      {"monster_chick", "rocket", 57},     {"monster_flyer", "blaster", 58},
      {"monster_medic", "blaster", 60},    {"monster_gladiator", "rail", 61},
      {"monster_hover", "blaster", 62},    {"monster_supertank", "gun", 64},
      {"monster_supertank", "rocket", 70}, {"monster_boss2", "gun", 73},
      {"monster_boss2", "rocket", 78},     {"monster_floater", "blaster", 82},
      {"monster_makron", "bfg", 101},      {"monster_makron", "blaster", 102},
      {"monster_makron", "rail", 119},     {"monster_jorg", "gun", 120},
      {"monster_jorg", "bfg", 132},
  };
  for (const Row& r : kRows)
    if (r.classname == classname && r.look == look) return r.value;
  return 0;
}

}  // namespace shooters_edge::server
