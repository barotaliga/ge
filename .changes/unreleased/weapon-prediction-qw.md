---
type: Added
title: Your own weapon predicted on a joined QuakeWorld server
---

On a QuakeWorld server that sends the weapon's state (ours always does), your shot is heard, your weapon fires in your hands and the ammunition on your bar counts down the moment you press the button, instead of a round trip later; a weapon you switch to shows at once. The server's next frame corrects any guess. `cl_nopred_weapon 1` turns it off, `cl_predict_weaponsound` picks which weapons' sounds are played ahead (0 none, 1 all, above 1 the weapons left to the server), `cl_predict_buffer` how many commands behind the newest the weapon is shown, and `cl_pext_weaponprediction 0` stops asking servers for the state. On our own servers the weapon in your hands now fires at all when joined, where before it stayed still.
