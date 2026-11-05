// THE ROSTER, CONTINUED: the kinds that fly or swim (porting/shooters-edge/campaign-rules.md
// § The roster): the flyer and the eel. They start where they are placed (ai.qc's
// `flymonster_start`, `swimmonster_start`); the step move keeps a flyer level with its enemy and
// a swimmer in its water.

// --- the flyer (monster_wizard): 80 health, spit fired two at a time ---

// Two spits at 600, 9 each, a step apart.
void() wizard_spit = {
  local entity m;
  self.think = wizard_spit;
  self.nextthink = time + 0.1;
  anim_tick();
  face_enemy();
  if (self.anim_step == 2 || self.anim_step == 3) {
    sound(self, CHAN_WEAPON, "wizard/wattack.wav", 1, ATTN_NORM);
    m = monster_missile("progs/w_spike.mdl", 600, 9, 0);
    m.deathtype = "spat on";
  }
  if (self.anim_step >= 5) monster_run();
};

void() wizard_missile = {
  anim_begin(self.fr_missile);
  wizard_spit();
};

void() wizard_flinch = {
  self.think = wizard_flinch;
  self.nextthink = time + 0.1;
  anim_tick();
  if (self.anim_step >= 2) monster_run();
};

void() wizard_pain = {
  if (self.pain_finished < time + 1) self.pain_finished = time + 1;
  sound(self, CHAN_VOICE, "wizard/wpain.wav", 1, ATTN_NORM);
  anim_begin(self.fr_pain);
  wizard_flinch();
};

// Dead, it falls.
void() wizard_die = {
  monster_die();
  self.flags = self.flags - (self.flags & FL_FLY);
  self.movetype = MOVETYPE_TOSS;
};

void() monster_wizard = {
  if (no_monsters_here()) return;
  precache_model("progs/wizard.mdl");
  precache_model("progs/h_wizard.mdl");
  precache_model("progs/w_spike.mdl");
  precache_sound("wizard/hit.wav");
  precache_sound("wizard/wattack.wav");
  precache_sound("wizard/wdeath.wav");
  precache_sound("wizard/widle1.wav");
  precache_sound("wizard/widle2.wav");
  precache_sound("wizard/wpain.wav");
  precache_sound("wizard/wsight.wav");
  self.noise = "wizard/wsight.wav";
  self.noise1 = "wizard/wdeath.wav";
  self.head_model = "progs/h_wizard.mdl";
  self.gib_health = -40;
  self.walk_step = 8;
  self.run_step = 16;
  self.th_missile = wizard_missile;
  self.th_pain = wizard_pain;
  self.th_die = wizard_die;
  self.fr_stand = '0 14 1';
  self.fr_walk = '0 14 1';
  self.fr_run = '15 28 1';
  self.fr_missile = '29 41 1';
  self.fr_pain = '42 45 1';
  self.fr_death = '46 53 1';
  flymonster_start("progs/wizard.mdl", 80, '-16 -16 -24', '16 16 40');
};

// --- the eel (monster_fish): 25 health, a bite ---

void() fish_bite_step = {
  self.think = fish_bite_step;
  self.nextthink = time + 0.1;
  anim_tick();
  ai_charge(10);
  if (self.anim_step == 3) {
    sound(self, CHAN_VOICE, "fish/bite.wav", 1, ATTN_NORM);
    melee_blow_n(2, 3);
  }
  if (self.anim_step >= 5) monster_run();
};

void() fish_melee = {
  anim_begin(self.fr_melee);
  fish_bite_step();
};

void() fish_pain = {
  if (self.pain_finished < time + 1) self.pain_finished = time + 1;
  anim_begin(self.fr_pain);
  wizard_flinch();
};

void() monster_fish = {
  if (no_monsters_here()) return;
  precache_model("progs/fish.mdl");
  precache_sound("fish/bite.wav");
  precache_sound("fish/death.wav");
  precache_sound("fish/idle.wav");
  self.noise = "";
  self.noise1 = "fish/death.wav";
  self.head_model = "progs/gib2.mdl";
  self.gib_health = -35;
  self.walk_step = 8;
  self.run_step = 12;
  self.th_melee = fish_melee;
  self.th_pain = fish_pain;
  self.th_die = monster_die;
  self.fr_stand = '39 56 1';
  self.fr_walk = '39 56 1';
  self.fr_run = '39 56 1';
  self.fr_melee = '0 17 1';
  self.fr_pain = '57 65 1';
  self.fr_death = '18 38 1';
  swimmonster_start("progs/fish.mdl", 25, '-16 -16 -24', '16 16 24');
};
