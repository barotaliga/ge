---
type: Added
---
Fall Line's window has the drop-down console: the key under the tilde opens it over the run, drawn by the GPU's 2D pass after post (the frame's new `ui` row); typed text, the edit keys and their repeats reach its input line through `ui::feed_console`, which also runs the binds by HID key (`console::usage_key`), and the run reads no keys while it is down. The `world` lab photographs it (`--console`).
