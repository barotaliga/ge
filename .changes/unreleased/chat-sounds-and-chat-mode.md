---
type: Added
title: Chat sounds, chat filters, time stamps and the first-word chat mode
---

On a server you joined, each chat line now plays a sound: `s_chat_custom` picks which lines (every line, team lines only, or none), and `s_mm1_file`, `s_mm2_file`, `s_spec_file` and `s_otherchat_file` with their volumes set each kind's sound (your own files apply while you watch). `msg_filter` hides lines to everyone or team lines, `con_timestamps` puts the time before each chat line, and `con_highlight` with `con_highlight_mark` marks a line that names you.

`cl_chatmode 2` (the console's `con_chat 2`) runs a typed line whose first word is a command, alias or setting and says any other. In every mode a line starting with `\` runs like one starting with `/`, and a line starting with `//` is only a note.
