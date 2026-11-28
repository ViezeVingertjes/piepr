"""Tests for web_pages.py. Run: python3 -m unittest discover -s firmware/scripts"""
import gzip
import json
import pathlib
import re
import shutil
import subprocess
import tempfile
import unittest

import web_pages

WEB = pathlib.Path(__file__).parent.parent / "web"


def site(pages, dutch, parts=None):
    """A temporary web/ folder with these pages, translations and shared parts."""
    folder = tempfile.TemporaryDirectory()
    root = pathlib.Path(folder.name)
    for name, text in pages.items():
        (root / (name + ".html")).write_text(text, encoding="utf-8")
    (root / "nl.json").write_text(json.dumps(dutch), encoding="utf-8")
    if parts:
        (root / "parts").mkdir()
        for name, text in parts.items():
            (root / "parts" / (name if "." in name else name + ".html")).write_text(text, encoding="utf-8")
    return folder


def html(pages, name, language):
    return gzip.decompress(pages[name][language]).decode("utf-8")


class WebPages(unittest.TestCase):
    def build(self, pages, dutch, parts=None):
        with site(pages, dutch, parts) as folder:
            return web_pages.build(folder)

    def test_english_keeps_the_text_and_dutch_gets_the_translation(self):
        pages = self.build({"a": "<p>[[Save]]</p>"}, {"Save": "Opslaan"})
        self.assertEqual(html(pages, "a", "en"), "<p>Save</p>")
        self.assertEqual(html(pages, "a", "nl"), "<p>Opslaan</p>")

    def test_indentation_and_blank_lines_are_dropped(self):
        pages = self.build({"a": "<main>\n\n   <p>x</p>\n</main>\n"}, {})
        self.assertEqual(html(pages, "a", "en"), "<main>\n<p>x</p>\n</main>")

    def test_each_page_knows_its_own_and_the_other_language(self):
        pages = self.build({"a": '<html lang="[[@lang]]"><b>[[@lang-name]]</b><a href="/?lang=[[@other]]">[[@other-name]]</a>'}, {})
        self.assertEqual(html(pages, "a", "en"), '<html lang="en"><b>English</b><a href="/?lang=nl">Nederlands</a>')
        self.assertEqual(html(pages, "a", "nl"), '<html lang="nl"><b>Nederlands</b><a href="/?lang=en">English</a>')

    def test_shared_parts_are_put_in_and_translated(self):
        pages = self.build({"a": "<main>\n  [[>foot]]\n</main>", "b": "<i>[[>foot]]</i>"},
                           {"Bye": "Dag"}, parts={"foot": "<p>[[Bye]]</p>\n<p>x</p>\n"})
        self.assertEqual(html(pages, "a", "nl"), "<main>\n<p>Dag</p>\n<p>x</p>\n</main>")
        self.assertEqual(html(pages, "b", "en"), "<i><p>Bye</p>\n<p>x</p></i>")

    def test_a_part_can_be_a_stylesheet_or_a_script(self):
        pages = self.build({"a": "<style>[[>base.css]]</style><script>[[>dom.js]]</script>"}, {},
                           parts={"base.css": "a { color: red }\n", "dom.js": "var a = 1;\n"})
        self.assertEqual(html(pages, "a", "en"), "<style>a{color:red}</style><script>var a=1;</script>")

    def test_a_part_can_use_a_part(self):
        pages = self.build({"a": "[[>head]]<p>x</p>"}, {"Hi": "Hoi"},
                           parts={"head": "<style>[[>base.css]]</style>\n[[>title]]\n", "base.css": "a { b: c }",
                                  "title": "<title>[[Hi]]</title>"})
        self.assertEqual(html(pages, "a", "nl"), "<style>a{b:c}</style>\n<title>Hoi</title><p>x</p>")

    def test_parts_may_go_as_deep_as_the_limit(self):
        depth = web_pages.PART_DEPTH
        parts = {"p%d" % n: "[[>p%d]]" % (n + 1) for n in range(1, depth)}
        parts["p%d" % depth] = "<p>x</p>"
        self.assertEqual(html(self.build({"a": "[[>p1]]"}, {}, parts=parts), "a", "en"), "<p>x</p>")

    def test_a_part_that_uses_itself_stops_the_build(self):
        with self.assertRaisesRegex(web_pages.PageError, "a.html.*parts within parts"):
            self.build({"a": "[[>loop]]"}, {}, parts={"loop": "<p>[[>loop]]</p>"})

    def test_comments_and_spare_whitespace_are_left_out(self):
        pages = self.build({"a": "<!-- top -->\n<script>\n  /* why */\n  var a = \"[[Save]]\"; // b\n</script>"},
                           {"Save": "Opslaan"})
        self.assertEqual(html(pages, "a", "nl"), '<script>var a="Opslaan";</script>')

    def test_a_script_that_cannot_be_read_stops_the_build(self):
        with self.assertRaisesRegex(web_pages.PageError, "a.html.*never ends"):
            self.build({"a": "<script>var a = \"x;\n</script>"}, {})

    def test_english_text_that_could_break_the_page_stops_the_build(self):
        for bad in ('say "hi"', "it's", "a<b", "a\\b"):
            with self.assertRaisesRegex(web_pages.PageError, "a.html", msg=bad):
                self.build({"a": "[[%s]]" % bad}, {bad: "x"})

    def test_a_missing_translation_stops_the_build(self):
        with self.assertRaisesRegex(web_pages.PageError, "no translation for 'Save'"):
            self.build({"a": "[[Save]]"}, {})

    def test_an_unused_translation_stops_the_build(self):
        with self.assertRaisesRegex(web_pages.PageError, "not used by any page.*'Old'"):
            self.build({"a": "[[Save]]"}, {"Save": "Opslaan", "Old": "Oud"})

    def test_a_translation_that_could_break_the_page_stops_the_build(self):
        for bad in ('say "hi"', "it's", "a<b", "a\\b", "two\nlines", "a]]b"):
            with self.assertRaises(web_pages.PageError, msg=bad):
                self.build({"a": "[[Save]]"}, {"Save": bad})

    def test_a_missing_part_stops_the_build(self):
        with self.assertRaisesRegex(web_pages.PageError, "parts/foot.html"):
            self.build({"a": "[[>foot]]"}, {})

    def test_output_is_the_same_every_time(self):
        with site({"a": "<p>[[Save]]</p>"}, {"Save": "Opslaan"}) as folder:
            self.assertEqual(web_pages.build(folder), web_pages.build(folder))

    def test_the_real_pages_build_in_both_languages(self):
        pages = web_pages.build(WEB)
        self.assertEqual(sorted(pages), ["invalid", "saved", "setup", "status"])
        for name in pages:
            self.assertIn('<html lang="en">', html(pages, name, "en"))
            self.assertIn('<html lang="nl">', html(pages, name, "nl"))
            for language in ("en", "nl"):
                self.assertNotIn("[[", html(pages, name, language))
                self.assertNotIn("<!--", html(pages, name, language))
                self.assertNotIn("/*", html(pages, name, language))

    @unittest.skipUnless(shutil.which("node"), "needs Node.js")
    def test_the_scripts_in_the_real_pages_are_valid_javascript(self):
        pages = web_pages.build(WEB)
        with tempfile.TemporaryDirectory() as folder:
            for name in pages:
                for language in ("en", "nl"):
                    for n, script in enumerate(re.findall(r"<script>(.*?)</script>", html(pages, name, language), re.S)):
                        path = pathlib.Path(folder) / ("%s-%s-%d.js" % (name, language, n))
                        path.write_text(script, encoding="utf-8")
                        result = subprocess.run(["node", "--check", str(path)], capture_output=True, text=True)
                        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
