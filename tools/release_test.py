#!/usr/bin/env python3
"""Unit tests for tools/release.py. Run: python3 -m unittest tools/release_test.py"""

import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import release as r  # noqa: E402


def frag(type_, breaking=False, title=None, body="Summary."):
    return {"file": "x.md", "type": type_, "breaking": breaking, "title": title, "body": body}


class ParseTest(unittest.TestCase):
    def test_valid(self):
        f = r.parse_fragment("a.md", "---\ntype: Fixed # note\ntitle: T\n---\n\nBody line.\n")
        self.assertEqual((f["type"], f["title"], f["breaking"], f["body"]), ("Fixed", "T", False, "Body line."))

    def test_rejects(self):
        for text in [
            "no front matter",
            "---\ntype: Nope\n---\nBody",
            "---\ntype: Added\n---\n\n",
            "---\ntype: Added\nbreaking: yes\n---\nBody",
            "---\ntype: Added\ndoc: x\n---\nBody",
        ]:
            with self.assertRaises(r.FragmentError, msg=text):
                r.parse_fragment("a.md", text)

    def test_repo_fragments_valid(self):
        r.read_fragments()


class BumpTest(unittest.TestCase):
    def test_levels(self):
        self.assertEqual(r.bump_level([frag("Fixed")]), "patch")
        self.assertEqual(r.bump_level([frag("Fixed"), frag("Added")]), "minor")
        self.assertEqual(r.bump_level([frag("Fixed", breaking=True)]), "major")

    def test_next_version(self):
        self.assertEqual(r.next_version("1.2.3", "patch"), "1.2.4")
        self.assertEqual(r.next_version("1.2.3", "minor"), "1.3.0")
        self.assertEqual(r.next_version("1.2.3", "major"), "2.0.0")
        self.assertEqual(r.next_version("0.4.1", "major"), "0.5.0")  # 0.x: breaking is a minor


class ChangelogTest(unittest.TestCase):
    def test_round_trip(self):
        base = "# Changelog\n\n## [Unreleased]\n\n## [0.1.0] - 2026-01-01\n\n### Fixed\n\n- Old.\n"
        section = r.render_section("0.2.0", "2026-02-02", [frag("Fixed", body="B."), frag("Added", title="T", body="A.\nmore")])
        out = r.insert_section(base, section)
        self.assertIn("## [Unreleased]\n\n## [0.2.0] - 2026-02-02\n\n### Added\n\n- **T** — A.\n  more\n\n### Fixed\n\n- B.\n", out)
        self.assertEqual(r.extract_section(out, "0.2.0"), "### Added\n\n- **T** — A.\n  more\n\n### Fixed\n\n- B.\n")
        self.assertEqual(r.extract_section(out, "0.1.0"), "### Fixed\n\n- Old.\n")

    def test_missing_anchor(self):
        with self.assertRaises(ValueError):
            r.insert_section("# Changelog\n", "x")


class CollateTest(unittest.TestCase):
    def test_collate_writes_and_consumes(self):
        with tempfile.TemporaryDirectory() as d:
            d = pathlib.Path(d)
            (d / "frags").mkdir()
            (d / "frags" / "1-a.md").write_text("---\ntype: Added\n---\nNew thing.\n")
            (d / "frags" / "README.md").write_text("not a fragment")
            (d / "CHANGELOG.md").write_text("# Changelog\n\n## [Unreleased]\n")
            (d / "VERSION").write_text("0.0.0\n")
            saved = (r.FRAG_DIR, r.CHANGELOG, r.VERSION_FILE)
            r.FRAG_DIR, r.CHANGELOG, r.VERSION_FILE = d / "frags", d / "CHANGELOG.md", d / "VERSION"
            try:
                self.assertEqual(r.main(["collate", "0.1.0"]), 0)
            finally:
                r.FRAG_DIR, r.CHANGELOG, r.VERSION_FILE = saved
            self.assertEqual((d / "VERSION").read_text(), "0.1.0\n")
            self.assertIn("### Added\n\n- New thing.", (d / "CHANGELOG.md").read_text())
            self.assertEqual(sorted(p.name for p in (d / "frags").iterdir()), ["README.md"])


if __name__ == "__main__":
    unittest.main()
