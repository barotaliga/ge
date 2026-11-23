# Answers: web (HTTP/HTTPS) download addresses in QuakeWorld and Quake II engines

Read by a clean-room reader. Sources, each the default branch cloned shallowly on 2026-11-23:

- FTE QuakeWorld: github.com/fte-team/fteqw, commit f09ac7c (2026-11-02)
- ezQuake: github.com/QW-Group/ezquake-source, commit f3fad8b (2026-11-22)
- MVDSV: github.com/QW-Group/mvdsv, commit 70f1135 (2026-11-18)
- Q2PRO: github.com/skullernet/q2pro, commit 601a8df (2025-12-11). The q2pro/q2pro address in the brief did not clone; skullernet/q2pro is the project's own repository.

Tags: [read] = stated by the source or its shipped documentation; [derived] = follows from reading several places together; nothing here was measured by running a program.

## 1. FTE server: the advertised web address

- The setting is `sv_dlURL`. It also answers to the alias `sv_curl_defaulturl`. Default: empty. [read]
- It is flagged as a server-info setting, so its value appears in the QuakeWorld server info under the key `sv_dlURL`: exact spelling, mixed case, NO leading `*`. [read]
- It is saved to the config file on native builds; it is not saved on the browser build. [read]
- The setting's help text says it gives clients an external URL "from which they can obtain pk3s/packages" instead of downloading over the game's UDP channel. [read]
- For Quake II clients the same value is also sent in the connect answer as ` dlserver=<value>` (appended to `client_connect`), so a Quake II client learns it the Quake II way. [read]
- The help text of `allow_download` (default 1; also answers to `sv_allowDownload`) says that setting it to 0 blocks every in-game download and suggests `sv_dlURL` so that clients can still get content. [read]
- Layout as the CLIENT builds it (the server only advertises; see question 2 for the details): for a package the URL is the address, then a `/` if the address does not already end in one, then `<gamedir>/<package file>`. For any other file it is the address, then a `/` if needed, then `<the client's current gamedir>/<path>`, for example `.../qw/maps/dm6.bsp`. There are no placeholders such as %s. [read]
- So it is used for maps, sounds and other single files too, not only for packages, though the help text says packages. [derived]
- Server-side redirects. When a client asks the server (with the `dlsize` query used by clients that support FTE's chunked downloads) for a package named `package/<gamedir>/<file>`, the server first looks for a text file `curl_urls.txt` in that game directory. Each line holds a wildcard pattern and a URL prefix. The first pattern that matches the package's name (without the gamedir) wins, and the server answers "redirect to <prefix><package name without gamedir>". The source notes that this file is the admin's own tool. [read]
- If no line matches and `sv_dlURL` is set, there is a fallback meant to redirect to an address under `sv_dlURL`. As read, though, it appends the package path to the setting's own stored value, not to a fresh copy, and the output buffer is never filled with the address first. So this fallback's result is not a clean `<sv_dlURL><gamedir>/<package>`. Treat the server-side fallback as defective. Clients still build the URL correctly themselves from the server-info key. [derived]
- These HTTP redirects are given only in answer to the `dlsize` query, never in answer to a plain `download` command: a plain download of a package's contents that would need a redirect is refused with "Package contents not available individually". [read]
- FTE also registers `download_map_url` (the MVDSV/ezQuake name). It does nothing: its help says it is redundant. [read]

## 2. FTE client: what it fetches over HTTP

Where the address comes from, in this order of priority [read]:
1. the server-info key `sv_dlURL`.
2. otherwise the `dlserver=` value from a Quake II connect answer.
3. otherwise the player's own local `sv_dlURL` setting.

Only addresses that start with `http://` or `https://` are used. Any other address is ignored. [read]

If none of the three gives an address, and the file is a map (`maps/<name>.bsp`, stored under its own name), the client uses `cl_download_mapsrc` (archived; default empty). Its URL is the setting's value followed by `<name>.bsp`, without the `maps/` part. If the value has no scheme, the client assumes https and prints "cl_download_mapsrc: Scheme not specified, assuming https.". When the game is Quake (the quake and qw schemes, and the re-release), FTE's built-in overrides set `cl_download_mapsrc` to `https://maps.quakeworld.nu/all/`. [read]

Which files may go to the web address [read]:
- the map (QuakeWorld's world model, model index 1).
- models in the extended model lists, and models announced during the game.
- sounds (under `sound/`).
- Quake II pictures (`pics/*.pcx`), and Quake II wall textures (`textures/*.wal`, only when neither the .wal nor a .tga is present).
- packages (pak/pk3) that the server lists as in use.

Which files always use the in-game channel [read]:
- other QuakeWorld model-list models.
- QuakeWorld skins (`skins/*.pcx`).
- Quake II player models, skins and sounds.
- MD2 skins, view-weapon models, the colour map, wads.
- `csprogs.dat`.

How the transfers run:
- Only one download runs at a time, web or in-game. [read]
- A web download goes to its local name. Packages go into the package cache under the root folder. [read]
- The console prints "Downloading <url> to <local>...". [read]
- Fallback: if the web download fails, a 404 counts as "file missing on server". Any other failure (other HTTP codes, TLS errors) counts as a server configuration fault. In both cases the file is queued again over the in-game channel. [read]
- Size limit: a web download stops when the declared or received size passes 2^31 bytes (2 GiB). The console prints "Request exceeds size limit - <url> (<got> vs <limit>)". [read]
- Server redirects: the setting `cl_download_redirection` (default 2, archived, cannot be set by the server) controls them. At 2, only redirects to `package/...` files ending in .pak, .pk3 or .pk4, or to `demos/*.mvd` or `*.gz`, are followed. Anything else, including a raw http URL, is refused with "Ignoring non-package download redirection to ...". At 1 any redirect is followed, except the dangerous names (game code, native libraries, paths with `..`, `:`, `\` or `//`). At 0 none are followed. [read]
- So the server's curl_urls.txt redirect to an http URL is followed only by a player who has set `cl_download_redirection` to 1. [derived]
- Other client settings: `cl_downloads` (default 1; also answers to `cl_allowDownload`) blocks all automatic downloads. `cl_download_packages` (default 1) controls whether in-use packages are fetched. `cl_download_wait` (default 1; old name `requiredownloads`) makes the client wait for downloads before it joins. No download starts while the client is recording a demo. [read]

Certificates [read]:
- https addresses are verified through the platform's TLS library (GnuTLS, or SSPI on Windows), against the host name.
- Errors are printed, for example "Certificate authority is not recognised", "Certificate has expired or was revoked or not yet valid", or "Certificate signature failure".
- `tls_ignorecertificateerrors` (default 0) can be set only from the command line, is never saved, and the server cannot change it. Its help says it should never be 1.
- There is also a store of known certificates per host that the player can trust. Exactly when it is consulted is not settled by my reading.

Speed: no setting limits the speed of a web download. The client only measures the rate, once a second, for display. [read]

## 3. ezQuake client

- No. The ezQuake client has no code that downloads game files over HTTP from an address a server advertises. It has no server-info key, no URL layout and no client setting for that. Its HTTP use is for other things: the version check, the server browser's sources, the QTV list and a central web service it reports to. [read]
- Game files reach ezQuake only over the in-game channel. That includes FTE's chunked downloads, and QTV downloads when the QTV stream offers them. [read]
- The ezQuake source tree also contains a server. That server has `download_map_url`, the same as MVDSV's (see question 4). [read]

## 4. MVDSV server

- Setting: `download_map_url`. Default: empty. It is an ordinary setting, not in the server info: the client is never told it automatically. [read]
- Effect: when a client starts an in-game download of a file under `maps/`, the server prints to that client "Map maps/<name> is <N>KB (<M>MB)". If `download_map_url` is set it then prints "Download this map faster:" and, on the next line, the address followed directly by the map's file name without `maps/` (for example `<download_map_url>dm6.bsp`). No `/` is added, so the admin ends the address with one. It is plain text for the player to open by hand. No client acts on it. [read]
- Other files get "File <name> is <N>KB (<M>MB)". [read]
- Download speed, which the project owner asked about [read]:
  - In-game downloads are paced per client by the userinfo key `drate`. Ordinary play is paced by `rate`. If `drate` is absent, 99999 is used.
  - The server clamps the result with `sv_maxdownloadrate` (default 0, meaning no separate cap). When that is 0, `sv_maxrate` applies (default 0, meaning no cap).
  - A rate of 0 becomes 2500. The result is held to at least 500 and at most 300000 (100000 times the maximum packet duplication of 3).
  - Packet duplication is switched off while a download runs.
  - These are bytes per second of channel pacing. [derived]
  - There is no server-wide total cap across all clients. [derived: no such setting found]

## 5. Q2PRO client: resume, speed, size, checksum

- Resume: no. Each file is written fresh to `<gamedir>/<path>.tmp`, opened for overwrite. No Range request is sent. The source states that HTTP resume was skipped on purpose because servers do not always support it. [read]
- On success the .tmp file is renamed to the real name. On failure it is deleted, so nothing is kept between attempts. [read]
- Speed: not capped. No receive-speed limit is set. The only related setting is `cl_http_max_connections` (default 2, clamped to 1 to 4): how many connections may be open to the download host at once. [read]
- After each file the console prints "[HTTP] <path> [<size>, <speed>/sec] [<n> remaining files]". [read]
- Size: files written to disk have no size cap. File lists, which are held in memory, are capped at 1 MiB. [read]
- Map checksum: there is no check specific to the download. But every map load compares the map's checksum with the server's map-checksum config string. On a mismatch the client drops with "Local map version differs from server:", followed by the local checksum, a not-equal sign and the server's value. During demo playback it only warns. So a wrong downloaded map is caught when it loads, not when it arrives. [read]
- Other facts:
  - The URL is the server's address directly followed by `<gamedir>/<path>`. The gamedir is `baseq2` when no mod is set. No `/` is added. Characters other than letters, digits and `/-_.~` are percent-encoded. [read]
  - Requests carry a User-Agent (the client's version string), a Referer of the form `quake2://<server address>`, and accept compressed encodings. They follow up to 5 redirects. Only the http and https protocols are allowed. [read]
  - `cl_http_proxy` (default empty) sets a proxy. `cl_http_filelists` is default 1. `cl_http_downloads` (default 1) turns HTTP downloads off. An `allow_download` of -1 disables them too. They are never used against the local server. [read]
  - `cl_http_default_url` (default empty) is used when the server gives no address. [read]
- Failure handling [read]:
  - A 404 is not fatal: the file is marked done and not retried over HTTP. The exception is the default repository (`cl_http_default_url`), or a file list, where a 404 is fatal.
  - Any other HTTP status is fatal. So is a failure to resolve or connect, or a failed certificate check.
  - A fatal error turns HTTP off for this server, and the remaining files fall back to the in-game UDP download.
  - When a pak arrives, the client's file system restarts.
- Addresses with a scheme other than http or https are ignored, with "[HTTP] Ignoring download server URL with non-HTTP schema.". Over-long addresses are ignored with "[HTTP] Ignoring oversize download server URL.". [read]

## 6. Q2PRO: https certificates

- Verified by default: the peer certificate is checked against the trusted authorities, and the host name must match. [read]
- `cl_http_insecure` (default 0) set to 1 turns off both checks, for downloads and for the client's other blocking HTTP fetches. The documentation says it disables "checking of server certificate when using HTTPS". [read]
- A failed verification is treated as a fatal HTTP error, so the client falls back to UDP (see question 5). [read]

## 7. Q2PRO server: beyond `dlserver=`

- The setting is `sv_downloadserver` (default empty). The documentation says it must start with `http://` or `https://` and should normally end with a slash. [read]
- When it is set, the connect answer has the form `client_connect` [` nc=0|1`] [anticheat part] ` dlserver=<url>` ` map=<mapname>`. The map name is always sent. [read]
- Status flag: when a client starts its first HTTP download from the server's own address (not from the default repository), and file lists are on, it sends the in-game command `download http`. The server takes that command only as a marker; it does not download anything. It marks the client as downloading over HTTP until the client spawns. While the mark is set [read]:
  - `status` shows the client's ping column as `HTTP`, instead of `CNCT`, `PRIM` or `DNLD`.
  - `status d` lists the client with the name `<HTTP download>` and 0%.
- No HTTP header is asked of the web server. The client sends the Referer `quake2://<server address>` described in question 5, which a web server may log or check. The game server neither sets nor reads it. [read]
- The server does not check the HTTP host or its files. [derived]
