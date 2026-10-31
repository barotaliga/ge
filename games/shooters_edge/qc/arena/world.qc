// THE ARENA'S WORLD: worldspawn, the frame's start (the settings, the time and frag limits, the
// end of the match) and the spawn points. The map's moving and touching things are common/map.qc.

// --- worldspawn and the match ---

void() worldspawn = {
  last_spawn = world;
  match_over = 0;
  intermission_time = 0;
  player_precache();
  weapon_precache();
  item_precache();
  precache_sound("misc/r_tele1.wav");
  precache_sound("doors/drclos4.wav");
  precache_sound("doors/doormv1.wav");
  precache_sound("plats/plat1.wav");
  precache_sound("plats/plat2.wav");
  precache_sound("buttons/switch21.wav");
  precache_sound("weapons/lock4.wav");
  precache_sound("weapons/pkup.wav");
  precache_sound("items/armor1.wav");
  // Light style 0 is the steady light; 1.. flicker and pulse.
  lightstyle(0, "m");
  lightstyle(1, "mmnmmommommnonmmonqnmmo");
  lightstyle(2, "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba");
};

// Ends the match: everyone is held where they stand, the scores are printed, and five seconds
// later the map starts again.
void(string why) end_match = {
  local entity p;
  match_over = 1;
  intermission_time = time;
  say_all(PRINT_HIGH, why, "\n", "", "", "", "");
  p = find(world, classname, "player");
  while (p) {
    p.takedamage = DAMAGE_NO;
    p.movetype = MOVETYPE_NONE;
    p.velocity = '0 0 0';
    p.button0 = 0;
    say_all(PRINT_HIGH, p.netname, ": ", ftos(p.frags), " frags\n", "", "");
    p = find(p, classname, "player");
  }
};

void() StartFrame = {
  local entity p;
  deathmatch = cvar("deathmatch");
  fraglimit = cvar("fraglimit");
  timelimit = cvar("timelimit");
  if (match_over) {
    if (time > intermission_time + 5) {
      changelevel(mapname);
      intermission_time = time + 1000;  // asked once
    }
    return;
  }
  if (timelimit && time >= timelimit * 60) {
    end_match("Timelimit hit.");
    return;
  }
  if (fraglimit) {
    p = find(world, classname, "player");
    while (p) {
      if (p.frags >= fraglimit) {
        end_match("Fraglimit hit.");
        return;
      }
      p = find(p, classname, "player");
    }
  }
};

void() main = {};

void() info_player_deathmatch = {};
void() info_player_start = {};
void() info_intermission = {};
