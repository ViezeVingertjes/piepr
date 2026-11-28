# The pages of web/, built for the firmware: every page in English and in Dutch,
# minified (web_minify.py) and gzipped. Plain Python, used by gen_web.py and
# tested in test_web_pages.py.
#
# In a page:
#   [[English text]]   text to translate; the Dutch for it comes from web/nl.json
#   [[>name]]          the shared piece web/parts/name.html; [[>name.css]] and
#                      [[>name.js]] are web/parts/name.css and name.js. A part
#                      may use other parts
#   [[@lang]]          this page's language ("en" or "nl"), and [[@lang-name]] its name
#   [[@other]]         the other language, and [[@other-name]] its name
# Write pages to be read: indentation, blank lines and comments are left out of
# what the device serves. The pages contain no <pre>.
import glob
import gzip
import json
import os
import re

from web_minify import minify_html

# language: its own name
LANGUAGES = {"en": "English", "nl": "Nederlands"}
PART = re.compile(r"\[\[>([a-z0-9-]+(?:\.(?:css|js))?)\]\]")
PART_DEPTH = 8  # parts within parts; more than this is a part that uses itself
MARKED = re.compile(r"\[\[(.+?)\]\]")
# Marked text lands inside HTML text, attributes and JavaScript strings alike.
NOT_IN_MARKED_TEXT = ('"', "'", "\\", "<", ">", "\n")
NOT_IN_A_TRANSLATION = NOT_IN_MARKED_TEXT + ("[[", "]]")


class PageError(Exception):
    """Something in web/ that has to be fixed before the pages can be built."""


def _read(path):
    try:
        with open(path, encoding="utf-8") as f:
            return f.read()
    except FileNotFoundError:
        raise PageError("%s is missing" % path) from None


def build(web_dir):
    """All pages of `web_dir`: {page name: {"en": gzipped bytes, "nl": gzipped bytes}}."""
    web_dir = os.fspath(web_dir)
    dutch = json.loads(_read(os.path.join(web_dir, "nl.json")))
    for translation in dutch.values():
        for bad in NOT_IN_A_TRANSLATION:
            if bad in translation:
                raise PageError("web/nl.json: %r may not contain %r" % (translation, bad))
    used = set()

    def part(match):
        name = match.group(1)
        return _read(os.path.join(web_dir, "parts", name if "." in name else name + ".html")).strip()

    def with_parts(page, text):
        for _ in range(PART_DEPTH):
            text = PART.sub(part, text)
            if not PART.search(text):
                return text
        raise PageError("web/%s: more than %d parts within parts" % (page, PART_DEPTH))

    def in_language(page, language):
        other = next(name for name in LANGUAGES if name != language)
        fixed = {"@lang": language, "@lang-name": LANGUAGES[language], "@other": other, "@other-name": LANGUAGES[other]}

        def swap(match):
            text = match.group(1)
            if text in fixed:
                return fixed[text]
            for bad in NOT_IN_MARKED_TEXT:
                if bad in text:
                    raise PageError("web/%s: [[%s]] may not contain %r" % (page, text, bad))
            if language == "en":
                return text
            if text not in dutch:
                raise PageError("web/nl.json: no translation for %r" % text)
            used.add(text)
            return dutch[text]

        return swap

    pages = {}
    for path in sorted(glob.glob(os.path.join(web_dir, "*.html"))):
        page = os.path.basename(path)
        text = with_parts(page, _read(path))
        versions = pages[page[: -len(".html")]] = {}
        for language in LANGUAGES:
            try:
                small = minify_html(MARKED.sub(in_language(page, language), text))
            except ValueError as error:
                raise PageError("web/%s: %s" % (page, error)) from None
            versions[language] = gzip.compress(small.encode("utf-8"), compresslevel=9, mtime=0)

    unused = sorted(set(dutch) - used)
    if unused:
        raise PageError("web/nl.json: not used by any page: %r" % unused)
    return pages
