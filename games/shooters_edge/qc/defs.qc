// THE BASELINE MOD'S DEFINITIONS: the engine-side globals and fields (their order is the layout
// the engine reads by offset, so it never changes), the builtins this mod calls by number, and
// the constants the rest of the mod shares. A mod built on this one keeps the first two parts
// as they are and adds its own globals and fields after `end_sys_fields`.

// --- the engine-side globals (28..89), in this order ---

entity self;
entity other;
entity world;
float time;
float frametime;
entity newmis;          // a missile made this frame: the server runs it once at once
float force_retouch;    // non-zero: every entity re-touches what it stands in
string mapname;
float serverflags;
float total_secrets;
float total_monsters;
float found_secrets;
float killed_monsters;
float parm1, parm2, parm3, parm4, parm5, parm6, parm7, parm8;
float parm9, parm10, parm11, parm12, parm13, parm14, parm15, parm16;
vector v_forward, v_up, v_right;   // set by makevectors
float trace_allsolid;
float trace_startsolid;
float trace_fraction;
vector trace_endpos;
vector trace_plane_normal;
float trace_plane_dist;
entity trace_ent;
float trace_inopen;
float trace_inwater;
entity msg_entity;     // the client a MSG_ONE message goes to

void() main;
void() StartFrame;
void() PlayerPreThink;
void() PlayerPostThink;
void() ClientKill;
void() ClientConnect;
void() PutClientInServer;
void() ClientDisconnect;
void() SetNewParms;
void() SetChangeParms;

void end_sys_globals;

// --- the engine-side fields, in this order ---

.float modelindex;
.vector absmin, absmax;
.float ltime;           // a mover's own clock
.float lastruntime;
.float movetype;
.float solid;
.vector origin;
.vector oldorigin;
.vector velocity;
.vector angles;
.vector avelocity;
.string classname;
.string model;
.float frame;
.float skin;
.float effects;
.vector mins, maxs;
.vector size;
.void() touch;
.void() use;
.void() think;
.void() blocked;
.float nextthink;
.entity groundentity;
.float health;
.float frags;
.float weapon;          // the weapon held: one of the W_ bits
.string weaponmodel;
.float weaponframe;
.float currentammo;     // the held weapon's ammunition, for the status bar
.float ammo_shells, ammo_nails, ammo_rockets, ammo_cells;
.float items;           // the W_ and IT_ bits the status bar draws
.float takedamage;
.entity chain;          // findradius's list
.float deadflag;
.vector view_ofs;       // the eye above the origin
.float button0;         // attack
.float button1;         // use
.float button2;         // jump
.float impulse;
.float fixangle;        // non-zero: the client's view is set to angles
.vector v_angle;        // where the player looks
.string netname;
.entity enemy;
.float flags;
.float colormap;
.float team;
.float max_health;
.float teleport_time;
.float armortype;       // the share of damage the armour takes
.float armorvalue;
.float waterlevel;
.float watertype;
.float ideal_yaw;
.float yaw_speed;
.entity aiment;
.entity goalentity;
.float spawnflags;
.string target;
.string targetname;
.float dmg_take;
.float dmg_save;
.entity dmg_inflictor;
.entity owner;
.vector movedir;
.string message;
.float sounds;
.string noise, noise1, noise2, noise3;

void end_sys_fields;

// --- the optional engine fields the server finds by name ---

.float maxspeed;
.float gravity;

// --- the builtins this mod calls ---

void(vector ang) makevectors = #1;
void(entity e, vector o) setorigin = #2;
void(entity e, string m) setmodel = #3;
void(entity e, vector min, vector max) setsize = #4;
float() random = #7;
void(entity e, float chan, string samp, float vol, float atten) sound = #8;
vector(vector v) normalize = #9;
void(string s, ...) error = #10;
void(string s, ...) objerror = #11;
float(vector v) vlen = #12;
float(vector v) vectoyaw = #13;
entity() spawn = #14;
void(entity e) remove = #15;
void(vector v1, vector v2, float nomonsters, entity ignore) traceline = #16;
entity(entity start, .string fld, string match) find = #18;
string(string s) precache_sound = #19;
string(string s) precache_model = #20;
void(entity client, string s) stuffcmd = #21;
entity(vector org, float rad) findradius = #22;
void(float level, string s, ...) bprint = #23;
void(entity client, float level, string s, ...) sprint = #24;
void(string s, ...) dprint = #25;
string(float f) ftos = #26;
string(vector v) vtos = #27;
float() droptofloor = #34;
void(float style, string value) lightstyle = #35;
float(float v) rint = #36;
float(float v) floor = #37;
float(float v) ceil = #38;
float(vector v) pointcontents = #41;
float(float f) fabs = #43;
float(string s) cvar = #45;
void(string s) localcmd = #46;
entity(entity e) nextent = #47;
vector(vector v) vectoangles = #51;
void(float to, float f) WriteByte = #52;
void(float to, float f) WriteShort = #54;
void(float to, float f) WriteCoord = #56;
void(float to, string s) WriteString = #58;
void(float to, entity s) WriteEntity = #59;
void(entity e) makestatic = #69;
void(string s) changelevel = #70;
void(string var, string val) cvar_set = #72;
void(entity client, string s, ...) centerprint = #73;
void(vector pos, string samp, float vol, float atten) ambientsound = #74;
void(entity e) setspawnparms = #78;
void(entity killer, entity killed) logfrag = #79;
string(entity e, string key) infokey = #80;
float(string s) stof = #81;
void(vector where, float set) multicast = #82;

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
float MSG_MULTICAST = 4;
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
.float light_lev;         // a light's brightness (the map's "light" key)
.float style;             // a light's style

float STATE_TOP = 0;
float STATE_BOTTOM = 1;
float STATE_UP = 2;
float STATE_DOWN = 3;

// --- the mod's own globals ---

float deathmatch;         // the server's setting, read each frame
float fraglimit;
float timelimit;
float match_over;
float intermission_time;  // when the match ended (0: still on)
entity last_spawn;        // the spawn point used last
entity activator;         // the entity that set off the targets being fired

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
