---
type: Added
title: Shooter bots see, hear and aim by skill
---

`plugins/shooter` gains the bot's senses (`senses.hpp`: what a client is sent, sight in the view's cone by the hull trace, the sounds heard, contacts on `ai/memory`), aim on `ai/hands` with four skill profiles (`aim.hpp`) and the bot that runs its pilot's route while it aims (`bot.hpp`, a server slot's command source); the `se-senses` lab pictures the seer per skill on the arena.
