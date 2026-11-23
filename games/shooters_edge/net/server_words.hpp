// THE DEDICATED SERVER'S OWN CONSOLE WORDS, per game: the words QuakeWorld's and Quake II's server
// take through `ServerGame::console` (qw_game.cpp's and q2_game.cpp's `console_line`: the
// masters, the admin's lists, the relay, the files, the rules' `map`), declared so the server's
// console registers them (engine/server/server.hpp's GameWord): Tab completes them, `cmdlist` and
// the reference manual list them. A word added to a `console_line` path is added here too, or it
// runs but no list shows it; `tests/shooters_edge_server_words_test.cpp` holds the tables (one
// row a word, its help in a player's words, no word the console's own).
//
// ge:summary The dedicated server's own console words per game, for Tab and the manual
// ge:keywords server-words game-words console-words dedicated-server manual tab-completion
// ge:keywords kick status map changelevel setmaster heartbeat addip mvdrecord
// ge:skill console net
// ge:test tests/shooters_edge_server_words_test.cpp
#pragma once

#include "server/server.hpp"

namespace shooters_edge::net {

// QuakeWorld's: the masters, then the rules' own (server/commands.cpp's console lines).
inline constexpr ge::server::GameWord kQwServerWords[] = {
    {"map", "map <name>: start a new match on that map"},
    {"changelevel", "changelevel <name>: go on to that map, the players kept"},
    {"restart", "restart: play the map again from its start"},
    {"say", "say <text>: a chat line to every player, from the console"},
    {"localinfo", "localinfo <key> <value>: a value the server's rules read and players never see"},
    {"floodprot", "floodprot <lines> <seconds> <wait>: how fast a player may chat"},
    {"setmaster", "setmaster <address>...: the master servers the server is listed on"},
    {"heartbeat", "heartbeat: tell the master servers about the server now"},
};

// Quake II's: the relay, the admin's words and lists, the files, saves, the masters, the map's
// words, then the rules' own. Our spelling first; `also` the game's own spelling of it.
inline constexpr ge::server::GameWord kQ2ServerWords[] = {
    // The rules.
    {"map", "map <name>: start a new game on that map"},
    {"gamemap", "gamemap <name>: go on to that map, the game kept (a unit's next level)"},
    {"changelevel", "changelevel <name>: go on to that map, the players kept"},
    {"restart", "restart: play the map again from its start"},
    {"say", "say <text>: a chat line to every player, from the console"},
    {"sv", "sv <word>: the game's own server words"},
    {"gamemode", "gamemode <dm|coop|sp|?>: the mode the next map is played in"},
    {"listmaps", "listmaps: the maps the server can load"},
    {"dumpents", "dumpents: write the map's entities, as loaded, to entdumps/<map>.ent"},
    {"bsplist", "bsplist [all]: the map's size, and with a word its counts"},
    {"sv_running", "sv_running: whether a map is running (read only)"},
    {"sv_paused", "sv_paused: whether the game is paused (read only)"},
    // Players.
    {"status", "status: every player on the server: id, score, ping, name and address"},
    {"kick", "kick <userid>: remove a player from the server"},
    {"kickban", "kickban <userid>: remove a player and ban their address"},
    {"dumpuser", "dumpuser <userid>: a player's user info"},
    {"stuff", "stuff <userid> <text>: run a console line in one player's game"},
    {"stuffcvar", "stuffcvar <userid> <setting>...: ask a player's game for settings' values"},
    {"stuffall", "stuffall <text>: run a console line in every player's game"},
    {"printall", "printall <text>: print a line on every player's screen"},
    {"pickclient", "pickclient <address:port>: ask a player behind a firewall to connect"},
    {"killserver", "killserver: end the map and drop every player"},
    // Bans and lists.
    {"addip", "addip <address> [ban|safe] [+seconds]: ban an address, or keep it safe from bans"},
    {"removeip", "removeip <address>: lift an address's ban"},
    {"listip", "listip: the banned addresses"},
    {"writeip", "writeip: save the banned addresses to the ban file"},
    {"blackhole_addip", "blackhole_addip <address> [+seconds]: ignore everything from an address",
     "addblackhole"},
    {"blackhole_removeip", "blackhole_removeip <address>: hear an address again", "delblackhole"},
    {"blackhole_listip", "blackhole_listip: the ignored addresses", "listblackholes"},
    {"cvarban_add", "cvarban_add <setting> <match> [action]: refuse players whose setting matches",
     "addcvarban"},
    {"cvarban_remove", "cvarban_remove <setting>: lift a setting ban", "delcvarban"},
    {"cvarban_list", "cvarban_list: the setting bans", "listcvarbans"},
    {"cvarban_check", "cvarban_check: check every player against the setting bans",
     "checkcvarbans"},
    {"userinfoban_add", "userinfoban_add <key> <match> [action]: refuse players whose info matches",
     "adduserinfoban"},
    {"userinfoban_remove", "userinfoban_remove <key>: lift a user info ban", "deluserinfoban"},
    {"userinfoban_list", "userinfoban_list: the user info bans", "listuserinfobans"},
    {"filtercmd_add", "filtercmd_add <word> [action]: what happens when a player sends that word",
     "addfiltercmd"},
    {"filtercmd_remove", "filtercmd_remove <word>: stop filtering a word", "delfiltercmd"},
    {"filtercmd_list", "filtercmd_list: the filtered words", "listfiltercmds"},
    {"lrcon_add", "lrcon_add <word>: let the limited remote console run a word", "addlrconcmd"},
    {"lrcon_del", "lrcon_del <word>: take a word from the limited remote console", "dellrconcmd"},
    {"lrcon_list", "lrcon_list: the words the limited remote console may run", "listlrconcmds"},
    {"addstuffcmd", "addstuffcmd <connect|begin> <text>: a line run in each player's game then"},
    {"delstuffcmd", "delstuffcmd <connect|begin> <number|line|all>: take such a line away"},
    {"liststuffcmds", "liststuffcmds <connect|begin>: the lines run in each player's game"},
    // Recording and the relay.
    {"mvdrecord", "mvdrecord [-z] <file>: record every player's view of the match"},
    {"mvdstop", "mvdstop: stop the recording"},
    {"mvdstuff", "mvdstuff <text>: a console line into the recording"},
    {"addgtvhost", "addgtvhost <address>: let a relay connect without the password"},
    {"delgtvhost", "delgtvhost <address>: take a relay host away"},
    {"listgtvhosts", "listgtvhosts: the relays let in"},
    {"addgtvban", "addgtvban <address>: refuse a relay"},
    {"delgtvban", "delgtvban <address>: lift a relay ban"},
    {"listgtvbans", "listgtvbans: the refused relays"},
    {"mvdconnect", "mvdconnect <address[:port]>: relay another server's match to viewers"},
    {"mvdplay", "mvdplay <file>...: play recordings to viewers as a channel"},
    {"mvdspawn", "mvdspawn: start the relay's waiting room"},
    {"mvdisconnect", "mvdisconnect [connection]: stop relaying a server"},
    {"mvdkill", "mvdkill [channel]: close a channel"},
    {"mvdchannels", "mvdchannels: the relay's channels and their viewers"},
    {"mvdservers", "mvdservers: the servers the relay is connected to"},
    {"mvdcontrol",
     "mvdcontrol [-l count] [-n name] [channel]: a channel's name and how often it loops"},
    {"mvdpause", "mvdpause [channel]: pause or resume a channel"},
    {"mvdskip", "mvdskip [channel] [count]: skip to a later map in a played recording"},
    {"mvdseek", "mvdseek [+-]<time|percent> [channel]: seek in a played recording"},
    // The game's files and saves.
    {"path", "path: the folders and packs the game's files come from, in order"},
    {"fs_restart", "fs_restart: open the game's files again"},
    {"dir", "dir <folder> [extension]: the files in a folder"},
    {"fdir", "fdir <filter>: every file whose path matches the filter"},
    {"whereis", "whereis <path> [all]: which pack or folder holds a file"},
    {"link", "link <name> <target>: read a path from another (none: the list)"},
    {"unlink", "unlink <name>: take a link away"},
    {"softlink", "softlink <name> <target>: a link used only when the file is missing"},
    {"softunlink", "softunlink <name>: take a soft link away"},
    {"save", "save <slot> [comment]: save the game"},
    {"load", "load <slot>: load a saved game"},
    {"autosave", "autosave: save the game to the autosave slot"},
    // The masters.
    {"setmaster", "setmaster <address>...: the master servers the server is listed on"},
    {"heartbeat", "heartbeat: tell the master servers about the server now"},
    {"listmasters", "listmasters: the master servers and when each last heard from the server"},
};

}  // namespace shooters_edge::net
