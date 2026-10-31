// THE CAMPAIGN VARIANT'S ENGINE SIDE: the single-player program form's engine-side globals and
// fields (their order is the layout the engine reads by offset, so it never changes), the optional
// fields the server finds by name, and the builtins the mod calls by number. Against the arena's:
// no `newmis`; `deathmatch`, `coop` and `teamplay` after `mapname`; no `lastruntime`;
// `punchangle` after `avelocity` and `idealpitch` after `v_angle`; `bprint` and `sprint` without a
// level; `aim` and `particle`; no logfrag, infokey, stof or multicast.

// --- the engine-side globals (28..89), in this order ---

entity self;
entity other;
entity world;
float time;
float frametime;
float force_retouch;    // non-zero: every entity re-touches what it stands in
string mapname;
float deathmatch;
float coop;
float teamplay;
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
.float movetype;
.float solid;
.vector origin;
.vector oldorigin;
.vector velocity;
.vector angles;
.vector avelocity;
.vector punchangle;     // the view's kick, decaying
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
.float idealpitch;      // the pitch the view leans to on a slope
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
void(string s, ...) bprint = #23;
void(entity client, string s, ...) sprint = #24;
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
vector(entity e, float speed) aim = #44;
float(string s) cvar = #45;
void(string s) localcmd = #46;
entity(entity e) nextent = #47;
void(vector o, vector d, float colour, float count) particle = #48;
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

// --- the variant's own constants ---

float MSG_EFFECT = 0;     // where an effect's bytes go: the unreliable broadcast
