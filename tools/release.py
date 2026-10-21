#!/usr/bin/env python3
"""Release plumbing: changeset fragments -> semver bump -> CHANGELOG section.

The release workflow (.github/workflows/release.yml) runs these commands;
agents run them locally to preview a release. Standard library only.

  python3 tools/release.py check            validate every fragment
  python3 tools/release.py bump [--auto|patch|minor|major]
                                            print the next version (X.Y.Z)
  python3 tools/release.py collate X.Y.Z    write CHANGELOG section, delete fragments
  python3 tools/release.py notes X.Y.Z      print that version's CHANGELOG section

Fragment: .changes/unreleased/<unix-ts>-<slug>.md

  ---
  type: Added        # Added | Changed | Deprecated | Removed | Fixed | Security
  title: Short title # optional, bolded at the head of the bullet
  breaking: true     # optional, forces a major bump
  ---

  One-sentence summary of the change, for a consumer of the engine.

Bump policy: breaking -> major; Added/Changed/Deprecated/Removed -> minor;
Fixed/Security -> patch. The release takes the highest level of any fragment.
While the major version is 0, a breaking change bumps the minor instead
(semver 0.x: anything may change), so 0.x never jumps to 1.0 by accident.
The current version is the VERSION file at the repository root.
"""

from __future__ import annotations

import datetime
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
FRAG_DIR = ROOT / ".changes" / "unreleased"
CHANGELOG = ROOT / "CHANGELOG.md"
VERSION_FILE = ROOT / "VERSION"

TYPES = ["Added", "Changed", "Deprecated", "Removed", "Fixed", "Security"]
TYPE_LEVEL = {
    "Added": "minor",
    "Changed": "minor",
    "Deprecated": "minor",
    "Removed": "minor",
    "Fixed": "patch",
    "Security": "patch",
}
LEVELS = ["patch", "minor", "major"]
SEMVER = re.compile(r"^(\d+)\.(\d+)\.(\d+)$")
FRONT = re.compile(r"^---\n(.*?)\n---\n(.*)$", re.S)


class FragmentError(ValueError):
    pass


def parse_fragment(name: str, text: str) -> dict:
    """Parse one fragment's text. Raises FragmentError on anything malformed."""
    m = FRONT.match(text.replace("\r\n", "\n"))
    if not m:
        raise FragmentError(f"{name}: missing '---' front matter")
    front = {}
    for line in m.group(1).splitlines():
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        if ":" not in line:
            raise FragmentError(f"{name}: bad front-matter line {line!r}")
        key, value = (s.strip() for s in line.split(":", 1))
        front[key] = value
    unknown = set(front) - {"type", "title", "breaking"}
    if unknown:
        raise FragmentError(f"{name}: unknown front-matter keys {sorted(unknown)}")
    if front.get("type") not in TYPES:
        raise FragmentError(f"{name}: type must be one of {TYPES}, got {front.get('type')!r}")
    if front.get("breaking", "false") not in ("true", "false"):
        raise FragmentError(f"{name}: breaking must be true or false")
    body = m.group(2).strip()
    if not body:
        raise FragmentError(f"{name}: empty body")
    return {
        "file": name,
        "type": front["type"],
        "title": front.get("title"),
        "breaking": front.get("breaking") == "true",
        "body": body,
    }


def read_fragments(frag_dir: pathlib.Path | None = None) -> list[dict]:
    files = sorted(p for p in (frag_dir or FRAG_DIR).glob("*.md") if p.name != "README.md")
    return [parse_fragment(p.name, p.read_text(encoding="utf-8")) for p in files]


def bump_level(fragments: list[dict]) -> str:
    rank = 0
    for f in fragments:
        level = "major" if f["breaking"] else TYPE_LEVEL[f["type"]]
        rank = max(rank, LEVELS.index(level))
    return LEVELS[rank]


def next_version(current: str, level: str) -> str:
    m = SEMVER.match(current)
    if not m:
        raise ValueError(f"not a semver version: {current!r}")
    major, minor, patch = (int(x) for x in m.groups())
    if level == "major" and major == 0:
        level = "minor"
    if level == "major":
        return f"{major + 1}.0.0"
    if level == "minor":
        return f"{major}.{minor + 1}.0"
    return f"{major}.{minor}.{patch + 1}"


def render_section(version: str, date: str, fragments: list[dict]) -> str:
    lines = [f"## [{version}] - {date}", ""]
    for t in TYPES:
        bullets = []
        for f in (f for f in fragments if f["type"] == t):
            body = f["body"].split("\n")
            title = f"**{f['title']}** — " if f["title"] else ""
            flag = "**BREAKING** " if f["breaking"] else ""
            bullet = f"- {flag}{title}{body[0]}"
            bullet += "".join(f"\n  {line}" if line else "\n" for line in body[1:])
            bullets.append(bullet)
        if bullets:
            lines += [f"### {t}", "", *bullets, ""]
    return "\n".join(lines).rstrip() + "\n"


def insert_section(changelog: str, section: str) -> str:
    out, n = re.subn(r"## \[Unreleased\]\s*\n", f"## [Unreleased]\n\n{section}\n", changelog, count=1)
    if n != 1:
        raise ValueError("CHANGELOG.md is missing the '## [Unreleased]' anchor")
    return out


def extract_section(changelog: str, version: str) -> str:
    m = re.search(rf"^## \[{re.escape(version)}\][^\n]*\n(.*?)(?=^## \[|\Z)", changelog, re.S | re.M)
    if not m:
        raise ValueError(f"no section for {version} in CHANGELOG.md")
    return m.group(1).strip() + "\n"


def main(argv: list[str]) -> int:
    if not argv:
        print(__doc__, file=sys.stderr)
        return 2
    cmd, args = argv[0], argv[1:]
    try:
        if cmd == "check":
            frags = read_fragments()
            for f in frags:
                print(f"{f['file']}: {f['type']}{' (breaking)' if f['breaking'] else ''}", file=sys.stderr)
            print(f"{len(frags)} fragment(s) valid", file=sys.stderr)
            return 0
        if cmd == "bump":
            current = VERSION_FILE.read_text(encoding="utf-8").strip()
            level = args[0] if args else "--auto"
            if level == "--auto":
                frags = read_fragments()
                if not frags:
                    print("no fragments in .changes/unreleased/ — nothing to release", file=sys.stderr)
                    return 1
                level = bump_level(frags)
            if level not in LEVELS:
                print(f"bump must be --auto or one of {LEVELS}", file=sys.stderr)
                return 2
            print(next_version(current, level))
            return 0
        if cmd in ("collate", "notes"):
            if len(args) != 1 or not SEMVER.match(args[0]):
                print(f"usage: release.py {cmd} X.Y.Z", file=sys.stderr)
                return 2
            version = args[0]
            if cmd == "notes":
                sys.stdout.write(extract_section(CHANGELOG.read_text(encoding="utf-8"), version))
                return 0
            frags = read_fragments()
            if not frags:
                print("no fragments in .changes/unreleased/ — refusing an empty release", file=sys.stderr)
                return 1
            date = datetime.datetime.now(datetime.timezone.utc).date().isoformat()
            section = render_section(version, date, frags)
            CHANGELOG.write_text(insert_section(CHANGELOG.read_text(encoding="utf-8"), section), encoding="utf-8")
            for f in frags:
                (FRAG_DIR / f["file"]).unlink()
            VERSION_FILE.write_text(version + "\n", encoding="utf-8")
            print(f"wrote {version} from {len(frags)} fragment(s)", file=sys.stderr)
            return 0
    except (FragmentError, ValueError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 1
    print(f"unknown command {cmd!r}", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
