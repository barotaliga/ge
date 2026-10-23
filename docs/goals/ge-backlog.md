# The `ge` backlog

What agents working on the engine and its games said `ge` lacked (AGENTS.md § SAY WHAT `ge`
LACKED), one wish a line under its group: the command, what it would answer, and `done:` with
what was built once it lands. Add a wish as a new line under the group it fits; git merges
this file with its union driver (`.gitattributes`), so threads adding lines side by side never
conflict. Milestone M12d in [`../goals.md`](../goals.md) takes these by group.

## Digests and goldens (done)

- `ge digest <lab> [--write]` (one lab's baseline, not the whole smoke run)
- `ge golden diff` (which columns of a golden's rows moved) and `ge golden write`
- `ge digest --diff` (done: every lab's digest against the baseline in one command, nothing written, only the labs that moved printed)
- `ge golden diff` pairing rows by their key when the count changes (done: a row inserted mid-table is one row added)
- `ge digest-diff --base=<ref> --versions --regions` (which generator versions and regions a change moved)

## Before and after across refs

- `ge bench ab --base=<ref>` (or `--ab <binA> <binB>`: two builds interleaved N times with statistics, since a container's timings wander by about 5%) and `ge compare <ref>` (both builds, one table)
- `ge profile --compare` (per zone and per pass, GPU and CPU deltas, outliers flagged)
- `ge digest <lab> --why` (which table row and which columns moved when a digest did)
- `ge lab diff` for a lab's tables and `--stash`
- `ge lab sheet <lab> --grid=2x2 [shots]` (shots on one image) and `--watch`
- `ge profile --pass=<name>` (one pass's median and p95, before and after) and `--cost <shader-term>`
- `ge lab diff <lab>` (built: every shot against the kept run, PSNR, SSIM and the share changed per distance band off the frame's depth, each band held to its floor, a four-panel sheet with the change heatmap and the bands' contours)

## The CI loop

- `ge precheck` (done: the repo's CI gates in both presets) and `ge lint --like-ci` (CI's exact gates and file set)
- `ge lint` (done: clang-tidy's bug-finding checks over what the change since `--base` reaches, the changed .cpp files and every .cpp including a changed header, or the files named, each finding at its line with its check's page; a few files in seconds)
- a teleport detector for the labs (done: `engine/lab/teleport.hpp`, every step of a track that moves farther than its speed allows, reported under every scenario lab's `--trace`)
- `ge test --affected` (done: the tests the change since `--base` reaches through the include graph, a target reached when its objects include a changed file or a header in a reached library's directory; only their targets built, `--list` names them; precheck takes its tests from it)
- `tools/labs-reached.sh` following the same reach (a plugin change reaches the game's labs through the game library including its headers)
- the labs' digest baseline merging without conflicts (done: `merge=union`, a lab's doubled row failed by name)
- `docs/goals.md` merging without conflicts (done: each row's status in `docs/goals/<row>.md`, this backlog one wish a line, union-merged, held by `tools/check-goals.mjs`)
- `ge ci <pr>` (done: each failed job's log as findings, over `gh api`; `--merge` squash-merges a green, clean one at its head, and a green PR GitHub blocks is answered with the required check or review it waits on)
- `ge test --filter` building only the matching tests' targets (done, and `make test T=<regex>` in the engine)
- lint that also covers the files whose compile command changed (`compile_commands.json` against the base)
- precheck running the smoke digest of every lab the change reaches (done in the engine's `make precheck`; `ge precheck` already runs every lab)
- `labs --all --write-digests` keeping the rows of labs the build does not register (done)
- `ge ci run --row=macos -- <cmd> --repeat=N` (a command on one CI row through a dispatched workflow, no temporary workflow step committed: a Metal flake hunt, the needs-mac bench)
- `ge ci <pr> --rebase-digests` (after merging the base, the digests both sides moved rewritten and named)
- `ge lab sweep <id> --set=<const>=a,b,c` (done: one constexpr rewritten to each value, the labs rebuilt and run, every metric side by side, the source put back; next, a scenario row's own numbers, such as a scripted hand's timings, swept the same way, and `--draws=N` over a range `a..b`, a Monte Carlo of one constant with each metric's spread)
- `ge lab sweep <id> --flag=<flag>=a,b,c` (done: one of the lab's own flags at each value, the labs built once, the same table; the hours of a day over a picture lab)
- `ge lab --split=N`
- `ge job run\|status\|log` (a long run in the background)
- `ge merge-main`

## Inspecting a run

- `ge record` (done: a run as an MP4, WebM or GIF and a contact sheet of timestamped stills, its resolution, frame rate, speed, range and encoder quality; and `--gpu`, the game's registered renderer drawing each frame headless in a mode, Fall Line's `lit`, `wireframe`, `depth` and `overlay` from a chase camera, a high chase or the named views; `ge shot --gpu` the same still)
- `ge state --entities=<kind> --at=<t>`
- `ge sim --human --compare=<tag>` (the humanness report beside a kept run's, each metric's move)
- `ge sim find <seed> --when "<predicate>"`
- `ge sim --stalls`
- `ge sim --why` (each run's bot decision timeline beside its input log)
- `ge why --compare=<log>` (two logs' bot decisions side by side where they part)
- every scenario lab's per-step trace as a series file `ge graph` charts (done: `--trace` on the `ride`, `sled`, `heli` and `para` labs writes each scenario's every step as `<id>.trace.tsv` and charts it with its events marked and its flights shaded, `engine/lab/series_charts.hpp` the one helper the `trace` lab shares)
- the same scenarios through `ge why --scenario=<lab>:<id> --step=<n>` once a machine pilot records its decisions (a wing's fold, bite and slack)
- `ge sim --outing=<machine> --seed=<n>` with `--trace=<series>` and `--shot` (one free-ride outing traced or pictured without the whole free lab)
- `ge map <seed>` (a seed's weather, fall, sun and day without a lab)
- `--sky=<kind>` on every picture lab (the map's weather overridden)
- `ge log --step --channel`
- `ge probe`
- `ge lab <id> --sunlit` (a warning when a look's pixels sit in the horizon map's shadow, so a shadow change that moves no pixel is caught)
- `ge lab scene <lab> <scene> --plan` (one scene's moment from above without running the whole lab)
- `ge why --agent=1000..1003 --decision=landing` (the decision trace filtered by agent range and decision name)
- `ge lab table <lab> --where col=value` (one slice of a lab's table)
- `ge lab pick <lab> <shot> --pixel=x,y` (a picture's value at a pixel before the tonemap, the HDR colour and its depth)
- `ge gpu dump <target> --frame=N --decode=<scheme>`
- `ge asset inspect`
- `ge lab table <lab> --where col=value` (one slice of a lab's table, a weapon at one range, read without re-running the lab or opening the whole report)

## Measuring

- `ge profile --wall` (a sampling CPU profiler: wall time by inlined frame, the main thread apart from the workers, where instruction counts hide a slow call)
- `ge profile --callgrind <lab>`
- `ge lab <id> --toggle=<feature>` (a lab's passes timed with the feature on and off, one table)
- `ge issue needs-mac --bench=<invocation>` (the needs-mac issue written with the exact GPU bench command and what to paste back, after trying CI's macOS runner on Metal first, since software-rendered timings say little about real cost)
- `ge mem --where` (the allocating call stack)
- `ge build --flags`
- `ge spans <lab>` (a lab's spans file read back as a table)
- `ge sanitize thread <tests>` (a ThreadSanitizer build of the tests that run threads)

## Hygiene

- `ge build` and `ge lab` warning when the labs binary is older than a library it links
- `ge deps fetch`
- `ge lint --explain`
- the software Vulkan driver's leaks suppressed under `ge leaks`
- GPU labs on no adapter reported as skipped
- `ge add lab --kind=overlay`
- `ge doctor --tools` / `ge env` (which tools, versions and drivers a gate needs, and which are missing)

(`ge shot` is M12b's; `ge anim` is M14b's.)

## Shooter's Edge (M18, docs/shooters-edge.md § The `ge` tooling it needs)

- `ge pak ls/cat/extract/build` (a search path's files, which archive wins one, a fixture pack built from a directory)
- `ge bsp info/ents/shot` (a map's lumps and limits, its entity lump, a CPU picture of leaves, the PVS from a point and the hulls)
- `ge qc build/disasm/run/trace/profile/diff` (our QC compiler; a progs disassembled, run, traced a step at a time, timed by function, diffed against another build)
- `ge server --map --bots --for` (a headless dedicated server with bots, writing an input log for `ge replay`, `ge state` and `ge diverge`)
- `ge demo info/frames/shot/diff` (a client or MVD demo decoded frame by frame, a frame pictured, two demos to their first difference)
- `ge net inspect --protocol=<name>` (pluggable decoders, so a capture of the classic protocol reads as its messages)
- `ge pmove <scenario>` (a movement scenario's trajectory as a table and a chart, against its parity recording)
- `ge console <target> "<command>"` (a console command or cvar run in a headless server or client, its output returned)
- `ge play` for first-person games (an angle axis on the protocol's grid, a step whose length the input sets, several players a run)
