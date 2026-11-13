---
type: Added
---
Shooter's Edge's first-person view moves like the classic one: the eyes bob with the stride (held in the air), roll a little when strafing, kick toward a blow and fade, ease up steps, and lie on their side when dead, tuned with `cl_bob`, `cl_bobcycle`, `cl_bobup`, `cl_rollangle`, `cl_rollspeed`, `v_kicktime`, `v_kickpitch` and `v_kickroll` (set them to 0 for a still view). On the sequel's rules the server works the view out and sends it to every client: the stride's bob and nod, the run's tilt, the kicks of blows, hard landings and shots, and the dead view looking at the killer, tuned with the server's `run_pitch`, `run_roll`, `bob_up`, `bob_pitch` and `bob_roll`. Lab `se-view` checks both.
