---
type: Added
title: VIPs on the classic server
---

A Shooter's Edge server can name VIPs: `sv_vippassword` (classic `vip_password`) lists words any of which makes a client a VIP, `sv_vipvalues` (`vip_values`) the level each gives, and `vip_addip <address> [level]` (with `vip_removeip`, `vip_listip` and `vip_writeip`, saved to `vip_ip.cfg`) names VIPs by address. A VIP skips the server's passwords, may take the `sv_maxvipspectators` (`maxvip_spectators`) spectator slots, and carries `*VIP <level>` in its user info for the mod.
