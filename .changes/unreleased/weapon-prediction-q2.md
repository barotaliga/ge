---
type: Added
title: Your own weapon predicted on a joined Quake II server
---

On any Quake II server, a stock one included, your shot's sound plays, your gun's firing frames start and the ammunition on your bar counts down the moment you press the button, instead of after a round trip and the wait for the server's next frame. The server's next frame corrects any guess. It uses the same settings as on QuakeWorld: `cl_nopred_weapon 1` turns it off, `cl_predict_weaponsound` picks which weapons' sounds are played ahead (above 1, 2 to the power of a weapon's key leaves that weapon to the server), and `cl_predict_buffer` sets how many moves behind the newest the weapon is shown. Weapon changes, the hand grenade and a mod's own weapons still show when the server says so.
