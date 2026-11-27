# PlatformIO pre-build script: web/*.html -> web/gz/<page>.html.gz (English) and
# web/gz/<page>.nl.html.gz (Dutch). The firmware embeds these and serves them
# gzip-encoded (about a third of the size).
#
# Text to translate is written in the pages as [[English text]]; the Dutch for
# it comes from web/nl.json. The build fails on a missing or unused entry.
# Indentation and blank lines are dropped; the pages contain no <pre> and no
# multi-line strings, so that is safe.
import glob
import gzip
import json
import os
import re
import sys

Import("env")  # noqa: F821  (provided by PlatformIO)
sys.path.insert(0, env.subst("$PROJECT_DIR/scripts"))  # noqa: F821
from gen_common import write_if_changed  # noqa: E402

WEB = os.path.join(env.subst("$PROJECT_DIR"), "web")  # noqa: F821
GZ = os.path.join(WEB, "gz")
MARKED = re.compile(r"\[\[(.+?)\]\]")
# Translations land inside HTML text, attributes and JavaScript strings alike.
NOT_IN_A_TRANSLATION = ('"', "\\", "<", ">", "[[", "]]", "\n")

with open(os.path.join(WEB, "nl.json"), encoding="utf-8") as f:
    dutch = json.load(f)
for english, translation in dutch.items():
    for bad in NOT_IN_A_TRANSLATION:
        if bad in translation:
            raise SystemExit("web/nl.json: %r may not contain %r" % (translation, bad))
used = set()


def in_dutch(match):
    english = match.group(1)
    if english not in dutch:
        raise SystemExit("web/nl.json: no translation for %r" % english)
    used.add(english)
    return dutch[english]


os.makedirs(GZ, exist_ok=True)
written = set()
for page in sorted(glob.glob(os.path.join(WEB, "*.html"))):
    with open(page, encoding="utf-8") as f:
        text = "\n".join(line.strip() for line in f if line.strip())
    name = os.path.basename(page)[: -len(".html")]
    versions = {
        name + ".html.gz": MARKED.sub(lambda m: m.group(1), text),
        name + ".nl.html.gz": MARKED.sub(in_dutch, text),
    }
    for filename, content in versions.items():
        packed = gzip.compress(content.encode("utf-8"), compresslevel=9, mtime=0)
        write_if_changed(os.path.join(GZ, filename), packed)
        written.add(os.path.join(GZ, filename))

unused = sorted(set(dutch) - used)
if unused:
    raise SystemExit("web/nl.json: not used by any page: %r" % unused)
for stale in set(glob.glob(os.path.join(GZ, "*.gz"))) - written:
    os.remove(stale)
