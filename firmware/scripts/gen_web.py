# PlatformIO pre-build script: web/*.html -> web/gz/*.html.gz. The firmware
# embeds these and serves them gzip-encoded (about a third of the size).
# Indentation and blank lines are dropped first; the pages contain no <pre> and
# no multi-line strings, so that is safe.
import glob
import gzip
import os
import sys

Import("env")  # noqa: F821  (provided by PlatformIO)
sys.path.insert(0, env.subst("$PROJECT_DIR/scripts"))  # noqa: F821
from gen_common import write_if_changed  # noqa: E402

WEB = os.path.join(env.subst("$PROJECT_DIR"), "web")  # noqa: F821
GZ = os.path.join(WEB, "gz")
os.makedirs(GZ, exist_ok=True)
pages = glob.glob(os.path.join(WEB, "*.html"))
for stale in set(glob.glob(os.path.join(GZ, "*.gz"))) - {os.path.join(GZ, os.path.basename(p) + ".gz") for p in pages}:
    os.remove(stale)
for page in pages:
    with open(page, encoding="utf-8") as f:
        text = "\n".join(line.strip() for line in f if line.strip())
    packed = gzip.compress(text.encode("utf-8"), compresslevel=9, mtime=0)
    write_if_changed(os.path.join(GZ, os.path.basename(page) + ".gz"), packed)
