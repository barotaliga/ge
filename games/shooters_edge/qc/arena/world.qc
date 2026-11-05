// THE ARENA'S WORLD: worldspawn, the frame's start (the settings, the time and frag limits, the
// end of the match) and the spawn points. The map's moving and touching things are common/map.qc.

// --- worldspawn and the match ---

float clock_said;  // the clock's seconds left last looked at (0: not yet)

void() worldspawn = {
  last_spawn = world;
  match_over = 0;
  intermission_time = 0;
  player_precache();
  weapon_precache();
  item_precache();
  precache_model("progs/backpack.mdl");
  precache_sound("misc/r_tele1.wav");
  precache_sound("doors/drclos4.wav");
  precache_sound("doors/doormv1.wav");
  precache_sound("plats/plat1.wav");
  precache_sound("plats/plat2.wav");
  precache_sound("buttons/switch21.wav");
  precache_sound("weapons/lock4.wav");
  precache_sound("weapons/pkup.wav");
  precache_sound("items/armor1.wav");
  clock_said = 0;
  // A loaded game's words bring their sounds: the powers wearing off and the lobby's countdown.
  if (game_words("power_quad_off", "", "", "", "") != "") {
    precache_sound("items/damage2.wav");
    precache_sound("items/protect2.wav");
    precache_sound("buttons/switch04.wav");
  }
  // Light style 0 is the steady light; 1.. flicker and pulse.
  lightstyle(0, "m");
  lightstyle(1, "mmnmmommommnonmmonqnmmo");
  lightstyle(2, "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba");
};

// The teams' frags in a game of exactly two named teams: whether there are two (`team_a` and
// `team_b` then named, `frags_a` and `frags_b` theirs).
string team_a, team_b;
float frags_a, frags_b;
float() two_teams = {
  local entity p;
  local string t;
  team_a = "";
  team_b = "";
  frags_a = 0;
  frags_b = 0;
  p = find(world, classname, "player");
  while (p) {
    t = infokey(p, "team");
    if (t != "") {
      if (team_a == "" || t == team_a) {
        team_a = t;
        frags_a = frags_a + p.frags;
      } else if (team_b == "" || t == team_b) {
        team_b = t;
        frags_b = frags_b + p.frags;
      } else {
        return FALSE;
      }
    }
    p = find(p, classname, "player");
  }
  return team_b != "";
};

float(float f) positive = {
  if (f > 0) return f;
  return 0;
};

// A share as text with one decimal ("56.0").
string(float share) one_decimal = {
  local float tenths, whole;
  tenths = rint(share * 10);
  whole = floor(tenths / 10);
  return strcat(ftos(whole), ".", ftos(tenths - whole * 10));
};

// The teams' scores in the loaded game's words: each team's frags and its share of the frags.
void() say_team_scores = {
  local float sum;
  if (!teamplay || !two_teams()) return;
  sum = 0;
  if (frags_a > 0) sum = sum + frags_a;
  if (frags_b > 0) sum = sum + frags_b;
  if (sum <= 0) sum = 1;
  say_all(PRINT_HIGH, game_words("team_scores_head", "", "", "", ""), "", "", "", "", "");
  say_all(PRINT_HIGH, game_words("team_scores_row", team_a, ftos(frags_a),
                                 one_decimal(100 * positive(frags_a) / sum), ""), "", "", "", "", "");
  say_all(PRINT_HIGH, game_words("team_scores_row", team_b, ftos(frags_b),
                                 one_decimal(100 * positive(frags_b) / sum), ""), "", "", "", "", "");
  say_all(PRINT_HIGH, game_words("team_scores_tail", "", "", "", ""), "", "", "", "", "");
};

// THE CLOCK in the loaded game's words (none in ours): each whole minute left, then 30, 15 and the
// last ten seconds, and in a two-team game who leads at each minute. Nothing at the start.
void() clock_frame = {
  local float left, prev, m;
  if (!timelimit || game_words("clock_minutes", "", "", "", "") == "") return;
  left = ceil(timelimit * 60 - time);
  prev = clock_said;
  if (left == prev) return;
  clock_said = left;
  if (prev == 0 || left <= 0) return;
  if (left >= 60) {
    m = floor(left / 60);
    if (left - m * 60 != 0) return;
    if (m == 1) say_all(PRINT_HIGH, game_words("clock_minute", ftos(m), "", "", ""), "", "", "", "", "");
    else say_all(PRINT_HIGH, game_words("clock_minutes", ftos(m), "", "", ""), "", "", "", "", "");
    if (teamplay && two_teams()) {
      if (frags_a == frags_b) say_all(PRINT_HIGH, game_words("clock_tie", "", "", "", ""), "", "", "", "", "");
      else if (frags_a > frags_b && frags_a - frags_b == 1)
        say_all(PRINT_HIGH, game_words("clock_lead_one", team_a, "1", "", ""), "", "", "", "", "");
      else if (frags_a > frags_b)
        say_all(PRINT_HIGH, game_words("clock_lead", team_a, ftos(frags_a - frags_b), "", ""), "", "", "", "", "");
      else if (frags_b - frags_a == 1)
        say_all(PRINT_HIGH, game_words("clock_lead_one", team_b, "1", "", ""), "", "", "", "", "");
      else
        say_all(PRINT_HIGH, game_words("clock_lead", team_b, ftos(frags_b - frags_a), "", ""), "", "", "", "", "");
    }
    return;
  }
  if (left == 30 || left == 15 || left <= 10) {
    if (left == 1) say_all(PRINT_HIGH, game_words("clock_second", "1", "", "", ""), "", "", "", "", "");
    else say_all(PRINT_HIGH, game_words("clock_seconds", ftos(left), "", "", ""), "", "", "", "", "");
  }
};

// Ends the match: everyone is held where they stand, the scores are printed (in the loaded game's
// words: its line and the teams' scores), and five seconds later the map starts again.
void(string why) end_match = {
  local entity p;
  local string over;
  match_over = 1;
  intermission_time = time;
  over = game_words("match_over", "", "", "", "");
  if (over != "") say_all(PRINT_HIGH, over, "", "", "", "", "");
  else say_all(PRINT_HIGH, why, "\n", "", "", "", "");
  p = find(world, classname, "player");
  while (p) {
    p.takedamage = DAMAGE_NO;
    p.movetype = MOVETYPE_NONE;
    p.velocity = '0 0 0';
    p.button0 = 0;
    if (over == "") say_all(PRINT_HIGH, p.netname, ": ", ftos(p.frags), " frags\n", "", "");
    p = find(p, classname, "player");
  }
  if (over != "") say_team_scores();
};

void() StartFrame = {
  local entity p;
  deathmatch = cvar("deathmatch");
  teamplay = cvar("teamplay");
  fraglimit = cvar("fraglimit");
  timelimit = cvar("timelimit");
  if (match_over) {
    if (time > intermission_time + 5) {
      changelevel(mapname);
      intermission_time = time + 1000;  // asked once
    }
    return;
  }
  clock_frame();
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

// A teleporter sends a living player, and arriving takes the spot (combat.qc): whoever stands on
// the destination dies.
float(entity who) teleport_takes = { return who.classname == "player" && who.health > 0; };
void(entity who) teleport_arrived = { telefrag(who); };
