// THE RULES' SHARED DECLARATIONS, included inside a variant's namespace after its enums and its
// `A` (arena_decl.hpp, campaign_decl.hpp): every rule both variants hold by name, the rules one
// common file calls in another, and the GLUE each variant writes its own way (printing, effects,
// sides, the inventory, spawn points, teleporting), as its QC glue did. Includes nothing.
//
// ge:summary The rules' shared declarations, included in each variant's namespace (internal)
// ge:keywords rules-common glue declaration native-arena native-campaign
// ge:skill progs shooter
// ge:test tests/shooters_edge_arena_rules_test.cpp

// THE RULES with a field's or the server's name.
#define SE_X(name, ...) void name(A& q);
SE_COMMON_FUNCTIONS(SE_X)
#undef SE_X

// THE RULES one file calls in another.
// common_util
void set_movedir(A& q);
void calc_move(A& q, Vec3 dest, float spd, int32_t done);
void use_targets(A& q);
void effect_head(A& q, float kind);
void write_vec(A& q, Vec3 v);
void effect_explosion(A& q, Vec3 org);
void effect_beam(A& q, Vec3 from, Vec3 to);
float crandom(A& q);
float min_of(float a, float b);
float number_of(A& q, int32_t s);
// common_combat
void score_death(A& q, int32_t targ, int32_t attacker);
void damage(A& q, int32_t targ, int32_t inflictor, int32_t attacker, float points);
void telefrag(A& q, int32_t who);
void radius_damage(A& q, int32_t inflictor, int32_t attacker, float points, int32_t ignore,
                   int32_t how);
// common_weapons
int32_t weapon_word(A& q, float w, float i);
float weapon_number(A& q, float w, float i);
float weapon_rank(float w);
float ammo_for(A& q, float w);
void weapon_show(A& q);
void weapon_set(A& q, float w);
void weapon_select_best(A& q);
void weapon_impulse(A& q);
void weapon_frame(A& q);
void weapon_precache(A& q);
// common_items
void backpack_drop(A& q);
void power_frame(A& q);
void item_precache(A& q);
// common_dmm4
void dmm4_give(A& q);
bool dmm4_shielded(A& q, int32_t p);
void dmm4_frame(A& q);
void dmm4_soaked(A& q, int32_t targ);
void dmm4_octa_taken(A& q);
void dmm4_octa_off(A& q);
void dmm4_pack_take(A& q);
void dmm4_precache(A& q);
// common_player
void player_pain(A& q);
void player_die(A& q);
void jump_sound(A& q);
void swim_sound(A& q);
void player_precache(A& q);

// THE HELPERS the campaign's own rules call too.
void brush_start(A& q);
void trigger_start(A& q);
void trigger_fire(A& q);
void fire_pellets_far(A& q, float pellets, float points, float spread, float reach);
void fire_pellets(A& q, float pellets, float points, float spread);
int32_t launch(A& q, int32_t mdl, float mtype, float spd, float life);

// THE GLUE, each variant's own.
void say_all(A& q, float level, std::initializer_list<int32_t> text);
void say_to(A& q, int32_t to, float level, std::initializer_list<int32_t> text);
int32_t name_of(A& q, int32_t class_name, int32_t fallback);
void effect_send(A& q, Vec3 org, float set);
void effect_impact(A& q, Vec3 org, float count, float bleeds);
bool team_shoves(A& q, int32_t a, int32_t b);
bool same_team(A& q, int32_t a, int32_t b);
int32_t game_words(A& q, int32_t key, int32_t a, int32_t b, int32_t c, int32_t d);
bool pellets_as_one_blow();
bool game_quiet(A& q);
int32_t death_words(A& q, int32_t targ, int32_t attacker);
void drop_on_death(A& q);
bool items_come_back(A& q);
void player_jump(A& q);
int32_t spawn_point(A& q);
void give_inventory(A& q);
void player_respawn(A& q);
bool teleport_takes(A& q, int32_t who);
void teleport_arrived(A& q, int32_t who);
void note_frag(A& q, int32_t killer, int32_t killed);
void missile_now(A& q, int32_t m);
