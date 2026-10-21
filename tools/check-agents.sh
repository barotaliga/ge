#!/usr/bin/env bash
# Holds the agent guidance's STRUCTURE (CI's `agents` job, `make agents-check`):
#   - every tool-specific guidance file is a symlink onto AGENTS.md, and every
#     tool skills dir a symlink onto .agents/skills — one source, no drift;
#   - every skill dir has a SKILL.md whose front-matter `name` is its dir;
#   - every skill is routed from AGENTS.md and listed in .agents/skills/README.md,
#     and nothing is routed that does not exist;
#   - no source file passes the 1000-line cap;
#   - every source file carries its ge: tag block and docs/index.md is fresh
#     (tools/agents-index.mjs), and every lesson fragment is well formed
#     (tools/skill-lessons.mjs);
#   - every plugin keeps the plugin contract (tools/check-plugins.mjs).
#
# ge:summary The agent guidance's structure check (make agents-check)
# ge:keywords ci check agents symlinks skills router line-cap
# ge:skill write-code
set -euo pipefail
cd "$(dirname "$0")/.."
fail=0
err() { echo "agents-check: $*" >&2; fail=1; }

link() {  # path, exact target
  if [[ ! -L "$1" ]]; then err "$1 must be a symlink to $2"; return; fi
  [[ "$(readlink "$1")" == "$2" ]] || err "$1 -> $(readlink "$1"), want $2"
}
link CLAUDE.md AGENTS.md
link GEMINI.md AGENTS.md
link .cursorrules AGENTS.md
link .github/copilot-instructions.md ../AGENTS.md
link .claude/skills ../.agents/skills
link .gemini/skills ../.agents/skills

skills=()
for dir in .agents/skills/*/; do
  name="$(basename "$dir")"
  skills+=("$name")
  file="$dir/SKILL.md"
  [[ -f "$file" ]] || { err "$file missing"; continue; }
  got="$(sed -n 's/^name: *//p' "$file" | head -1)"
  [[ "$got" == "$name" ]] || err "$file: name '$got' != dir '$name'"
  grep -q '^description: ' "$file" || err "$file: no description"
  [[ -d "$dir/.lessons" ]] || err "$dir.lessons/ missing"
  grep -q "\`$name\`" AGENTS.md || err "skill '$name' is not routed from AGENTS.md"
  grep -q "\`$name\`" .agents/skills/README.md || err "skill '$name' is not in .agents/skills/README.md"
done

# Every skill named in AGENTS.md's Skills table exists.
while read -r name; do
  [[ -d ".agents/skills/$name" ]] || err "AGENTS.md routes '$name' but .agents/skills/$name does not exist"
done < <(sed -n '/^## Skills/,/^## /p' AGENTS.md | grep -o '^| `[a-z0-9-]*`' | tr -d '|` ')

while read -r f; do
  n=$(wc -l < "$f")
  if (( n > 1000 )) && ! grep -q 'agents:allow-large-file' "$f"; then
    err "$f has $n lines (cap 1000)"
  fi
done < <(git ls-files --cached --others --exclude-standard '*.cpp' '*.hpp' '*.h' '*.mjs' '*.sh' '*.cmake' 'CMakeLists.txt')

node tools/agents-index.mjs --check || fail=1
node tools/check-plugins.mjs || fail=1
node tools/skill-lessons.mjs --check || fail=1

(( fail == 0 )) && echo "agents-check: ok (${#skills[@]} skills)"
exit $fail
