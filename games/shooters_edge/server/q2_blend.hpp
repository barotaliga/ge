#pragma once
// QUAKE II'S SCREEN TINT: the colour laid over a player's view, worked out once a frame on the
// server and sent with his view (the player state's blend), so the window, a recording and a stock
// client tint alike. From the reader's facts (porting/shooters-edge/q2-sp-world.md § View):
//
// - each tint (r, g, b, a) with a above 0 is laid over what is there: the new alpha is
//   A + (1 - A) a, and the colour keeps A / A' of the old;
// - in order: what is at the eye (solid or lava 1 .3 0 .6, else slime 0 .1 .05 .6, else water .5
//   .3 .2 .4); one power, the first held (quad 0 0 1 .08, invulnerability 1 1 0 .08, suit 0 1 0
//   .08, rebreather .4 1 .4 .04), shown past 3 s left or on frames whose count left has bit 4;
//   the damage flash; the pickup flash (.85 .7 .3);
// - the damage flash: a frame's blows (blood, armour, power armour; at least 10) add a hundredth
//   each to its alpha, held to 0.2..0.6, coloured by each part's share (red, white, green), and
//   it fades 0.06 a frame; a pickup sets 0.25, fading 0.1 a frame;
// - in the intermission there is none.
//
// THE DAMAGE FLASH APART: beside the whole tint, the tint without the damage flash
// (`blend_apart`) and the flash alone (`damage_blend`), for a window that draws the flash as a
// frame round the view's edge (gl_damageblend_frac); a stock client gets the whole tint.
//
// ge:summary Quake II's screen tint: liquids at the eye, the powers, the damage and pickup flashes
// ge:keywords q2 blend screen-tint damage-flash pickup-flash bonus-flash power-tint underwater
// ge:keywords damage_blend blend_apart
// ge:skill shooter
// ge:test tests/shooters_edge_q2_fight_test.cpp

namespace shooters_edge::server {

struct Q2Server;

// Player `slot`'s tint this frame into his `blend`, the flashes faded; after his view's frame.
void blend_frame(Q2Server& s, int slot);

}  // namespace shooters_edge::server
