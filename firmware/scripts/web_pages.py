# The setup pages, built from web/: every page in English and in Dutch, with
# indentation and blank lines dropped and gzipped (about a third of the size).
# Plain Python, used by gen_web.py and tested in test_web_pages.py.
#
# In a page:
#   [[English text]]   text to translate; the Dutch for it comes from web/nl.json
#   [[>name]]          the shared piece web/parts/name.html
#   [[@lang]]          this page's language ("en" or "nl")
#   [[@other]]         the other language, and [[@other-name]] its name
# The pages contain no <pre> and no multi-line strings, so dropping indentation
# is safe.
import glob
import gzip
import json
import os
import re

# language: (the other language, the other language's own name)
LANGUAGES = {"en": ("nl", "Nederlands"), "nl": ("en", "English")}
PART = re.compile(r"\[\[>([a-z0-9-]+)\]\]")
MARKED = re.compile(r"\[\[(.+?)\]\]")
# Translations land inside HTML text, attributes and JavaScript strings alike.
NOT_IN_A_TRANSLATION = ('"', "'", "\\", "<", ">", "[[", "]]", "\n")


class PageError(Exception):
    """Something in web/ that has to be fixed before the pages can be built."""


def _read(path):
    try:
        with open(path, encoding="utf-8") as f:
            return f.read()
    except FileNotFoundError:
        raise PageError("%s is missing" % path) from None


def _minified(text):
    return "\n".join(line.strip() for line in text.splitlines() if line.strip())


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
        return _minified(_read(os.path.join(web_dir, "parts", match.group(1) + ".html")))

    def in_language(language):
        other, other_name = LANGUAGES[language]
        fixed = {"@lang": language, "@other": other, "@other-name": other_name}

        def swap(match):
            text = match.group(1)
            if text in fixed:
                return fixed[text]
            if language == "en":
                return text
            if text not in dutch:
                raise PageError("web/nl.json: no translation for %r" % text)
            used.add(text)
            return dutch[text]

        return swap

    pages = {}
    for path in sorted(glob.glob(os.path.join(web_dir, "*.html"))):
        text = PART.sub(part, _minified(_read(path)))
        name = os.path.basename(path)[: -len(".html")]
        pages[name] = {
            language: gzip.compress(
                MARKED.sub(in_language(language), text).encode("utf-8"), compresslevel=9, mtime=0
            )
            for language in LANGUAGES
        }

    unused = sorted(set(dutch) - used)
    if unused:
        raise PageError("web/nl.json: not used by any page: %r" % unused)
    return pages
