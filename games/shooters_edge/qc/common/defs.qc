// THE MODS' SHARED DEFINITIONS: the constants, the mod's own fields and globals and the forward
// declarations every variant shares. A variant's `engine.qc` comes first (the engine-side
// globals and fields of its program form, and its builtins); its `glue.qc` comes after this file
// and before the rest of `common/` (what the two forms do differently: printing, effects, frags,
// a missile's first run).
// --- constants ---

float FALSE = 0;
float TRUE = 1;

// movetype
float MOVETYPE_NONE = 0;
float MOVETYPE_WALK = 3;
float MOVETYPE_STEP = 4;
float MOVETYPE_FLY = 5;
float MOVETYPE_TOSS = 6;
float MOVETYPE_PUSH = 7;
float MOVETYPE_NOCLIP = 8;
float MOVETYPE_FLYMISSILE = 9;
float MOVETYPE_BOUNCE = 10;

// solid
float SOLID_NOT = 0;
float SOLID_TRIGGER = 1;
float SOLID_BBOX = 2;
float SOLID_SLIDEBOX = 3;
float SOLID_BSP = 4;

// flags
float FL_FLY = 1;
float FL_SWIM = 2;
float FL_CLIENT = 8;
float FL_INWATER = 16;
float FL_MONSTER = 32;
float FL_ITEM = 256;
float FL_ONGROUND = 512;

// takedamage
float DAMAGE_NO = 0;
float DAMAGE_YES = 1;
float DAMAGE_AIM = 2;

// deadflag
float DEAD_NO = 0;
float DEAD_DYING = 1;
float DEAD_DEAD = 2;
float DEAD_RESPAWNABLE = 3;

// point contents
float CONTENT_EMPTY = -1;
float CONTENT_SOLID = -2;
float CONTENT_WATER = -3;
float CONTENT_SLIME = -4;
float CONTENT_LAVA = -5;
float CONTENT_SKY = -6;

// the weapons' bits in .items and .weapon (the status bar reads them)
float W_SCATTER = 1;
float W_DOUBLE = 2;
float W_SPIKER = 4;
float W_HEAVY_SPIKER = 8;
float W_GRENADES = 16;
float W_ROCKETS = 32;
float W_ARC = 64;
float W_BLADE = 4096;

// the other bits in .items
float IT_SHELLS = 256;
float IT_NAILS = 512;
float IT_ROCKETS = 1024;
float IT_CELLS = 2048;
float IT_ARMOR1 = 8192;
float IT_ARMOR2 = 16384;
float IT_ARMOR3 = 32768;
float IT_ARMOR_ALL = 57344;
float IT_KEY1 = 131072;
float IT_KEY2 = 262144;
float IT_INVULNERABILITY = 1048576;
float IT_QUAD = 4194304;

// sound channels and how far a sound carries
float CHAN_AUTO = 0;
float CHAN_WEAPON = 1;
float CHAN_VOICE = 2;
float CHAN_ITEM = 3;
float CHAN_BODY = 4;
float ATTN_NONE = 0;
float ATTN_NORM = 1;
float ATTN_IDLE = 2;
float ATTN_STATIC = 3;

// print levels
float PRINT_LOW = 0;
float PRINT_MEDIUM = 1;
float PRINT_HIGH = 2;
float PRINT_CHAT = 3;

// message destinations and multicast sets
float MSG_BROADCAST = 0;
float MSG_ONE = 1;
float MSG_ALL = 2;
float MULTICAST_ALL = 0;
float MULTICAST_PHS = 1;
float MULTICAST_PVS = 2;

// the temporary-effect message and its kinds
float SVC_TEMPENTITY = 23;
float TE_SPIKE = 0;
float TE_GUNSHOT = 2;
float TE_EXPLOSION = 3;
float TE_LIGHTNING2 = 6;
float TE_BLOOD = 12;

// effects
float EF_MUZZLEFLASH = 2;
float EF_DIMLIGHT = 8;  // a glow round the player (a power held)
float EF_BLUE = 64;     // its colour: the quad's
float EF_RED = 128;     // the shield's

// --- the mod's own fields ---

.float attack_finished;   // when the held weapon may fire again
.float pain_finished;     // when the next pain sound may play
.float air_finished;      // when a player under water starts to drown
.float respawn_ready;     // a dead player may respawn after this
.float spawn_time;        // when the player last came in
.float delay;             // seconds before a trigger fires its targets
.float wait;              // seconds a mover or trigger waits
.float speed;             // a mover's units per second, a missile's
.float lip;               // the units a door leaves showing
.float height;            // the units a plat travels (its own height when not set)
.float dmg;               // damage a mover or a trigger deals
.float state;             // a mover's: STATE_ below
.vector pos1, pos2;       // a mover's two ends
.vector finaldest;        // where a calc_move is heading
.void() move_done;        // what a calc_move calls when it arrives
.string killtarget;
.string deathtype;        // how a death is told
.float respawn_time;      // an item's seconds to come back
.float hazard_finished;   // when lava or slime next hurts
.float super_damage_finished;  // the quad runs out
.float invincible_finished;    // the shield runs out
.float light_lev;         // a light's brightness (the map's "light" key)
.float style;             // a light's style
.void() th_die;           // what a death does to this kind of thing (unset: a player's)
.void() th_hurt;          // what a hit that leaves it alive does (unset: a player's pain)
.float show_hostile;      // a player is noisy until this (he fired): monsters hear him

float STATE_TOP = 0;
float STATE_BOTTOM = 1;
float STATE_UP = 2;
float STATE_DOWN = 3;

// --- the mod's own globals ---

float fraglimit;
float timelimit;
float match_over;
float intermission_time;  // when the match ended (0: still on)
entity last_spawn;        // the spawn point used last
entity activator;         // the entity that set off the targets being fired
entity damage_attacker;   // who dealt the hit being taken, for th_hurt and th_die

// --- forward declarations (the files are compiled in order) ---

void() remove_self;
void() noop;
void(entity targ, entity inflictor, entity attacker, float damage) damage;
void(entity inflictor, entity attacker, float damage, entity ignore, string how) radius_damage;
void() use_targets;
void(vector dest, float spd, void() done) calc_move;
void() player_respawn;
float(float w) weapon_rank;
void() weapon_select_best;
void(float w) weapon_set;
